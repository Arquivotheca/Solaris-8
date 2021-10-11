/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rts.c	1.35	99/10/21 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <sys/proc.h>
#include <sys/suntpi.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <inet/common.h>
#include <netinet/ip6.h>
#include <inet/ip.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/optcom.h>
#include <netinet/ip_mroute.h>
#include <sys/isa_defs.h>
#include <net/route.h>

/*
 * This is a transport provider for routing sockets.  Downstream messages are
 * wrapped with a IP_IOCTL header, and ip_wput_ioctl calls the appropriate entry
 * in the ip_ioctl_ftbl callout table to pass the routing socket data into IP.
 * Upstream messages are generated for listeners of the routing socket as well
 * as the message sender (unless they have turned off their end using
 * SO_USELOOPBACK or shutdown(3n)).  Upstream messages may also be generated
 * asynchronously when:
 *
 *	Interfaces are brought up or down.
 *	Addresses are assigned to interfaces.
 *	ICMP redirects are processed and a IRE_HOST_REDIRECT is installed.
 *	No route is found while sending a packet.
 *	When TCP requests IP to remove an IRE_CACHE of a troubled destination.
 *
 * Synchronization notes:
 *
 * At all points in this code
 * where exclusive, writer, access is required, we pass a message to a
 * subroutine by invoking "become_writer" which will arrange to call the
 * routine only after all reader threads have exited the shared resource, and
 * the writer lock has been acquired. For uniprocessor, single-thread,
 * nonpreemptive environments, become_writer can simply be a macro which
 * invokes the routine immediately.
 */

/*
 * Object to represent database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 * XXX. These and other externs should really move to a rts header.
 */
extern optdb_obj_t	rts_opt_obj;
extern uint_t		rts_max_optbuf_len;

/* Internal routing socket stream control structure, one per open stream */
typedef	struct rts_s {
	uint_t	rts_state;		/* Provider interface state */
	uint_t	rts_error;		/* Routing socket error code */
	uint_t	rts_flag;		/* Pending I/O state */
	uint_t	rts_proto;		/* SO_PROTOTYPE "socket" option. */
	uint_t	rts_priv_stream : 1,	/* Stream opened by privileged user. */
		rts_debug : 1,		/* SO_DEBUG "socket" option. */
		rts_dontroute : 1,	/* SO_DONTROUTE "socket" option. */
		rts_broadcast : 1,	/* SO_BROADCAST "socket" option. */

		rts_reuseaddr : 1,	/* SO_REUSEADDR "socket" option. */
		rts_useloopback : 1,	/* SO_USELOOPBACK "socket" option. */
		rts_multicast_loop : 1,	/* IP_MULTICAST_LOOP option */
		rts_hdrincl : 1,	/* IP_HDRINCL option + RAW and IGMP */

		: 0;
} rts_t;

#define	RTS_WPUT_PENDING	0x1	/* Waiting for write-side to complete */
#define	RTS_WRW_PENDING		0x2	/* Routing socket write in progress */

/* Default structure copied into T_INFO_ACK messages */
static struct T_info_ack rts_g_t_info_ack = {
	T_INFO_ACK,
	T_INFINITE,	/* TSDU_size. Maximum size messages. */
	T_INVALID,	/* ETSDU_size. No expedited data. */
	T_INVALID,	/* CDATA_size. No connect data. */
	T_INVALID,	/* DDATA_size. No disconnect data. */
	0,		/* ADDR_size. */
	0,		/* OPT_size - not initialized here */
	64 * 1024,	/* TIDU_size. rts allows maximum size messages. */
	T_COTS,		/* SERV_type. rts supports connection oriented. */
	TS_UNBND,	/* CURRENT_state. This is set from rts_state. */
	(XPG4_1)	/* PROVIDER_flag */
};

/* Named Dispatch Parameter Management Structure */
typedef struct rtspparam_s {
	uint_t	rts_param_min;
	uint_t	rts_param_max;
	uint_t	rts_param_value;
	char	*rts_param_name;
} rtsparam_t;

/*
 * Table of ND variables supported by rts. These are loaded into rts_g_nd
 * in rts_open.
 * All of these are alterable, within the min/max values given, at run time.
 */
static rtsparam_t	rts_param_arr[] = {
	/* min		max		value		name */
	{ 4096,		65536,		8192,		"rts_xmit_hiwat"},
	{ 0,		65536,		1024,		"rts_xmit_lowat"},
	{ 4096,		65536,		8192,		"rts_recv_hiwat"},
	{ 65536,	1024*1024*1024, 256*1024,	"rts_max_buf"},
};
#define	rts_xmit_hiwat			rts_param_arr[0].rts_param_value
#define	rts_xmit_lowat			rts_param_arr[1].rts_param_value
#define	rts_recv_hiwat			rts_param_arr[2].rts_param_value
#define	rts_max_buf			rts_param_arr[3].rts_param_value

static int	rts_close(queue_t *q);
static void 	rts_err_ack(queue_t *q, mblk_t *mp, t_scalar_t t_error,
    int sys_error);
static mblk_t	*rts_ioctl_alloc(mblk_t *data);
static int	rts_open(queue_t *q, dev_t *devp, int flag, int sflag,
    cred_t *credp);
int		rts_opt_default(queue_t *q, t_scalar_t level, t_scalar_t name,
    uchar_t *ptr);
int		rts_opt_get(queue_t *q, t_scalar_t level, t_scalar_t name,
    uchar_t *ptr);
int		rts_opt_set(queue_t *q, uint_t optset_context, t_scalar_t level,
    t_scalar_t name, uint_t inlen, uchar_t *invalp, uint_t *outlenp,
    uchar_t *outvalp, uchar_t *thisdg_attrs);
static void	rts_param_cleanup(void);
static int	rts_param_get(queue_t *q, mblk_t *mp, caddr_t cp);
static boolean_t rts_param_register(rtsparam_t *rtspa, int cnt);
static int	rts_param_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp);
static void	rts_rput(queue_t *q, mblk_t *mp);
static void	rts_wput(queue_t *q, mblk_t *mp);
static void	rts_wput_iocdata(queue_t *q, mblk_t *mp);
static void 	rts_wput_other(queue_t *q, mblk_t *mp);
static int	rts_wrw(queue_t *q, struiod_t *dp);

static struct module_info info = {
	129, "rts", 1, INFPSZ, 512, 128
};

static struct qinit rinit = {
	(pfi_t)rts_rput, NULL, rts_open, rts_close, NULL, &info
};

static struct qinit winit = {
	(pfi_t)rts_wput, NULL, NULL, NULL, NULL, &info,
	NULL, (pfi_t)rts_wrw, NULL, STRUIOT_STANDARD
};

struct streamtab rtsinfo = {
	&rinit, &winit
};

static IDP	rts_g_nd;	/* Points to table of RTS ND variables. */
uint_t		rts_open_streams = 0;

/*
 * This routine allocates the necessary
 * message blocks for IOCTL wrapping the
 * user data.
 */
static mblk_t *
rts_ioctl_alloc(mblk_t *data)
{
	mblk_t	*mp = NULL;
	mblk_t	*mp1 = NULL;
	ipllc_t	*ipllc;
	struct iocblk	*ioc;

	mp = allocb(sizeof (ipllc_t), BPRI_MED);
	if (mp == NULL)
		return (NULL);
	mp1 = allocb(sizeof (struct iocblk), BPRI_MED);
	if (mp1 == NULL) {
		freemsg(mp);
		return (NULL);
	}
	ipllc = (ipllc_t *)mp->b_rptr;
	ipllc->ipllc_cmd = IP_IOC_RTS_REQUEST;
	ipllc->ipllc_name_offset = 0;
	ipllc->ipllc_name_length = 0;
	mp->b_wptr = (uchar_t *)(mp->b_rptr + sizeof (ipllc_t));
	if (data != NULL)
		linkb(mp, data);

	ioc = (struct iocblk *)mp1->b_rptr;
	ioc->ioc_cmd = IP_IOCTL;
	ioc->ioc_error = 0;
	ioc->ioc_cr = NULL;
	ioc->ioc_count = msgdsize(mp);
	mp1->b_wptr = (uchar_t *)(mp1->b_rptr + sizeof (struct iocblk));
	mp1->b_datap->db_type = M_IOCTL;

	linkb(mp1, mp);
	return (mp1);
}

/*
 * This routine closes rts stream, by disabling
 * put/srv routines and freeing the this module
 * internal datastructure.
 */
static int
rts_close(queue_t *q)
{
	qprocsoff(q);
	mi_free(q->q_ptr);
	rts_open_streams--;
	/*
	 * Free the ND table if this was
	 * the last stream close
	 */
	rts_param_cleanup();
	return (0);
}

/*
 * This is the open routine for routing socket. It allocates
 * rts_t structure for the stream and sends an IOCTL to
 * the down module to indicate that it is a routing socket
 * stream.
 */
/* ARGSUSED */
static int
rts_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	mblk_t	*mp = NULL;
	boolean_t	priv = drv_priv(credp) == 0;
	rts_t	*rts;

	/* If the stream is already open, return immediately. */
	if ((rts = (rts_t *)q->q_ptr) != NULL) {
		if (rts->rts_priv_stream && !priv)
			return (EPERM);
		return (0);
	}
	/* If this is not a push of rts as a module, fail. */
	if (sflag != MODOPEN)
		return (EINVAL);

	/* If this is the first open of rts, create the ND table. */
	if (rts_g_nd == NULL) {
		if (!rts_param_register(rts_param_arr, A_CNT(rts_param_arr)))
			return (ENOMEM);
	}
	q->q_ptr = mi_zalloc_sleep(sizeof (rts_t));
	WR(q)->q_ptr = q->q_ptr;
	rts = (rts_t *)q->q_ptr;
	if (priv)
		rts->rts_priv_stream = 1;
	/*
	 * The receive hiwat is only looked at on the stream head queue.
	 * Store in q_hiwat in order to return on SO_RCVBUF getsockopts.
	 */
	q->q_hiwat = rts_recv_hiwat;
	/*
	 * The transmit hiwat/lowat is only looked at on IP's queue.
	 * Store in q_hiwat/q_lowat in order to return on SO_SNDBUF/SO_SNDLOWAT
	 * getsockopts.
	 */
	WR(q)->q_hiwat = rts_xmit_hiwat;
	WR(q)->q_lowat = rts_xmit_lowat;
	qprocson(q);
	/*
	 * Indicate the down IP module that this is
	 * a routing socket client by sending an RTS IOCTL
	 * without any user data.
	 */
	mp = rts_ioctl_alloc(NULL);
	if (mp == NULL) {
		rts_param_cleanup();
		qprocsoff(q);
		return (ENOMEM);
	}
	rts_open_streams++;
	putnext(WR(q), mp);
	rts->rts_state = TS_UNBND;
	return (0);
}

/*
 * This routine creates a T_ERROR_ACK message and passes it upstream.
 */
static void
rts_err_ack(queue_t *q, mblk_t *mp, t_scalar_t t_error, int sys_error)
{
	if ((mp = mi_tpi_err_ack_alloc(mp, t_error, sys_error)) != NULL)
		qreply(q, mp);
}

/*
 * This routine creates a T_OK_ACK message and passes it upstream.
 */
static void
rts_ok_ack(queue_t *q, mblk_t *mp)
{
	if ((mp = mi_tpi_ok_ack_alloc(mp)) != NULL)
		qreply(q, mp);
}

/*
 * This routine is called by rts_wput to handle T_UNBIND_REQ messages.
 * After some error checking, the message is passed downstream to ip.
 */
static void
rts_unbind(queue_t *q, mblk_t *mp)
{
	rts_t	*rts;

	rts = (rts_t *)q->q_ptr;
	/* If a bind has not been done, we can't unbind. */
	if (rts->rts_state != TS_IDLE) {
		rts_err_ack(q, mp, TOUTSTATE, 0);
		return;
	}
	rts->rts_state = TS_UNBND;
	rts_ok_ack(q, mp);
}

/*
 * This routine is called to handle each
 * O_T_BIND_REQ/T_BIND_REQ message passed to
 * rts_wput. Note: This routine works with both
 * O_T_BIND_REQ and T_BIND_REQ semantics.
 */
static void
rts_bind(queue_t *q, mblk_t *mp)
{
	mblk_t	*mp1;
	struct T_bind_req *tbr;
	rts_t	*rts;

	rts = (rts_t *)q->q_ptr;
	if ((mp->b_wptr - mp->b_rptr) < sizeof (*tbr)) {
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "rts_bind: bad data, %d", rts->rts_state);
		rts_err_ack(q, mp, TBADADDR, 0);
		return;
	}
	if (rts->rts_state != TS_UNBND) {
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "rts_bind: bad state, %d", rts->rts_state);
		rts_err_ack(q, mp, TOUTSTATE, 0);
		return;
	}
	/*
	 * Reallocate the message to make sure we have enough room for an
	 * address and the protocol type.
	 */
	mp1 = reallocb(mp, sizeof (struct T_bind_ack) + sizeof (sin_t), 1);
	if (mp1 == NULL) {
		rts_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}
	mp = mp1;
	tbr = (struct T_bind_req *)mp->b_rptr;
	if (tbr->ADDR_length != 0) {
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "rts_bind: bad ADDR_length %d", tbr->ADDR_length);
		rts_err_ack(q, mp, TBADADDR, 0);
		return;
	}
	/* Generic request */
	tbr->ADDR_offset = (t_scalar_t)sizeof (struct T_bind_req);
	tbr->ADDR_length = 0;
	tbr->PRIM_type = T_BIND_ACK;
	rts->rts_state = TS_IDLE;
	qreply(q, mp);
}

static void
rts_copy_info(struct T_info_ack *tap, rts_t *rts)
{
	*tap = rts_g_t_info_ack;
	tap->CURRENT_state = rts->rts_state;
	tap->OPT_size = rts_max_optbuf_len;
}

/*
 * This routine responds to T_CAPABILITY_REQ messages.  It is called by
 * rts_wput.  Much of the T_CAPABILITY_ACK information is copied from
 * rts_g_t_info_ack.  The current state of the stream is copied from
 * rts_state.
 */
static void
rts_capability_req(queue_t *q, mblk_t *mp)
{
	rts_t			*rts = (rts_t *)q->q_ptr;
	t_uscalar_t		cap_bits1;
	struct T_capability_ack	*tcap;

	cap_bits1 = ((struct T_capability_req *)mp->b_rptr)->CAP_bits1;

	mp = tpi_ack_alloc(mp, sizeof (struct T_capability_ack),
		mp->b_datap->db_type, T_CAPABILITY_ACK);
	if (mp == NULL)
		return;

	tcap = (struct T_capability_ack *)mp->b_rptr;
	tcap->CAP_bits1 = 0;

	if (cap_bits1 & TC1_INFO) {
		rts_copy_info(&tcap->INFO_ack, rts);
		tcap->CAP_bits1 |= TC1_INFO;
	}

	qreply(q, mp);
}

/*
 * This routine responds to T_INFO_REQ messages.  It is called by rts_wput.
 * Most of the T_INFO_ACK information is copied from rts_g_t_info_ack.
 * The current state of the stream is copied from rts_state.
 */
static void
rts_info_req(queue_t *q, mblk_t *mp)
{
	rts_t	*rts = (rts_t *)q->q_ptr;

	mp = tpi_ack_alloc(mp, sizeof (rts_g_t_info_ack), M_PCPROTO,
	    T_INFO_ACK);
	if (mp == NULL)
		return;
	rts_copy_info((struct T_info_ack *)mp->b_rptr, rts);
	qreply(q, mp);
}

/*
 * This routine gets default values of certain options whose default
 * values are maintained by protcol specific code
 */
/* ARGSUSED */
int
rts_opt_default(queue_t *q, t_scalar_t level, t_scalar_t name, uchar_t *ptr)
{
	/* no default value processed by protocol specific code currently */
	return (-1);
}

/*
 * This routine retrieves the current status of socket options.
 * It returns the size of the option retrieved.
 */
int
rts_opt_get(queue_t *q, t_scalar_t level, t_scalar_t name, uchar_t *ptr)
{
	int	*i1 = (int *)ptr;
	rts_t	*rts = (rts_t *)q->q_ptr;

	switch (level) {
	case SOL_SOCKET:
		switch (name) {
		case SO_DEBUG:
			*i1 = rts->rts_debug;
			break;
		case SO_REUSEADDR:
			*i1 = rts->rts_reuseaddr;
			break;
		case SO_TYPE:
			*i1 = SOCK_RAW;
			break;

		/*
		 * The following three items are available here,
		 * but are only meaningful to IP.
		 */
		case SO_DONTROUTE:
			*i1 = rts->rts_dontroute;
			break;
		case SO_USELOOPBACK:
			*i1 = rts->rts_useloopback;
			break;
		case SO_BROADCAST:
			*i1 = rts->rts_broadcast;
			break;
		case SO_PROTOTYPE:
			*i1 = rts->rts_proto;
			break;
		/*
		 * The following two items can be manipulated,
		 * but changing them should do nothing.
		 */
		case SO_SNDBUF:
			ASSERT(q->q_hiwat <= INT_MAX);
			*i1 = (int)(q->q_hiwat);
			break;
		case SO_RCVBUF:
			ASSERT(q->q_hiwat <= INT_MAX);
			*i1 = (int)(RD(q)->q_hiwat);
			break;
		default:
			return (-1);
		}
		break;
	default:
		return (-1);
	}
	return ((int)sizeof (int));
}


/*
 * This routine sets socket options.
 */
int
rts_opt_set(queue_t *q, uint_t optset_context, t_scalar_t level,
    t_scalar_t name, uint_t inlen, uchar_t *invalp, uint_t *outlenp,
    uchar_t *outvalp, uchar_t *thisdg_attrs)
{
	int	*i1 = (int *)invalp;
	rts_t	*rts = (rts_t *)q->q_ptr;
	boolean_t checkonly;

	switch (optset_context) {
	case SETFN_OPTCOM_CHECKONLY:
		checkonly = B_TRUE;
		/*
		 * Note: Implies T_CHECK semantics for T_OPTCOM_REQ
		 * inlen != 0 implies value supplied and
		 * 	we have to "pretend" to set it.
		 * inlen == 0 implies that there is no
		 * 	value part in T_CHECK request and just validation
		 * done elsewhere should be enough, we just return here.
		 */
		if (inlen == 0) {
			*outlenp = 0;
			return (0);
		}
		break;
	case SETFN_OPTCOM_NEGOTIATE:
		checkonly = B_FALSE;
		break;
	case SETFN_UD_NEGOTIATE:
	case SETFN_CONN_NEGOTIATE:
		checkonly = B_FALSE;
		/*
		 * Negotiating local and "association-related" options
		 * through T_UNITDATA_REQ or T_CONN_{REQ,CON}
		 * Not allowed in this module.
		 */
		return (EINVAL);
	default:
		/*
		 * We should never get here
		 */
		*outlenp = 0;
		return (EINVAL);
	}

	ASSERT((optset_context != SETFN_OPTCOM_CHECKONLY) ||
	    (optset_context == SETFN_OPTCOM_CHECKONLY && inlen != 0));

	/*
	 * For rts, we should have no ancillary data sent down
	 * (rts_wput doesn't handle options).
	 */
	ASSERT(thisdg_attrs == NULL);

	/*
	 * For fixed length options, no sanity check
	 * of passed in length is done. It is assumed *_optcom_req()
	 * routines do the right thing.
	 */

	switch (level) {
	case SOL_SOCKET:
		switch (name) {
		case SO_REUSEADDR:
			if (!checkonly)
				rts->rts_reuseaddr = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_DEBUG:
			if (!checkonly)
				rts->rts_debug = *i1;
			break;	/* goto sizeof (int) option return */
		/*
		 * The following three items are available here,
		 * but are only meaningful to IP.
		 */
		case SO_DONTROUTE:
			if (!checkonly)
				rts->rts_dontroute = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_USELOOPBACK:
			if (!checkonly)
				rts->rts_useloopback = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_BROADCAST:
			if (!checkonly)
				rts->rts_broadcast = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_PROTOTYPE:
			/*
			 * Routing socket applications that call socket() with
			 * a third argument can filter which messages will be
			 * sent upstream thanks to sockfs.  so_socket() sends
			 * down the SO_PROTOTYPE and rts_queue_input()
			 * implements the filtering.
			 */
			if (*i1 != AF_INET && *i1 != AF_INET6)
				return (EPROTONOSUPPORT);
			if (!checkonly)
				rts->rts_proto = *i1;
			break;	/* goto sizeof (int) option return */
		/*
		 * The following two items can be manipulated,
		 * but changing them should do nothing.
		 */
		case SO_SNDBUF:
			if (*i1 > rts_max_buf) {
				*outlenp = 0;
				return (ENOBUFS);
			}
			if (!checkonly) {
				q->q_hiwat = *i1;
				q->q_next->q_hiwat = *i1;
			}
			break;	/* goto sizeof (int) option return */
		case SO_RCVBUF:
			if (*i1 > rts_max_buf) {
				*outlenp = 0;
				return (ENOBUFS);
			}
			if (!checkonly) {
				RD(q)->q_hiwat = *i1;
				(void) mi_set_sth_hiwat(RD(q), *i1);
			}
			break;	/* goto sizeof (int) option return */
		default:
			*outlenp = 0;
			return (EINVAL);
		}
		break;
	default:
		*outlenp = 0;
		return (EINVAL);
	}
	/*
	 * Common case of return from an option that is sizeof (int)
	 */
	*(int *)outvalp = *i1;
	*outlenp = (t_uscalar_t)sizeof (int);
	return (0);
}

/*
 * This routine frees the ND table if all streams have been closed.
 * It is called by rts_close and rts_open.
 */
static void
rts_param_cleanup(void)
{
	if (!rts_open_streams)
		nd_free(&rts_g_nd);
}

/*
 * This routine retrieves the value of an ND variable in a rtsparam_t
 * structure. It is called through nd_getset when a user reads the
 * variable.
 */
/* ARGSUSED */
static int
rts_param_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	rtsparam_t	*rtspa = (rtsparam_t *)cp;

	(void) mi_mpprintf(mp, "%u", rtspa->rts_param_value);
	return (0);
}

/*
 * Walk through the param array specified registering each element with the
 * named dispatch (ND) handler.
 */
static boolean_t
rts_param_register(rtsparam_t *rtspa, int cnt)
{
	for (; cnt-- > 0; rtspa++) {
		if (rtspa->rts_param_name != NULL && rtspa->rts_param_name[0]) {
			if (!nd_load(&rts_g_nd, rtspa->rts_param_name,
			    rts_param_get, rts_param_set, (caddr_t)rtspa)) {
				nd_free(&rts_g_nd);
				return (B_FALSE);
			}
		}
	}
	return (B_TRUE);
}

/* This routine sets an ND variable in a rtsparam_t structure. */
/* ARGSUSED */
static int
rts_param_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{
	char	*end;
	uint_t	new_value;
	rtsparam_t	*rtspa = (rtsparam_t *)cp;

	/* Convert the value from a string into a uint_t. */
	new_value = (uint_t)mi_strtol(value, &end, 10);
	/*
	 * Fail the request if the new value does not lie within the
	 * required bounds.
	 */
	if (end == value ||
	    new_value < rtspa->rts_param_min ||
	    new_value > rtspa->rts_param_max)
		return (EINVAL);

	/* Set the new value */
	rtspa->rts_param_value = new_value;
	return (0);
}

/*
 * This routine handles synchronous messages passed downstream. It either
 * consumes the message or passes it downstream; it never queues a
 * a message. The data messages that go down are wrapped in an IOCTL
 * message.
 *
 * Since it is synchronous, it waits for the M_IOCACK/M_IOCNAK so that
 * it can return an immediate error (such as ENETUNREACH when adding a route).
 * It uses the RTS_WRW_PENDING to ensure that each rts instance has only
 * one M_IOCTL outstanding at any given time.
 */
static int
rts_wrw(queue_t *q, struiod_t *dp)
{
	mblk_t	*mp = dp->d_mp;
	mblk_t	*mp1;
	int	error;
	rt_msghdr_t	*rtm;
	rts_t	*rts;

	rts = (rts_t *)q->q_ptr;
	while (rts->rts_flag & RTS_WRW_PENDING)
		qwait_rw(q);
	rts->rts_flag |= RTS_WRW_PENDING;

	if (isuioq(q) && (error = struioget(q, mp, dp, 0))) {
		/*
		 * Uio error of some sort, so just return the error.
		 */
		rts->rts_error = error;
		goto err_ret;
	}
	/*
	 * Pass the mblk (chain) onto wput().
	 */
	dp->d_mp = 0;

	switch (mp->b_datap->db_type) {
	case M_PROTO:
	case M_PCPROTO:
		/* Expedite other than T_DATA_REQ to below the switch */
		if (((mp->b_wptr - mp->b_rptr) !=
		    sizeof (struct T_data_req)) ||
		    (((union T_primitives *)mp->b_rptr)->type != T_DATA_REQ))
			break;
		if ((mp1 = mp->b_cont) == NULL) {
			rts->rts_error = EINVAL;
			goto err_ret;
		}
		(void) unlinkb(mp);
		freemsg(mp);
		mp = mp1;
		/* FALLTHRU */
	case M_DATA:
		/*
		 * The semantics of the routing socket is such that the rtm_pid
		 * field is automatically filled in during requests with the
		 * current process' pid.  We do this here (where we still have
		 * user context) after checking we have at least a message the
		 * size of a routing message header.
		 */
		if ((mp->b_wptr - mp->b_rptr) < sizeof (rt_msghdr_t)) {
			if (!pullupmsg(mp, sizeof (rt_msghdr_t))) {
				rts->rts_error = EINVAL;
				goto err_ret;
			}
		}
		rtm = (rt_msghdr_t *)mp->b_rptr;
		rtm->rtm_pid = curproc->p_pid;
		break;
	default:
		break;
	}
	rts->rts_flag |= RTS_WPUT_PENDING;
	rts_wput(q, mp);
	while (rts->rts_flag & RTS_WPUT_PENDING)
		qwait_rw(q);
err_ret:
	rts->rts_flag &= ~RTS_WRW_PENDING;
	return (rts->rts_error);
}

/*
 * This routine handles all messages passed downstream. It either
 * consumes the message or passes it downstream; it never queues a
 * a message. The data messages that go down are wrapped in an IOCTL
 * message.
 */
static void
rts_wput(queue_t *q, mblk_t *mp)
{
	uchar_t	*rptr = mp->b_rptr;
	mblk_t	*mp1;

	switch (mp->b_datap->db_type) {
	case M_DATA:
		break;
	case M_PROTO:
	case M_PCPROTO:
		if ((mp->b_wptr - rptr) == sizeof (struct T_data_req)) {
			/* Expedite valid T_DATA_REQ to below the switch */
			if (((union T_primitives *)rptr)->type == T_DATA_REQ) {
				if ((mp1 = mp->b_cont) == NULL) {
					freemsg(mp);
					return;
				}
				(void) unlinkb(mp);
				freemsg(mp);
				mp = mp1;
				break;
			}
		}
		/* FALLTHRU */
	default:
		rts_wput_other(q, mp);
		return;
	}
	mp1 = rts_ioctl_alloc(mp);
	if (mp1 == NULL) {
		freemsg(mp);
		return;
	}
	putnext(q, mp1);
}


/*
 * Handles all the control message, if it
 * can not understand it, it will
 * pass down stream.
 */
static void
rts_wput_other(queue_t *q, mblk_t *mp)
{
	uchar_t	*rptr = mp->b_rptr;
	rts_t	*rts;
	struct iocblk	*iocp;

	rts = (rts_t *)q->q_ptr;
	switch (mp->b_datap->db_type) {
	case M_PROTO:
	case M_PCPROTO:
		if ((mp->b_wptr - rptr) < sizeof (t_scalar_t)) {
			/*
			 * If the message does not contain a PRIM_type,
			 * throw it away.
			 */
			freemsg(mp);
			return;
		}
		switch (((union T_primitives *)rptr)->type) {
		case T_BIND_REQ:
		case O_T_BIND_REQ:
			rts_bind(q, mp);
			return;
		case T_UNBIND_REQ:
			rts_unbind(q, mp);
			return;
		case T_CAPABILITY_REQ:
			rts_capability_req(q, mp);
			return;
		case T_INFO_REQ:
			rts_info_req(q, mp);
			return;
		case T_SVR4_OPTMGMT_REQ:
			svr4_optcom_req(q, mp, rts->rts_priv_stream,
			    &rts_opt_obj);
			return;
		case T_OPTMGMT_REQ:
			tpi_optcom_req(q, mp, rts->rts_priv_stream,
			    &rts_opt_obj);
			return;
		case O_T_CONN_RES:
		case T_CONN_RES:
		case T_DISCON_REQ:
			/* Not supported by rts. */
			rts_err_ack(q, mp, TNOTSUPPORT, 0);
			return;
		case T_DATA_REQ:
		case T_EXDATA_REQ:
		case T_ORDREL_REQ:
			/* Illegal for rts. */
			freemsg(mp);
			(void) putnextctl1(RD(q), M_ERROR, EPROTO);
			return;
		default:
			break;
		}
		break;
	case M_IOCTL:
		iocp = (struct iocblk *)mp->b_rptr;
		switch (iocp->ioc_cmd) {
		case ND_SET:
			if (!rts->rts_priv_stream) {
				iocp->ioc_error = EPERM;
				iocp->ioc_count = 0;
				mp->b_datap->db_type = M_IOCACK;
				qreply(q, mp);
				return;
			}
			/* FALLTHRU */
		case ND_GET:
			if (nd_getset(q, rts_g_nd, mp)) {
				qreply(q, mp);
				return;
			}
			break;
		case TI_GETPEERNAME:
			mi_copyin(q, mp, NULL,
			    SIZEOF_STRUCT(strbuf, iocp->ioc_flag));
			return;
		default:
			break;
		}
	case M_IOCDATA:
		rts_wput_iocdata(q, mp);
		return;
	default:
		break;
	}
	putnext(q, mp);
}

/*
 * Called by rts_wput_other to handle all M_IOCDATA messages.
 */
static void
rts_wput_iocdata(queue_t *q, mblk_t *mp)
{
	struct sockaddr	*rtsaddr;
	mblk_t	*mp1;
	STRUCT_HANDLE(strbuf, sb);
	struct iocblk	*iocp	= (struct iocblk *)mp->b_rptr;

	/* Make sure it is one of ours. */
	switch (iocp->ioc_cmd) {
	case TI_GETPEERNAME:
		break;
	default:
		putnext(q, mp);
		return;
	}
	switch (mi_copy_state(q, mp, &mp1)) {
	case -1:
		return;
	case MI_COPY_CASE(MI_COPY_IN, 1):
		break;
	case MI_COPY_CASE(MI_COPY_OUT, 1):
		/* Copy out the strbuf. */
		mi_copyout(q, mp);
		return;
	case MI_COPY_CASE(MI_COPY_OUT, 2):
		/* All done. */
		mi_copy_done(q, mp, 0);
		return;
	default:
		mi_copy_done(q, mp, EPROTO);
		return;
	}
	STRUCT_SET_HANDLE(sb, iocp->ioc_flag, (void *)mp1->b_rptr);
	if (STRUCT_FGET(sb, maxlen) < (int)sizeof (sin_t)) {
		mi_copy_done(q, mp, EINVAL);
		return;
	}
	switch (iocp->ioc_cmd) {
	case TI_GETPEERNAME:
		break;
	default:
		mi_copy_done(q, mp, EPROTO);
		return;
	}
	mp1 = mi_copyout_alloc(q, mp, STRUCT_FGETP(sb, buf), sizeof (sin_t));
	if (mp1 == NULL)
		return;
	STRUCT_FSET(sb, len, (int)sizeof (sin_t));
	rtsaddr = (struct sockaddr *)mp1->b_rptr;
	mp1->b_wptr = (uchar_t *)&rtsaddr[1];
	bzero(rtsaddr, sizeof (struct sockaddr));
	rtsaddr->sa_family = AF_ROUTE;
	/* Copy out the address */
	mi_copyout(q, mp);
}

static void
rts_rput(queue_t *q, mblk_t *mp)
{
	rts_t	*rts;
	struct iocblk	*iocp;
	mblk_t *mp1;
	struct T_data_ind *tdi;

	rts = (rts_t *)q->q_ptr;
	switch (mp->b_datap->db_type) {
	case M_IOCACK:
	case M_IOCNAK:
		iocp = (struct iocblk *)mp->b_rptr;
		if (rts->rts_flag & RTS_WPUT_PENDING) {
			rts->rts_error = iocp->ioc_error;
			rts->rts_flag &= ~RTS_WPUT_PENDING;
			freemsg(mp);
			return;
		}
		break;
	case M_DATA:
		/*
		 * Prepend T_DATA_IND to prevent the stream head from
		 * consolidating multiple messages together.
		 * If the allocation fails just send up the M_DATA.
		 */
		mp1 = allocb(sizeof (*tdi), BPRI_MED);
		if (mp1 != NULL) {
			mp1->b_cont = mp;
			mp = mp1;

			mp->b_datap->db_type = M_PROTO;
			mp->b_wptr += sizeof (*tdi);
			tdi = (struct T_data_ind *)mp->b_rptr;
			tdi->PRIM_type = T_DATA_IND;
			tdi->MORE_flag = 0;
		}
		break;
	default:
		break;
	}
	putnext(q, mp);
}


void
rts_ddi_init(void)
{
	rts_max_optbuf_len = optcom_max_optbuf_len(rts_opt_obj.odb_opt_des_arr,
	    rts_opt_obj.odb_opt_arr_cnt);
}

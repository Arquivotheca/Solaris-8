/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)sockstr.c	1.33	99/10/22 SMI"

#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/open.h>
#include <sys/user.h>
#include <sys/termios.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/esunddi.h>
#include <sys/flock.h>
#include <sys/modctl.h>
#include <sys/vtrace.h>
#include <sys/cmn_err.h>
#include <sys/proc.h>
#include <sys/ddi.h>

#include <sys/suntpi.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <sys/un.h>

#include <sys/tiuser.h>
#define	_SUN_TPI_VERSION	2
#include <sys/tihdr.h>
#include <sys/tl.h>	/* For TL_IOC .. */

#include <c2/audit.h>

int so_default_version = SOV_SOCKSTREAM;

#ifdef DEBUG
/* Set sockdebug to print debug messages when SO_DEBUG is set */
int sockdebug = 0;

/* Set sockprinterr to print error messages when SO_DEBUG is set */
int sockprinterr = 0;

/*
 * Set so_default_options to SO_DEBUG is all sockets should be created
 * with SO_DEBUG set. This is needed to get debug printouts from the
 * socket() call itself.
 */
int so_default_options = 0;
#endif /* DEBUG */

#ifdef SOCK_TEST
/*
 * Set to number of ticks to limit cv_waits for code coverage testing.
 * Set to 1000 when SO_DEBUG is set to 2.
 */
clock_t sock_test_timelimit = 0;
#endif /* SOCK_TEST */

/*
 * For concurrency testing of e.g. opening /dev/ip which does not
 * handle T_INFO_REQ messages.
 */
int so_no_tinfo = 0;

/*
 * Timeout for getting a T_CAPABILITY_ACK - it is possible for a provider
 * to simply ignore the T_CAPABILITY_REQ.
 */
clock_t	sock_capability_timeout	= 2;	/* seconds */

static int do_tcapability(struct sonode *so, t_uscalar_t cap_bits1);
static void so_installhooks(struct sonode *so, int on);

static mblk_t *strsock_proto(vnode_t *vp, mblk_t *mp,
		strwakeup_t *wakeups, strsigset_t *firstmsgsigs,
		strsigset_t *allmsgsigs, strpollset_t *pollwakeups);
static mblk_t *strsock_misc(vnode_t *vp, mblk_t *mp,
		strwakeup_t *wakeups, strsigset_t *firstmsgsigs,
		strsigset_t *allmsgsigs, strpollset_t *pollwakeups);

static int tlitosyserr(int terr);

/*
 * Convert a socket to a stream. Invoked when the illusory sockmod
 * is popped from the stream.
 * Change the stream head back to default operation without loosing
 * any messages (T_conn_ind's are moved the to stream head queue).
 */
void
so_sock2stream(struct sonode *so)
{
	struct vnode *vp = SOTOV(so);
	queue_t *rq;
	mblk_t *mp;

	ASSERT(MUTEX_HELD(&so->so_lock));
	ASSERT(so->so_flag & SOLOCKED);

	ASSERT(so->so_version != SOV_STREAM);
	so->so_version = SOV_STREAM;

	/*
	 * Remove the hooks in the stream head to avoid queuing more
	 * packets in sockfs.
	 */
	mutex_exit(&so->so_lock);
	so_installhooks(so, 0);
	mutex_enter(&so->so_lock);

	/*
	 * Clear any state related to urgent data. Leave any T_EXDATA_IND
	 * on the queue - the behavior of urgent data after a switch is
	 * left undefined.
	 */
	so->so_error = so->so_delayed_error = 0;
	freemsg(so->so_oobmsg);
	so->so_oobmsg = NULL;
	so->so_oobsigcnt = so->so_oobcnt = 0;

	so->so_state &= ~(SS_RCVATMARK|SS_OOBPEND|SS_HAVEOOBDATA|SS_HADOOBDATA|
			SS_HASCONNIND|SS_SAVEDEOR);
	ASSERT(so_verify_oobstate(so));

	freemsg(so->so_ack_mp);
	so->so_ack_mp = NULL;

	/*
	 * Move any queued T_CONN_IND messages to stream head queue.
	 */
	rq = RD(strvp2wq(vp));
	while ((mp = so->so_conn_ind_head) != NULL) {
		so->so_conn_ind_head = mp->b_next;
		mp->b_next = NULL;
		if (so->so_conn_ind_head == NULL) {
			ASSERT(so->so_conn_ind_tail == mp);
			so->so_conn_ind_tail = NULL;
		}
		dprintso(so, 0,
			("so_sock2stream(%p): moving T_CONN_IND\n",
			so));
		/* Drop lock across put() */
		mutex_exit(&so->so_lock);
		put(rq, mp);
		mutex_enter(&so->so_lock);
	}
	ASSERT(MUTEX_HELD(&so->so_lock));
}

/*
 * Covert a stream back to a socket. This is invoked when the illusory
 * sockmod is pushed on a stream (where the stream was "created" by
 * popping the illusory sockmod).
 * This routine can not recreate the socket state (certain aspects of
 * it like urgent data state and the bound/connected addresses for AF_UNIX
 * sockets can not be recreated by asking the transport for information).
 * Thus this routine implicitly assumes that the socket is in an initial
 * state (as if it was just created). It flushes any messages queued on the
 * read queue to avoid dealing with e.g. TPI acks or T_exdata_ind messages.
 */
void
so_stream2sock(struct sonode *so)
{
	struct vnode *vp = SOTOV(so);

	ASSERT(MUTEX_HELD(&so->so_lock));
	ASSERT(so->so_flag & SOLOCKED);
	ASSERT(so->so_version == SOV_STREAM);
	so->so_version = SOV_SOCKSTREAM;
	so->so_pushcnt = 0;
	mutex_exit(&so->so_lock);

	/*
	 * Set a permenent error to force any thread in sorecvmsg to
	 * return (and drop SOREADLOCKED). Clear the error once
	 * we have SOREADLOCKED.
	 * This makes a read sleeping during the I_PUSH of sockmod return
	 * EIO.
	 */
	strsetrerror(SOTOV(so), EIO, 1, NULL);

	/*
	 * Get the read lock before flushing data to avoid
	 * problems with the T_EXDATA_IND MSG_PEEK code in sorecvmsg.
	 */
	mutex_enter(&so->so_lock);
	(void) so_lock_single(so, SOREADLOCKED, 0);
	mutex_exit(&so->so_lock);

	strsetrerror(SOTOV(so), 0, 0, NULL);
	so_installhooks(so, 1);

	/*
	 * Flush everything on the read queue.
	 * This ensures that no T_CONN_IND remain and that no T_EXDATA_IND
	 * remain; those types of messages would confuse sockfs.
	 */
	strflushrq(vp, FLUSHALL);
	mutex_enter(&so->so_lock);
	so_unlock_single(so, SOREADLOCKED);
}

/*
 * Install/deinstall the hooks in the stream head.
 */
static void
so_installhooks(struct sonode *so, int on)
{
	struct vnode *vp = SOTOV(so);
	uint_t flags;

	if (on) {
		flags = SH_SIGALLDATA | SH_IGN_ZEROLEN | SH_CONSOL_DATA;

		strsetrputhooks(vp, flags, strsock_proto, strsock_misc);
		strsetwputhooks(vp, SH_SIGPIPE | SH_RECHECK_ERR, 0);
	} else {
		strsetrputhooks(vp, 0, NULL, NULL);
		strsetwputhooks(vp, 0, STRTIMOUT);
		/*
		 * Leave read behavior as it would have been for a normal
		 * stream i.e. a read of an M_PROTO will fail.
		 */
	}
}

/*
 * Initialize the streams side of a socket including
 * T_info_req/ack processing. If tso is not NULL its values are used thereby
 * avoiding the T_INFO_REQ.
 */
int
so_strinit(struct sonode *so, struct sonode *tso)
{
	struct vnode *vp = SOTOV(so);
	struct stdata *stp;
	mblk_t *mp;
	int error;

	dprintso(so, 1, ("so_strinit(%p)\n", so));

	/* Preallocate an unbind_req message */
	mp = soallocproto(sizeof (struct T_unbind_req), _ALLOC_SLEEP);
	mutex_enter(&so->so_lock);
	so->so_unbind_mp = mp;
#ifdef DEBUG
	so->so_options = so_default_options;
#endif /* DEBUG */
	mutex_exit(&so->so_lock);

	so_installhooks(so, 1);

	if (tso == NULL) {
		error = do_tcapability(so, TC1_ACCEPTOR_ID | TC1_INFO);
		if (error)
			return (error);
	} else {
		mutex_enter(&so->so_lock);
		so->so_tsdu_size = tso->so_tsdu_size;
		so->so_etsdu_size = tso->so_etsdu_size;
		so->so_addr_size = tso->so_addr_size;
		so->so_opt_size = tso->so_opt_size;
		so->so_tidu_size = tso->so_tidu_size;
		so->so_serv_type = tso->so_serv_type;
		so->so_mode = tso->so_mode & ~SM_ACCEPTOR_ID;
		mutex_exit(&so->so_lock);

		/* the following do_tcapability may update so->so_mode */
		if (tso->so_serv_type != T_CLTS) {
			error = do_tcapability(so, TC1_ACCEPTOR_ID);
			if (error)
				return (error);
		}
	}
	/*
	 * If the addr_size is 0 we treat it as already bound
	 * and connected. This is used by the routing socket.
	 * We set the addr_size to something to allocate a the address
	 * structures.
	 */
	if (so->so_addr_size == 0) {
		so->so_state |= SS_ISBOUND | SS_ISCONNECTED;
		/* Address size can vary with address families. */
		if (so->so_family == AF_INET6)
			so->so_addr_size =
			    (t_scalar_t)sizeof (struct sockaddr_in6);
		else
			so->so_addr_size =
			    (t_scalar_t)sizeof (struct sockaddr_in);
		ASSERT(so->so_unbind_mp);
	}
	/*
	 * Allocate the addresses.
	 */
	ASSERT(so->so_laddr_sa == NULL && so->so_faddr_sa == NULL);
	ASSERT(so->so_laddr_len == 0 && so->so_faddr_len == 0);
	so->so_laddr_maxlen = (socklen_t)so->so_addr_size;
	so->so_laddr_sa = kmem_alloc(so->so_addr_size, KM_SLEEP);
	so->so_faddr_maxlen = (socklen_t)so->so_addr_size;
	so->so_faddr_sa = kmem_alloc(so->so_addr_size, KM_SLEEP);

	if (so->so_family == AF_UNIX) {
		/* Send down indication that this is a socket */
		int32_t arg = 1;
		struct strioctl strioc;
		int32_t retval;

		strioc.ic_cmd = TL_IOC_SOCKET;
		strioc.ic_timout = INFTIM;
		strioc.ic_len = (int)sizeof (arg);
		strioc.ic_dp = (char *)&arg;
		error = strdoioctl(vp->v_stream, &strioc, NULL,
				FNATIVE, STR_NOSIG|K_TO_K, NULL,
				CRED(), &retval);
		if (error)
			printf("TL_IOC_SOCKET failed: %d\n", error);
		/*
		 * Clear AF_UNIX related fields
		 */
		bzero(&so->so_ux_laddr, sizeof (so->so_ux_laddr));
		bzero(&so->so_ux_faddr, sizeof (so->so_ux_faddr));
	}

	stp = vp->v_stream;
	/*
	 * Have to keep minpsz at zero in order to allow write/send of zero
	 * bytes.
	 */
	mutex_enter(&stp->sd_lock);
	if (stp->sd_qn_minpsz == 1)
		stp->sd_qn_minpsz = 0;
	mutex_exit(&stp->sd_lock);

	if (suser(CRED())) {
		mutex_enter(&so->so_lock);
		so->so_mode |= SM_PRIV;
		mutex_exit(&so->so_lock);
	}
	return (0);
}

static void
copy_tinfo(struct sonode *so, struct T_info_ack *tia)
{
	so->so_tsdu_size = tia->TSDU_size;
	so->so_etsdu_size = tia->ETSDU_size;
	so->so_addr_size = tia->ADDR_size;
	so->so_opt_size = tia->OPT_size;
	so->so_tidu_size = tia->TIDU_size;
	so->so_serv_type = tia->SERV_type;
	switch (tia->CURRENT_state) {
	case TS_UNBND:
		break;
	case TS_IDLE:
		so->so_state |= SS_ISBOUND;
		so->so_laddr_len = 0;
		break;
	case TS_DATA_XFER:
		so->so_state |= SS_ISBOUND|SS_ISCONNECTED;
		so->so_laddr_len = 0;
		so->so_faddr_len = 0;
		break;
	}

	/*
	 * Heuristics for determining the socket mode flags
	 * (SM_ATOMIC, SM_CONNREQUIRED, SM_ADDR, SM_FDPASSING,
	 * and SM_EXDATA, SM_OPTDATA, and SM_BYTESTREAM)
	 * from the info ack.
	 */
	if (so->so_serv_type == T_CLTS) {
		so->so_mode |= SM_ATOMIC | SM_ADDR;
	} else {
		so->so_mode |= SM_CONNREQUIRED;
		if (so->so_etsdu_size != 0 && so->so_etsdu_size != -2)
			so->so_mode |= SM_EXDATA;
	}
	if (so->so_type == SOCK_SEQPACKET) {
		/* Semantics are to discard tail end of messages */
		so->so_mode |= SM_ATOMIC;
	}
	if (so->so_family == AF_UNIX) {
		so->so_mode |= SM_FDPASSING | SM_OPTDATA;
		if (so->so_addr_size == -1) {
			/* MAXPATHLEN + soun_family + nul termination */
			so->so_addr_size = (t_scalar_t)(MAXPATHLEN +
				sizeof (short) + 1);
		}
		if (so->so_type == SOCK_STREAM) {
			/*
			 * Make it into a byte-stream transport.
			 * SOCK_SEQPACKET sockets are unchanged.
			 */
			so->so_tsdu_size = 0;
		}
	} else if (so->so_addr_size == -1) {
		/*
		 * Logic extracted from sockmod - have to pick some max address
		 * length in order to preallocate the addresses.
		 */
		so->so_addr_size = SOA_DEFSIZE;
	}
	if (so->so_tsdu_size == 0)
		so->so_mode |= SM_BYTESTREAM;
}

static int
check_tinfo(struct sonode *so)
{
	/* Consistency checks */
	if (so->so_type == SOCK_DGRAM && so->so_serv_type != T_CLTS) {
		eprintso(so, ("service type and socket type mismatch\n"));
		eprintsoline(so, EPROTO);
		return (EPROTO);
	}
	if (so->so_type == SOCK_STREAM && so->so_serv_type == T_CLTS) {
		eprintso(so, ("service type and socket type mismatch\n"));
		eprintsoline(so, EPROTO);
		return (EPROTO);
	}
	if (so->so_type == SOCK_SEQPACKET && so->so_serv_type == T_CLTS) {
		eprintso(so, ("service type and socket type mismatch\n"));
		eprintsoline(so, EPROTO);
		return (EPROTO);
	}
	if (so->so_family == AF_INET &&
	    so->so_addr_size != (t_scalar_t)sizeof (struct sockaddr_in)) {
		eprintso(so,
		    ("AF_INET must have sockaddr_in address length. Got %d\n",
		    so->so_addr_size));
		eprintsoline(so, EMSGSIZE);
		return (EMSGSIZE);
	}
	if (so->so_family == AF_INET6 &&
	    so->so_addr_size != (t_scalar_t)sizeof (struct sockaddr_in6)) {
		eprintso(so,
		    ("AF_INET6 must have sockaddr_in6 address length. Got %d\n",
		    so->so_addr_size));
		eprintsoline(so, EMSGSIZE);
		return (EMSGSIZE);
	}

	dprintso(so, 1, (
	    "tinfo: serv %d tsdu %d, etsdu %d, addr %d, opt %d, tidu %d\n",
	    so->so_serv_type, so->so_tsdu_size, so->so_etsdu_size,
	    so->so_addr_size, so->so_opt_size,
	    so->so_tidu_size));
	dprintso(so, 1, ("tinfo: so_state %s\n",
			pr_state(so->so_state, so->so_mode)));
	return (0);
}

/*
 * Send down T_info_req and wait for the ack.
 * Record interesting T_info_ack values in the sonode.
 */
static int
do_tinfo(struct sonode *so)
{
	struct T_info_req tir;
	mblk_t *mp;
	int error;

	ASSERT(MUTEX_NOT_HELD(&so->so_lock));

	if (so_no_tinfo) {
		so->so_addr_size = 0;
		return (0);
	}

	dprintso(so, 1, ("do_tinfo(%p)\n", so));

	/* Send T_INFO_REQ */
	tir.PRIM_type = T_INFO_REQ;
	mp = soallocproto1(&tir, sizeof (tir),
	    sizeof (struct T_info_req) + sizeof (struct T_info_ack),
	    _ALLOC_INTR);
	if (mp == NULL) {
		eprintsoline(so, ENOBUFS);
		return (ENOBUFS);
	}
	/* T_INFO_REQ has to be M_PCPROTO */
	mp->b_datap->db_type = M_PCPROTO;

	error = kstrputmsg(SOTOV(so), mp, NULL, 0, 0,
			MSG_BAND|MSG_HOLDSIG|MSG_IGNERROR, 0);
	if (error) {
		eprintsoline(so, error);
		return (error);
	}
	mutex_enter(&so->so_lock);
	/* Wait for T_INFO_ACK */
	if ((error = sowaitprim(so, T_INFO_REQ, T_INFO_ACK,
	    (t_uscalar_t)sizeof (struct T_info_ack), &mp, 0))) {
		mutex_exit(&so->so_lock);
		eprintsoline(so, error);
		return (error);
	}

	ASSERT(mp);
	copy_tinfo(so, (struct T_info_ack *)mp->b_rptr);
	mutex_exit(&so->so_lock);
	freemsg(mp);
	return (check_tinfo(so));
}

/*
 * Send down T_capability_req and wait for the ack.
 * Record interesting T_capability_ack values in the sonode.
 */
static int
do_tcapability(struct sonode *so, t_uscalar_t cap_bits1)
{
	struct T_capability_req tcr;
	struct T_capability_ack *tca;
	mblk_t *mp;
	int error;

	ASSERT(cap_bits1 != 0);
	ASSERT((cap_bits1 & ~(TC1_ACCEPTOR_ID | TC1_INFO)) == 0);
	ASSERT(MUTEX_NOT_HELD(&so->so_lock));

	if (so->so_provinfo->tpi_capability == PI_NO)
		return (do_tinfo(so));

	if (so_no_tinfo) {
		so->so_addr_size = 0;
		if ((cap_bits1 &= ~TC1_INFO) == 0)
			return (0);
	}

	dprintso(so, 1, ("do_tcapability(%p)\n", so));

	/* Send T_CAPABILITY_REQ */
	tcr.PRIM_type = T_CAPABILITY_REQ;
	tcr.CAP_bits1 = cap_bits1;
	mp = soallocproto1(&tcr, sizeof (tcr),
	    sizeof (struct T_capability_req) + sizeof (struct T_capability_ack),
	    _ALLOC_INTR);
	if (mp == NULL) {
		eprintsoline(so, ENOBUFS);
		return (ENOBUFS);
	}
	/* T_CAPABILITY_REQ should be M_PCPROTO here */
	mp->b_datap->db_type = M_PCPROTO;

	error = kstrputmsg(SOTOV(so), mp, NULL, 0, 0,
	    MSG_BAND|MSG_HOLDSIG|MSG_IGNERROR, 0);
	if (error) {
		eprintsoline(so, error);
		return (error);
	}
	mutex_enter(&so->so_lock);
	/* Wait for T_CAPABILITY_ACK */
	if ((error = sowaitprim(so, T_CAPABILITY_REQ, T_CAPABILITY_ACK,
	    (t_uscalar_t)sizeof (*tca), &mp, sock_capability_timeout * hz))) {
		mutex_exit(&so->so_lock);
		PI_PROVLOCK(so->so_provinfo);
		if (so->so_provinfo->tpi_capability == PI_DONTKNOW)
			so->so_provinfo->tpi_capability = PI_NO;
		PI_PROVUNLOCK(so->so_provinfo);
		ASSERT((so->so_mode & SM_ACCEPTOR_ID) == 0);
		if (cap_bits1 & TC1_INFO)
			return (do_tinfo(so));
		return (0);
	}

	if (so->so_provinfo->tpi_capability == PI_DONTKNOW) {
		PI_PROVLOCK(so->so_provinfo);
		so->so_provinfo->tpi_capability = PI_YES;
		PI_PROVUNLOCK(so->so_provinfo);
	}

	ASSERT(mp);
	tca = (struct T_capability_ack *)mp->b_rptr;

	ASSERT((cap_bits1 & TC1_INFO) == (tca->CAP_bits1 & TC1_INFO));

	cap_bits1 = tca->CAP_bits1;

	if (cap_bits1 & TC1_ACCEPTOR_ID) {
		so->so_acceptor_id = tca->ACCEPTOR_id;
		so->so_mode |= SM_ACCEPTOR_ID;
	}

	if (cap_bits1 & TC1_INFO)
		copy_tinfo(so, &tca->INFO_ack);

	mutex_exit(&so->so_lock);
	freemsg(mp);

	if (cap_bits1 & TC1_INFO)
		return (check_tinfo(so));

	return (0);
}

/*
 * Retrieve and clear the socket error.
 */
int
sogeterr(struct sonode *so, int ispeek)
{
	int error;

	ASSERT(MUTEX_HELD(&so->so_lock));

	error = so->so_error;
	if (!ispeek)
		so->so_error = 0;
	return (error);
}

/*
 * This routine is registered with the stream head to retrieve read
 * side errors.
 * It does not clear the socket error for a peeking read side operation.
 * It the error is to be cleared it sets *clearerr.
 */
int
sogetrderr(vnode_t *vp, int ispeek, int *clearerr)
{
	struct sonode *so = VTOSO(vp);
	int error;

	mutex_enter(&so->so_lock);
	if (ispeek) {
		error = so->so_error;
		*clearerr = 0;
	} else {
		error = so->so_error;
		so->so_error = 0;
		*clearerr = 1;
	}
	mutex_exit(&so->so_lock);
	return (error);
}

/*
 * This routine is registered with the stream head to retrieve write
 * side errors.
 * It does not clear the socket error for a peeking read side operation.
 * It the error is to be cleared it sets *clearerr.
 */
int
sogetwrerr(vnode_t *vp, int ispeek, int *clearerr)
{
	struct sonode *so = VTOSO(vp);
	int error;

	mutex_enter(&so->so_lock);
	if (so->so_state & SS_CANTSENDMORE) {
		error = EPIPE;
		*clearerr = 0;
	} else {
		error = so->so_error;
		if (ispeek) {
			*clearerr = 0;
		} else {
			so->so_error = 0;
			*clearerr = 1;
		}
	}
	mutex_exit(&so->so_lock);
	return (error);
}

/*
 * Set a nonpersistent read and write error on the socket.
 * Used when there is a T_uderror_ind for a connected socket.
 * The caller also needs to call strsetrerror and strsetwerror
 * after dropping the lock.
 */
void
soseterror(struct sonode *so, int error)
{
	ASSERT(error != 0);

	ASSERT(MUTEX_HELD(&so->so_lock));
	so->so_error = (ushort_t)error;
}

void
soisconnecting(struct sonode *so)
{
	ASSERT(MUTEX_HELD(&so->so_lock));
	so->so_state &= ~(SS_ISCONNECTED|SS_ISDISCONNECTING);
	so->so_state |= SS_ISCONNECTING;
	cv_broadcast(&so->so_state_cv);
}

void
soisconnected(struct sonode *so)
{
	ASSERT(MUTEX_HELD(&so->so_lock));
	so->so_state &= ~(SS_ISCONNECTING|SS_ISDISCONNECTING);
	so->so_state |= SS_ISCONNECTED;
	cv_broadcast(&so->so_state_cv);
}

/*
 * The caller also needs to call strsetrerror, strsetwerror and strseteof.
 */
void
soisdisconnected(struct sonode *so, int error)
{
	ASSERT(MUTEX_HELD(&so->so_lock));
	so->so_state &= ~(SS_ISCONNECTING|SS_ISCONNECTED|SS_ISDISCONNECTING);
	so->so_state |= (SS_CANTRCVMORE|SS_CANTSENDMORE);
	so->so_error = (ushort_t)error;
	cv_broadcast(&so->so_state_cv);
}

/*
 * For connected AF_UNIX SOCK_DGRAM sockets when the peer closes.
 * Does not affect write side.
 * The caller also has to call strsetrerror.
 */
static void
sobreakconn(struct sonode *so, int error)
{
	ASSERT(MUTEX_HELD(&so->so_lock));
	so->so_state &= ~(SS_ISCONNECTING|SS_ISCONNECTED|SS_ISDISCONNECTING);
	so->so_error = (ushort_t)error;
	cv_broadcast(&so->so_state_cv);
}

/*
 * Can no longer send.
 * Caller must also call strsetwerror.
 */
void
socantsendmore(struct sonode *so)
{
	ASSERT(MUTEX_HELD(&so->so_lock));
	so->so_state |= SS_CANTSENDMORE;
	cv_broadcast(&so->so_state_cv);
}

/*
 * The caller must call strseteof(,1) as well as this routine
 * to change the socket state.
 */
void
socantrcvmore(struct sonode *so)
{
	ASSERT(MUTEX_HELD(&so->so_lock));
	so->so_state |= SS_CANTRCVMORE;
	cv_broadcast(&so->so_state_cv);
}

/*
 * The caller has sent down a "request_prim" primitive and wants to wait for
 * an ack ("ack_prim") or an T_ERROR_ACK for it.
 * The specified "ack_prim" can be a T_OK_ACK.
 *
 * Assumes that all the TPI acks are M_PCPROTO messages.
 *
 * Note that the socket is single-threaded (using so_lock_single)
 * for all operations that generate TPI ack messages. Since
 * only TPI ack messages are M_PCPROTO we should never receive
 * anything except either the ack we are expecting or a T_ERROR_ACK
 * for the same primitive.
 */
int
sowaitprim(struct sonode *so, t_scalar_t request_prim, t_scalar_t ack_prim,
	    t_uscalar_t min_size, mblk_t **mpp, clock_t wait)
{
	mblk_t *mp;
	union T_primitives *tpr;
	int error;

	dprintso(so, 1, ("sowaitprim(%p, %d, %d, %d, %p, %lu)\n",
		so, request_prim, ack_prim, min_size, mpp, wait));

	ASSERT(MUTEX_HELD(&so->so_lock));

	error = sowaitack(so, &mp, wait);
	if (error)
		return (error);

	dprintso(so, 1, ("got msg %p\n", mp));
	if (mp->b_datap->db_type != M_PCPROTO ||
	    mp->b_wptr - mp->b_rptr < sizeof (tpr->type)) {
		freemsg(mp);
		eprintsoline(so, EPROTO);
		return (EPROTO);
	}
	tpr = (union T_primitives *)mp->b_rptr;
	/*
	 * Did we get the primitive that we were asking for?
	 * For T_OK_ACK we also check that it matches the request primitive.
	 */
	if (tpr->type == ack_prim &&
	    (ack_prim != T_OK_ACK ||
	    tpr->ok_ack.CORRECT_prim == request_prim)) {
		if (mp->b_wptr - mp->b_rptr >= (ssize_t)min_size) {
			/* Found what we are looking for */
			*mpp = mp;
			return (0);
		}
		/* Too short */
		freemsg(mp);
		eprintsoline(so, EPROTO);
		return (EPROTO);
	}

	if (tpr->type == T_ERROR_ACK &&
	    tpr->error_ack.ERROR_prim == request_prim) {
		/* Error to the primitive we were looking for */
		if (tpr->error_ack.TLI_error == TSYSERR) {
			error = tpr->error_ack.UNIX_error;
		} else {
			error = tlitosyserr(tpr->error_ack.TLI_error);
		}
		dprintso(so, 0, ("error_ack for %d: %d/%d ->%d\n",
			tpr->error_ack.ERROR_prim,
			tpr->error_ack.TLI_error,
			tpr->error_ack.UNIX_error,
			error));
		freemsg(mp);
		return (error);
	}
	/*
	 * Wrong primitive or T_ERROR_ACK for the wrong primitive
	 */
#ifdef DEBUG
	if (tpr->type == T_ERROR_ACK) {
		dprintso(so, 0, ("error_ack for %d: %d/%d\n",
			tpr->error_ack.ERROR_prim,
			tpr->error_ack.TLI_error,
			tpr->error_ack.UNIX_error));
	} else if (tpr->type == T_OK_ACK) {
		dprintso(so, 0, ("ok_ack for %d, expected %d for %d\n",
			tpr->ok_ack.CORRECT_prim,
			ack_prim, request_prim));
	} else {
		dprintso(so, 0,
			("unexpected primitive %d, expected %d for %d\n",
			tpr->type, ack_prim, request_prim));
	}
#endif /* DEBUG */

	freemsg(mp);
	eprintsoline(so, EPROTO);
	return (EPROTO);
}

/*
 * Wait for a T_OK_ACK for the specified primitive.
 */
int
sowaitokack(struct sonode *so, t_scalar_t request_prim)
{
	mblk_t *mp;
	int error;

	error = sowaitprim(so, request_prim, T_OK_ACK,
	    (t_uscalar_t)sizeof (struct T_ok_ack), &mp, 0);
	if (error)
		return (error);
	freemsg(mp);
	return (0);
}

/*
 * Queue a received TPI ack message on so_ack_mp.
 */
void
soqueueack(struct sonode *so, mblk_t *mp)
{
	if (mp->b_datap->db_type != M_PCPROTO) {
		cmn_err(CE_WARN,
		    "sockfs: received unexpected M_PROTO TPI ack. Prim %d\n",
		    *(t_scalar_t *)mp->b_rptr);
		freemsg(mp);
		return;
	}

	mutex_enter(&so->so_lock);
	if (so->so_ack_mp != NULL) {
		dprintso(so, 1, ("so_ack_mp already set\n"));
		freemsg(so->so_ack_mp);
		so->so_ack_mp = NULL;
	}
	so->so_ack_mp = mp;
	cv_broadcast(&so->so_ack_cv);
	mutex_exit(&so->so_lock);
}

/*
 * Wait for a TPI ack ignoring signals and errors.
 */
int
sowaitack(struct sonode *so, mblk_t **mpp, clock_t wait)
{
	ASSERT(MUTEX_HELD(&so->so_lock));

	while (so->so_ack_mp == NULL) {
#ifdef SOCK_TEST
		if (wait == 0 && sock_test_timelimit != 0)
			wait = sock_test_timelimit;
#endif
		if (wait != 0) {
			/*
			 * Only wait for the time limit.
			 */
			clock_t now;

			time_to_wait(&now, wait);
			if (cv_timedwait(&so->so_ack_cv, &so->so_lock,
			    now) == -1) {
				eprintsoline(so, ETIME);
				return (ETIME);
			}
		}
		else
			cv_wait(&so->so_ack_cv, &so->so_lock);
	}
	*mpp = so->so_ack_mp;
#ifdef DEBUG
	{
		union T_primitives *tpr;
		mblk_t *mp = *mpp;

		tpr = (union T_primitives *)mp->b_rptr;
		ASSERT(mp->b_datap->db_type == M_PCPROTO);
		ASSERT(tpr->type == T_OK_ACK ||
			tpr->type == T_ERROR_ACK ||
			tpr->type == T_BIND_ACK ||
			tpr->type == T_CAPABILITY_ACK ||
			tpr->type == T_INFO_ACK ||
			tpr->type == T_OPTMGMT_ACK);
	}
#endif /* DEBUG */
	so->so_ack_mp = NULL;
	return (0);
}

/*
 * Queue a received T_CONN_IND message on so_conn_ind_head/tail.
 */
void
soqueueconnind(struct sonode *so, mblk_t *mp)
{
	if (mp->b_datap->db_type != M_PROTO) {
		cmn_err(CE_WARN,
		    "sockfs: received unexpected M_PCPROTO T_CONN_IND\n");
		freemsg(mp);
		return;
	}

	mutex_enter(&so->so_lock);
	ASSERT(mp->b_next == NULL);
	if (so->so_conn_ind_head == NULL) {
		so->so_conn_ind_head = mp;
		so->so_state |= SS_HASCONNIND;
	} else {
		ASSERT(so->so_state & SS_HASCONNIND);
		ASSERT(so->so_conn_ind_tail->b_next == NULL);
		so->so_conn_ind_tail->b_next = mp;
	}
	so->so_conn_ind_tail = mp;
	/* Wakeup a single consumer of the T_CONN_IND */
	cv_signal(&so->so_connind_cv);
	mutex_exit(&so->so_lock);
}

/*
 * Wait for a T_CONN_IND.
 * Don't wait if nonblocking.
 * Accept signals and socket errors.
 */
int
sowaitconnind(struct sonode *so, int fmode, mblk_t **mpp)
{
	mblk_t *mp;
	int error = 0;

	ASSERT(MUTEX_NOT_HELD(&so->so_lock));
	mutex_enter(&so->so_lock);
	if (so->so_error) {
		error = sogeterr(so, 0);
		if (error) {
			mutex_exit(&so->so_lock);
			return (error);
		}
	}

	while (so->so_conn_ind_head == NULL) {
		if (fmode & (FNDELAY|FNONBLOCK)) {
			error = EWOULDBLOCK;
			goto done;
		}
		if (!cv_wait_sig_swap(&so->so_connind_cv, &so->so_lock)) {
			error = EINTR;
			goto done;
		}
	}
	mp = so->so_conn_ind_head;
	so->so_conn_ind_head = mp->b_next;
	mp->b_next = NULL;
	if (so->so_conn_ind_head == NULL) {
		ASSERT(so->so_conn_ind_tail == mp);
		so->so_conn_ind_tail = NULL;
		so->so_state &= ~SS_HASCONNIND;
	}
	*mpp = mp;
done:
	mutex_exit(&so->so_lock);
	return (error);
}

/*
 * Flush a T_CONN_IND matching the sequence number from the list.
 * Return zero if found; non-zero otherwise.
 * This is called very infrequently thus it is ok to do a linear search.
 */
int
soflushconnind(struct sonode *so, t_scalar_t seqno)
{
	mblk_t *prevmp, *mp;
	struct T_conn_ind *tci;

	mutex_enter(&so->so_lock);
	for (prevmp = NULL, mp = so->so_conn_ind_head; mp != NULL;
	    prevmp = mp, mp = mp->b_next) {
		tci = (struct T_conn_ind *)mp->b_rptr;
		if (tci->SEQ_number == seqno) {
			dprintso(so, 1,
				("t_discon_ind: found T_CONN_IND %d\n", seqno));
			/* Deleting last? */
			if (so->so_conn_ind_tail == mp) {
				so->so_conn_ind_tail = prevmp;
			}
			if (prevmp == NULL) {
				/* Deleting first */
				so->so_conn_ind_head = mp->b_next;
			} else {
				prevmp->b_next = mp->b_next;
			}
			mp->b_next = NULL;
			if (so->so_conn_ind_head == NULL) {
				ASSERT(so->so_conn_ind_tail == NULL);
				so->so_state &= ~SS_HASCONNIND;
			} else {
				ASSERT(so->so_conn_ind_tail != NULL);
			}
			mutex_exit(&so->so_lock);
			freemsg(mp);
			return (0);
		}
	}
	mutex_exit(&so->so_lock);
	dprintso(so, 1,	("t_discon_ind: NOT found T_CONN_IND %d\n", seqno));
	return (-1);
}

/*
 * Wait until the socket is connected or there is an error.
 * fmode should contain any nonblocking flags. nosig should be
 * set if the caller does not want the wait to be interrupted by a signal.
 */
int
sowaitconnected(struct sonode *so, int fmode, int nosig)
{
	int error;

	ASSERT(MUTEX_HELD(&so->so_lock));

	while ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) ==
		SS_ISCONNECTING && so->so_error == 0) {

		dprintso(so, 1, ("waiting for SS_ISCONNECTED on %p\n", so));
		if (fmode & (FNDELAY|FNONBLOCK))
			return (EINPROGRESS);

		if (nosig)
			cv_wait(&so->so_state_cv, &so->so_lock);
		else if (!cv_wait_sig_swap(&so->so_state_cv, &so->so_lock)) {
			/*
			 * Return EINTR and let the application use
			 * nonblocking techniques for detecting when
			 * the connection has been established.
			 */
			return (EINTR);
		}
		dprintso(so, 1, ("awoken on %p\n", so));
	}

	if (so->so_error != 0) {
		error = sogeterr(so, 0);
		ASSERT(error != 0);
		dprintso(so, 1, ("sowaitconnected: error %d\n", error));
		return (error);
	}
	if (!(so->so_state & SS_ISCONNECTED)) {
		/*
		 * Could have received a T_ORDREL_IND or a T_DISCON_IND with
		 * zero errno. Or another thread could have consumed so_error
		 * e.g. by calling read.
		 */
		error = ECONNREFUSED;
		dprintso(so, 1, ("sowaitconnected: error %d\n", error));
		return (error);
	}
	return (0);
}


/*
 * Handle the signal generation aspect of urgent data.
 */
static void
so_oob_sig(struct sonode *so, int extrasig,
    strsigset_t *signals, strpollset_t *pollwakeups)
{
	ASSERT(MUTEX_HELD(&so->so_lock));

	ASSERT(so_verify_oobstate(so));
	ASSERT(so->so_oobsigcnt >= so->so_oobcnt);
	if (so->so_oobsigcnt > so->so_oobcnt) {
		/*
		 * Signal has already been generated once for this
		 * urgent "event". However, since TCP can receive updated
		 * urgent pointers we still generate a signal.
		 */
		ASSERT(so->so_state & SS_OOBPEND);
		if (extrasig) {
			*signals |= S_RDBAND;
			*pollwakeups |= POLLRDBAND;
		}
		return;
	}

	so->so_oobsigcnt++;
	ASSERT(so->so_oobsigcnt > 0);	/* Wraparound */
	ASSERT(so->so_oobsigcnt > so->so_oobcnt);

	/*
	 * Record (for select/poll) that urgent data is pending.
	 */
	so->so_state |= SS_OOBPEND;
	/*
	 * New urgent data on the way so forget about any old
	 * urgent data.
	 */
	so->so_state &= ~(SS_HAVEOOBDATA|SS_HADOOBDATA);
	if (so->so_oobmsg != NULL) {
		dprintso(so, 1, ("sock: discarding old oob\n"));
		freemsg(so->so_oobmsg);
		so->so_oobmsg = NULL;
	}
	*signals |= S_RDBAND;
	*pollwakeups |= POLLRDBAND;
	ASSERT(so_verify_oobstate(so));
}

/*
 * Handle the processing of the T_EXDATA_IND with urgent data.
 * Returns the T_EXDATA_IND if it should be queued on the read queue.
 */
/* ARGSUSED2 */
static mblk_t *
so_oob_exdata(struct sonode *so, mblk_t *mp,
	strsigset_t *signals, strpollset_t *pollwakeups)
{
	ASSERT(MUTEX_HELD(&so->so_lock));

	ASSERT(so_verify_oobstate(so));

	ASSERT(so->so_oobsigcnt > so->so_oobcnt);

	so->so_oobcnt++;
	ASSERT(so->so_oobcnt > 0);	/* wraparound? */
	ASSERT(so->so_oobsigcnt >= so->so_oobcnt);

	/*
	 * Set MSGMARK for SIOCATMARK.
	 */
	mp->b_flag |= MSGMARK;

	ASSERT(so_verify_oobstate(so));
	return (mp);
}

/*
 * Handle the processing of the actual urgent data.
 * Returns the data mblk if it should be queued on the read queue.
 */
static mblk_t *
so_oob_data(struct sonode *so, mblk_t *mp,
	strsigset_t *signals, strpollset_t *pollwakeups)
{
	ASSERT(MUTEX_HELD(&so->so_lock));

	ASSERT(so_verify_oobstate(so));

	ASSERT(so->so_oobsigcnt >= so->so_oobcnt);
	ASSERT(mp != NULL);
	/*
	 * For OOBINLINE we keep the data in the T_EXDATA_IND.
	 * Otherwise we store it in so_oobmsg.
	 */
	ASSERT(so->so_oobmsg == NULL);
	if (so->so_options & SO_OOBINLINE) {
		*pollwakeups |= POLLIN | POLLRDNORM | POLLRDBAND;
		*signals |= S_INPUT | S_RDNORM;
	} else {
		*pollwakeups |= POLLRDBAND;
		so->so_state |= SS_HAVEOOBDATA;
		so->so_oobmsg = mp;
		mp = NULL;
	}
	ASSERT(so_verify_oobstate(so));
	return (mp);
}



/*
 * This routine is registered with the stream head to receive M_PROTO
 * and M_PCPROTO messages.
 *
 * Returns NULL if the message was consumed.
 * Returns an mblk to make that mblk be processed (and queued) by the stream
 * head.
 *
 * Sets the return parameters (*wakeups, *firstmsgsigs, *allmsgsigs, and
 * *pollwakeups) for the stream head to take action on. Note that since
 * sockets always deliver SIGIO for every new piece of data this routine
 * never sets *firstmsgsigs; any signals are returned in *allmsgsigs.
 *
 * This routine handles all data related TPI messages independent of
 * the type of the socket i.e. it doesn't care if T_UNITDATA_IND message
 * arrive on a SOCK_STREAM.
 */
static mblk_t *
strsock_proto(vnode_t *vp, mblk_t *mp,
		strwakeup_t *wakeups, strsigset_t *firstmsgsigs,
		strsigset_t *allmsgsigs, strpollset_t *pollwakeups)
{
	union T_primitives *tpr;
	struct sonode *so;

	so = VTOSO(vp);

	dprintso(so, 1, ("strsock_proto(%p, %p)\n", vp, mp));

	/* Set default return values */
	*firstmsgsigs = *wakeups = *allmsgsigs = *pollwakeups = 0;

	ASSERT(mp->b_datap->db_type == M_PROTO ||
	    mp->b_datap->db_type == M_PCPROTO);

	if (mp->b_wptr - mp->b_rptr < sizeof (tpr->type)) {
		/* The message is too short to even contain the primitive */
		cmn_err(CE_WARN,
		    "sockfs: Too short TPI message received. Len = %ld\n",
		    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
		freemsg(mp);
		return (NULL);
	}
	if (!__TPI_PRIM_ISALIGNED(mp->b_rptr)) {
		/* The read pointer is not aligned correctly for TPI */
		cmn_err(CE_WARN,
		    "sockfs: Unaligned TPI message received. rptr = %p\n",
		    (void *)mp->b_rptr);
		freemsg(mp);
		return (NULL);
	}
	tpr = (union T_primitives *)mp->b_rptr;
	dprintso(so, 1, ("strsock_proto: primitive %d\n", tpr->type));

	switch (tpr->type) {

	case T_DATA_IND:
		if (mp->b_wptr - mp->b_rptr < sizeof (struct T_data_ind)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_DATA_IND. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}
		/*
		 * Ignore zero-length T_DATA_IND messages. These might be
		 * generated by some transports.
		 * This is needed to prevent read (which skips the M_PROTO
		 * part) to unexpectedly return 0 (or return EWOULDBLOCK
		 * on a non-blocking socket after select/poll has indicated
		 * that data is available).
		 */
		if (msgdsize(mp->b_cont) == 0) {
			dprintso(so, 0,
			    ("strsock_proto: zero length T_DATA_IND\n"));
			freemsg(mp);
			return (NULL);
		}
		*allmsgsigs = S_INPUT | S_RDNORM;
		*pollwakeups = POLLIN | POLLRDNORM;
		*wakeups = RSLEEP;
		return (mp);

	case T_UNITDATA_IND: {
		struct T_unitdata_ind	*tudi = &tpr->unitdata_ind;
		void			*addr;
		t_uscalar_t		addrlen;

		if (mp->b_wptr - mp->b_rptr < sizeof (struct T_unitdata_ind)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_UNITDATA_IND. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}

		/* Is this is not a connected datagram socket? */
		if ((so->so_mode & SM_CONNREQUIRED) ||
		    !(so->so_state & SS_ISCONNECTED)) {
			/*
			 * Not a connected datagram socket. Look for
			 * the SO_UNIX_CLOSE option. If such an option is found
			 * discard the message (since it has no meaning
			 * unless connected).
			 */
			if (so->so_family == AF_UNIX && msgdsize(mp) == 0 &&
			    tudi->OPT_length != 0) {
				void *opt;
				t_uscalar_t optlen = tudi->OPT_length;

				opt = sogetoff(mp, tudi->OPT_offset,
					optlen, __TPI_ALIGN_SIZE);
				if (opt == NULL) {
					/* The len/off falls outside mp */
					freemsg(mp);
					mutex_enter(&so->so_lock);
					soseterror(so, EPROTO);
					mutex_exit(&so->so_lock);
					cmn_err(CE_WARN,
					    "sockfs: T_unidata_ind with "
					    "invalid optlen/offset %u/%d\n",
					    optlen, tudi->OPT_offset);
					return (NULL);
				}
				if (so_getopt_unix_close(opt, optlen)) {
					freemsg(mp);
					return (NULL);
				}
			}
			*allmsgsigs = S_INPUT | S_RDNORM;
			*pollwakeups = POLLIN | POLLRDNORM;
			*wakeups = RSLEEP;
#ifdef C2_AUDIT
			if (audit_active)
				audit_sock(T_UNITDATA_IND, strvp2wq(vp),
					mp, 0);
#endif /* C2_AUDIT */
			return (mp);
		}

		/*
		 * A connect datagram socket. For AF_INET{,6} we verify that
		 * the source address matches the "connected to" address.
		 * The semantics of AF_UNIX sockets is to not verify
		 * the source address.
		 * Note that this source address verification is transport
		 * specific. Thus the real fix would be to extent TPI
		 * to allow T_CONN_REQ messages to be send to connectionless
		 * transport providers and always let the transport provider
		 * do whatever filtering is needed.
		 *
		 * The verification/filtering semantics for transports
		 * other than AF_INET and AF_UNIX are unknown. The choice
		 * would be to either filter using bcmp or let all messages
		 * get through. This code does not filter other address
		 * families since this at least allows the application to
		 * work around any missing filtering.
		 *
		 * XXX Should we move filtering to UDP/ICMP???
		 * That would require passing e.g. a T_DISCON_REQ to UDP
		 * when the socket becomes unconnected.
		 * XXX AF_INET{,6}/SOCK_RAW shouldn't inspect the port numbers.
		 */
		addrlen = tudi->SRC_length;
		/*
		 * The alignment restriction is really to strict but
		 * we want enough alignment to inspect the fields of
		 * a sockaddr_in.
		 */
		addr = sogetoff(mp, tudi->SRC_offset, addrlen,
				__TPI_ALIGN_SIZE);
		if (addr == NULL) {
			freemsg(mp);
			mutex_enter(&so->so_lock);
			soseterror(so, EPROTO);
			mutex_exit(&so->so_lock);
			cmn_err(CE_WARN,
			    "sockfs: T_unidata_ind with invalid "
			    "addrlen/offset %u/%d\n",
			    addrlen, tudi->SRC_offset);
			return (NULL);
		}

		if (so->so_family == AF_INET) {
			/*
			 * For AF_INET we allow wildcarding both sin_addr
			 * and sin_port.
			 */
			struct sockaddr_in *faddr, *sin;

			/* Prevent so_faddr_sa from changing while accessed */
			mutex_enter(&so->so_lock);
			ASSERT(so->so_faddr_len ==
				(socklen_t)sizeof (struct sockaddr_in));
			faddr = (struct sockaddr_in *)so->so_faddr_sa;
			sin = (struct sockaddr_in *)addr;
			if (addrlen !=
				(t_uscalar_t)sizeof (struct sockaddr_in) ||
			    (sin->sin_addr.s_addr != faddr->sin_addr.s_addr &&
			    faddr->sin_addr.s_addr != INADDR_ANY) ||
			    (sin->sin_port != faddr->sin_port &&
			    faddr->sin_port != 0)) {
#ifdef DEBUG
				dprintso(so, 0,
					("sockfs: T_UNITDATA_IND mismatch: %s",
					pr_addr(so->so_family,
						(struct sockaddr *)addr,
						addrlen)));
				dprintso(so, 0, (" - %s\n",
					pr_addr(so->so_family, so->so_faddr_sa,
					    (t_uscalar_t)so->so_faddr_len)));
#endif /* DEBUG */
				mutex_exit(&so->so_lock);
				freemsg(mp);
				return (NULL);
			}
			mutex_exit(&so->so_lock);
		} else if (so->so_family == AF_INET6) {
			/*
			 * For AF_INET6 we allow wildcarding both sin6_addr
			 * and sin6_port.
			 */
			struct sockaddr_in6 *faddr6, *sin6;
			static struct in6_addr zeroes; /* inits to all zeros */

			/* Prevent so_faddr_sa from changing while accessed */
			mutex_enter(&so->so_lock);
			ASSERT(so->so_faddr_len ==
			    (socklen_t)sizeof (struct sockaddr_in6));
			faddr6 = (struct sockaddr_in6 *)so->so_faddr_sa;
			sin6 = (struct sockaddr_in6 *)addr;
			/* XXX could we get a mapped address ::ffff:0.0.0.0 ? */
			if (addrlen !=
			    (t_uscalar_t)sizeof (struct sockaddr_in6) ||
			    (!IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
				&faddr6->sin6_addr) &&
			    !IN6_ARE_ADDR_EQUAL(&faddr6->sin6_addr, &zeroes)) ||
			    (sin6->sin6_port != faddr6->sin6_port &&
			    faddr6->sin6_port != 0)) {
#ifdef DEBUG
				dprintso(so, 0,
				    ("sockfs: T_UNITDATA_IND mismatch: %s",
					pr_addr(so->so_family,
					    (struct sockaddr *)addr,
					    addrlen)));
				dprintso(so, 0, (" - %s\n",
				    pr_addr(so->so_family, so->so_faddr_sa,
					(t_uscalar_t)so->so_faddr_len)));
#endif /* DEBUG */
				mutex_exit(&so->so_lock);
				freemsg(mp);
				return (NULL);
			}
			mutex_exit(&so->so_lock);
		} else if (so->so_family == AF_UNIX &&
		    msgdsize(mp->b_cont) == 0 &&
		    tudi->OPT_length != 0) {
			/*
			 * Attempt to extract AF_UNIX
			 * SO_UNIX_CLOSE indication from options.
			 */
			void *opt;
			t_uscalar_t optlen = tudi->OPT_length;

			opt = sogetoff(mp, tudi->OPT_offset,
				optlen, __TPI_ALIGN_SIZE);
			if (opt == NULL) {
				/* The len/off falls outside mp */
				freemsg(mp);
				mutex_enter(&so->so_lock);
				soseterror(so, EPROTO);
				mutex_exit(&so->so_lock);
				cmn_err(CE_WARN,
				    "sockfs: T_unidata_ind with invalid "
				    "optlen/offset %u/%d\n",
				    optlen, tudi->OPT_offset);
				return (NULL);
			}
			/*
			 * If we received a unix close indication mark the
			 * socket and discard this message.
			 */
			if (so_getopt_unix_close(opt, optlen)) {
				mutex_enter(&so->so_lock);
				sobreakconn(so, ECONNRESET);
				mutex_exit(&so->so_lock);
				strsetrerror(SOTOV(so), 0, 0, sogetrderr);
				freemsg(mp);
				*pollwakeups = POLLIN | POLLRDNORM;
				*allmsgsigs = S_INPUT | S_RDNORM;
				*wakeups = RSLEEP;
				return (NULL);
			}
		}
		*allmsgsigs = S_INPUT | S_RDNORM;
		*pollwakeups = POLLIN | POLLRDNORM;
		*wakeups = RSLEEP;
		return (mp);
	}

	case T_OPTDATA_IND: {
		struct T_optdata_ind	*tdi = &tpr->optdata_ind;

		if (mp->b_wptr - mp->b_rptr < sizeof (struct T_optdata_ind)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_OPTDATA_IND. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}
		/*
		 * Allow zero-length messages carrying options.
		 * This is used when carrying the SO_UNIX_CLOSE option.
		 */
		if (so->so_family == AF_UNIX && msgdsize(mp->b_cont) == 0 &&
		    tdi->OPT_length != 0) {
			/*
			 * Attempt to extract AF_UNIX close indication
			 * from the options. Ignore any other options -
			 * those are handled once the message is removed
			 * from the queue.
			 * The close indication message should not carry data.
			 */
			void *opt;
			t_uscalar_t optlen = tdi->OPT_length;

			opt = sogetoff(mp, tdi->OPT_offset,
				optlen, __TPI_ALIGN_SIZE);
			if (opt == NULL) {
				/* The len/off falls outside mp */
				freemsg(mp);
				mutex_enter(&so->so_lock);
				soseterror(so, EPROTO);
				mutex_exit(&so->so_lock);
				cmn_err(CE_WARN,
				    "sockfs: T_optdata_ind with invalid "
				    "optlen/offset %u/%d\n",
				    optlen, tdi->OPT_offset);
				return (NULL);
			}
			/*
			 * If we received a close indication mark the
			 * socket and discard this message.
			 */
			if (so_getopt_unix_close(opt, optlen)) {
				mutex_enter(&so->so_lock);
				socantsendmore(so);
				mutex_exit(&so->so_lock);
				strsetwerror(SOTOV(so), 0, 0, sogetwrerr);
				freemsg(mp);
				return (NULL);
			}
		}
		*allmsgsigs = S_INPUT | S_RDNORM;
		*pollwakeups = POLLIN | POLLRDNORM;
		*wakeups = RSLEEP;
		return (mp);
	}

	case T_EXDATA_IND: {
		mblk_t		*mctl, *mdata;

		if (mp->b_wptr - mp->b_rptr < sizeof (struct T_exdata_ind)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_EXDATA_IND. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}
		/*
		 * Ignore zero-length T_EXDATA_IND messages. These might be
		 * generated by some transports.
		 *
		 * This is needed to prevent read (which skips the M_PROTO
		 * part) to unexpectedly return 0 (or return EWOULDBLOCK
		 * on a non-blocking socket after select/poll has indicated
		 * that data is available).
		 */
		dprintso(so, 1,
			("T_EXDATA_IND(%p): counts %d/%d state %s\n",
			vp, so->so_oobsigcnt, so->so_oobcnt,
			pr_state(so->so_state, so->so_mode)));

		if (msgdsize(mp->b_cont) == 0) {
			dprintso(so, 0,
				("strsock_proto: zero length T_EXDATA_IND\n"));
			freemsg(mp);
			return (NULL);
		}

		/*
		 * Split into the T_EXDATA_IND and the M_DATA part.
		 * We process these three pieces separately:
		 *	signal generation
		 *	handling T_EXDATA_IND
		 *	handling M_DATA component
		 */
		mctl = mp;
		mdata = mctl->b_cont;
		mctl->b_cont = NULL;
		mutex_enter(&so->so_lock);
		so_oob_sig(so, 0, allmsgsigs, pollwakeups);
		mctl = so_oob_exdata(so, mctl, allmsgsigs, pollwakeups);
		mdata = so_oob_data(so, mdata, allmsgsigs, pollwakeups);

		/*
		 * Pass the T_EXDATA_IND and the M_DATA back separately
		 * by using b_next linkage. (The stream head will queue any
		 * b_next linked messages separately.) This is needed
		 * since MSGMARK applies to the last by of the message
		 * hence we can not have any M_DATA component attached
		 * to the marked T_EXDATA_IND. Note that the stream head
		 * will not consolidate M_DATA messages onto an MSGMARK'ed
		 * message in order to preserve the constraint that
		 * the T_EXDATA_IND always is a separate message.
		 */
		ASSERT(mctl != NULL);
		mctl->b_next = mdata;
		mp = mctl;
#ifdef DEBUG
		if (mdata == NULL) {
			dprintso(so, 1,
				("after outofline T_EXDATA_IND(%p): "
				"counts %d/%d  poll 0x%x sig 0x%x state %s\n",
				vp, so->so_oobsigcnt,
				so->so_oobcnt, *pollwakeups, *allmsgsigs,
				pr_state(so->so_state, so->so_mode)));
		} else {
			dprintso(so, 1,
				("after inline T_EXDATA_IND(%p): "
				"counts %d/%d  poll 0x%x sig 0x%x state %s\n",
				vp, so->so_oobsigcnt,
				so->so_oobcnt, *pollwakeups, *allmsgsigs,
				pr_state(so->so_state, so->so_mode)));
		}
#endif /* DEBUG */
		mutex_exit(&so->so_lock);
		*wakeups = RSLEEP;
		return (mp);
	}

	case T_CONN_CON: {
		struct T_conn_con	*conn_con;
		void			*addr;
		t_uscalar_t		addrlen;

		/*
		 * Verify the state, update the state to ISCONNECTED,
		 * record the potentially new address in the message,
		 * and drop the message.
		 */
		if (mp->b_wptr - mp->b_rptr < sizeof (struct T_conn_con)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_CONN_CON. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}

		mutex_enter(&so->so_lock);
		if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) !=
		    SS_ISCONNECTING) {
			mutex_exit(&so->so_lock);
			dprintso(so, 1,
				("T_CONN_CON: state %x\n", so->so_state));
			freemsg(mp);
			return (NULL);
		}

		conn_con = &tpr->conn_con;
		addrlen = conn_con->RES_length;
		/*
		 * Allow the address to be of different size than sent down
		 * in the T_CONN_REQ as long as it doesn't exceed the maxlen.
		 * For AF_UNIX require the identical length.
		 */
		if (so->so_family == AF_UNIX ?
		    addrlen != (t_uscalar_t)sizeof (so->so_ux_laddr) :
		    addrlen > (t_uscalar_t)so->so_faddr_maxlen) {
			cmn_err(CE_WARN,
			    "sockfs: T_conn_con with different "
			    "length %u/%d\n",
			    addrlen, conn_con->RES_length);
			soisdisconnected(so, EPROTO);
			mutex_exit(&so->so_lock);
			strsetrerror(SOTOV(so), 0, 0, sogetrderr);
			strsetwerror(SOTOV(so), 0, 0, sogetwrerr);
			strseteof(SOTOV(so), 1);
			freemsg(mp);
			/*
			 * strseteof takes care of read side wakeups,
			 * pollwakeups, and signals.
			 */
			*wakeups = WSLEEP;
			*allmsgsigs = S_OUTPUT;
			*pollwakeups = POLLOUT;
			return (NULL);
		}
		addr = sogetoff(mp, conn_con->RES_offset, addrlen, 1);
		if (addr == NULL) {
			cmn_err(CE_WARN,
			    "sockfs: T_conn_con with invalid "
			    "addrlen/offset %u/%d\n",
			    addrlen, conn_con->RES_offset);
			mutex_exit(&so->so_lock);
			strsetrerror(SOTOV(so), 0, 0, sogetrderr);
			strsetwerror(SOTOV(so), 0, 0, sogetwrerr);
			strseteof(SOTOV(so), 1);
			freemsg(mp);
			/*
			 * strseteof takes care of read side wakeups,
			 * pollwakeups, and signals.
			 */
			*wakeups = WSLEEP;
			*allmsgsigs = S_OUTPUT;
			*pollwakeups = POLLOUT;
			return (NULL);
		}

		/*
		 * Save for getpeername.
		 */
		if (so->so_family != AF_UNIX) {
			so->so_faddr_len = (socklen_t)addrlen;
			ASSERT(so->so_faddr_len <= so->so_faddr_maxlen);
			bcopy(addr, so->so_faddr_sa, addrlen);
		}
		/* Wakeup anybody sleeping in sowaitconnected */
		soisconnected(so);
		mutex_exit(&so->so_lock);

		/*
		 * The socket is now available for sending data.
		 */
		*wakeups = WSLEEP;
		*allmsgsigs = S_OUTPUT;
		*pollwakeups = POLLOUT;
		freemsg(mp);
		return (NULL);
	}

	case T_CONN_IND:
		/*
		 * Verify the min size and queue the message on
		 * the so_conn_ind_head/tail list.
		 */
		if (mp->b_wptr - mp->b_rptr < sizeof (struct T_conn_ind)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_CONN_IND. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}

#ifdef C2_AUDIT
		if (audit_active)
			audit_sock(T_CONN_IND, strvp2wq(vp), mp, 0);
#endif /* C2_AUDIT */
		if (!(so->so_state & SS_ACCEPTCONN)) {
			cmn_err(CE_WARN,
			    "sockfs: T_conn_ind on non-listening socket\n");
			freemsg(mp);
			return (NULL);
		}
		soqueueconnind(so, mp);
		*allmsgsigs = S_INPUT | S_RDNORM;
		*pollwakeups = POLLIN | POLLRDNORM;
		*wakeups = RSLEEP;
		return (NULL);

	case T_ORDREL_IND:
		if (mp->b_wptr - mp->b_rptr < sizeof (struct T_ordrel_ind)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_ORDREL_IND. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}

		/*
		 * Some providers send this when not fully connected.
		 * SunLink X.25 needs to retrieve disconnect reason after
		 * disconnect for compatibility. It uses T_ORDREL_IND
		 * instead of T_DISCON_IND so that it may use the
		 * endpoint after a connect failure to retrieve the
		 * reason using an ioctl. Thus we explicitly clear
		 * SS_ISCONNECTING here for SunLink X.25.
		 * This is a needed TPI violation.
		 */
		mutex_enter(&so->so_lock);
		so->so_state &= ~SS_ISCONNECTING;
		socantrcvmore(so);
		mutex_exit(&so->so_lock);
		strseteof(SOTOV(so), 1);
		/*
		 * strseteof takes care of read side wakeups,
		 * pollwakeups, and signals.
		 */
		freemsg(mp);
		return (NULL);

	case T_DISCON_IND:
		if (mp->b_wptr - mp->b_rptr < sizeof (struct T_discon_ind)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_DISCON_IND. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}
		if (so->so_state & SS_ACCEPTCONN) {
			/*
			 * This is a listener. Look for a queued T_CONN_IND
			 * with a matching sequence number and remove it
			 * from the list.
			 * It is normal to not find the sequence number since
			 * the soaccept might have already dequeued it
			 * (in which case the T_CONN_RES will fail with
			 * TBADSEQ).
			 */
			(void) soflushconnind(so, tpr->discon_ind.SEQ_number);
			freemsg(mp);
			return (0);
		}

		/*
		 * Not a listener
		 *
		 * If SS_CANTRCVMORE for AF_UNIX ignore the discon_reason.
		 * Such a discon_ind appears when the peer has first done
		 * a shutdown() followed by a close() in which case we just
		 * want to record socantsendmore.
		 * In this case sockfs first receives a T_ORDREL_IND followed
		 * by a T_DISCON_IND.
		 * Note that for other transports (e.g. TCP) we need to handle
		 * the discon_ind in this case since it signals an error.
		 */
		mutex_enter(&so->so_lock);
		if ((so->so_state & SS_CANTRCVMORE) &&
		    (so->so_family == AF_UNIX)) {
			socantsendmore(so);
			mutex_exit(&so->so_lock);
			strsetwerror(SOTOV(so), 0, 0, sogetwrerr);
		} else {
			/*
			 * This assumes that the name space for ERROR_type
			 * is the errno name space.
			 */
			soisdisconnected(so, tpr->discon_ind.DISCON_reason);
			mutex_exit(&so->so_lock);
			/*
			 * Unbind with the transport without
			 * blocking. If we've already received a T_DISCON_IND
			 * this routine will do nothing.
			 */
			sounbind_nonblock(so);

			if (tpr->discon_ind.DISCON_reason != 0)
				strsetrerror(SOTOV(so), 0, 0, sogetrderr);
			strsetwerror(SOTOV(so), 0, 0, sogetwrerr);
			strseteof(SOTOV(so), 1);
			/*
			 * strseteof takes care of read side wakeups,
			 * pollwakeups, and signals.
			 */
		}
		dprintso(so, 1, ("T_DISCON_IND: error %d\n", so->so_error));
		freemsg(mp);

		*wakeups = WSLEEP;
		*allmsgsigs = S_OUTPUT;
		*pollwakeups = POLLOUT;
		return (NULL);

	case T_UDERROR_IND: {
		struct T_uderror_ind	*tudi = &tpr->uderror_ind;
		void			*addr;
		t_uscalar_t		addrlen;
		int			error;

		dprintso(so, 0,
			("T_UDERROR_IND: error %d\n", tudi->ERROR_type));

		if (mp->b_wptr - mp->b_rptr < sizeof (struct T_uderror_ind)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_UDERROR_IND. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}
		/* Ignore on connection-oriented transports */
		if (so->so_mode & SM_CONNREQUIRED) {
			freemsg(mp);
			eprintsoline(so, 0);
			cmn_err(CE_WARN,
			    "sockfs: T_uderror_ind on connection-oriented "
			    "transport\n");
			return (NULL);
		}
		addrlen = tudi->DEST_length;
		addr = sogetoff(mp, tudi->DEST_offset, addrlen, 1);
		if (addr == NULL) {
			cmn_err(CE_WARN,
			    "sockfs: T_uderror_ind with invalid "
			    "addrlen/offset %u/%d\n",
			    addrlen, tudi->DEST_offset);
			freemsg(mp);
			return (NULL);
		}

		/* Verify source address for connected socket. */
		mutex_enter(&so->so_lock);
		if (so->so_state & SS_ISCONNECTED) {
			void *faddr;
			t_uscalar_t faddr_len;
			boolean_t match = B_FALSE;

			switch (so->so_family) {
			case AF_INET: {
				/* Compare just IP address and port */
				struct sockaddr_in *sin1, *sin2;

				sin1 = (struct sockaddr_in *)so->so_faddr_sa;
				sin2 = (struct sockaddr_in *)addr;
				if (addrlen == sizeof (struct sockaddr_in) &&
				    sin1->sin_port == sin2->sin_port &&
				    sin1->sin_addr.s_addr ==
				    sin2->sin_addr.s_addr)
					match = B_TRUE;
				break;
			}
			case AF_INET6: {
				/* Compare just IP address and port. Not flow */
				struct sockaddr_in6 *sin1, *sin2;

				sin1 = (struct sockaddr_in6 *)so->so_faddr_sa;
				sin2 = (struct sockaddr_in6 *)addr;
				if (addrlen == sizeof (struct sockaddr_in6) &&
				    sin1->sin6_port == sin2->sin6_port &&
				    IN6_ARE_ADDR_EQUAL(&sin1->sin6_addr,
					&sin2->sin6_addr))
					match = B_TRUE;
				break;
			}
			case AF_UNIX:
				faddr = &so->so_ux_faddr;
				faddr_len =
					(t_uscalar_t)sizeof (so->so_ux_faddr);
				if (faddr_len == addrlen &&
				    bcmp(addr, faddr, addrlen) == 0)
					match = B_TRUE;
				break;
			default:
				faddr = so->so_faddr_sa;
				faddr_len = (t_uscalar_t)so->so_faddr_len;
				if (faddr_len == addrlen &&
				    bcmp(addr, faddr, addrlen) == 0)
					match = B_TRUE;
				break;
			}

			if (!match) {
#ifdef DEBUG
				dprintso(so, 0,
					("sockfs: T_UDERR_IND mismatch: %s - ",
					pr_addr(so->so_family,
						(struct sockaddr *)addr,
						addrlen)));
				dprintso(so, 0, ("%s\n",
					pr_addr(so->so_family, so->so_faddr_sa,
						so->so_faddr_len)));
#endif /* DEBUG */
				mutex_exit(&so->so_lock);
				freemsg(mp);
				return (NULL);
			}
			/*
			 * Make the write error nonpersistent. If the error
			 * is zero we use ECONNRESET.
			 * This assumes that the name space for ERROR_type
			 * is the errno name space.
			 */
			if (tudi->ERROR_type != 0)
				error = tudi->ERROR_type;
			else
				error = ECONNRESET;

			soseterror(so, error);
			mutex_exit(&so->so_lock);
			strsetrerror(SOTOV(so), 0, 0, sogetrderr);
			strsetwerror(SOTOV(so), 0, 0, sogetwrerr);
			*wakeups = RSLEEP | WSLEEP;
			*allmsgsigs = S_INPUT | S_RDNORM | S_OUTPUT;
			*pollwakeups = POLLIN | POLLRDNORM | POLLOUT;
			freemsg(mp);
			return (NULL);
		}
		/*
		 * If the application asked for delayed errors
		 * record the T_UDERROR_IND so_eaddr_mp and the reason in
		 * so_delayed_error for delayed error posting. If the reason
		 * is zero use ECONNRESET.
		 * Note that delayed error indications do not make sense for
		 * AF_UNIX sockets since sendto checks that the destination
		 * address is valid at the time of the sendto.
		 */
		if (!(so->so_options & SO_DGRAM_ERRIND)) {
			mutex_exit(&so->so_lock);
			freemsg(mp);
			return (NULL);
		}
		if (so->so_eaddr_mp != NULL)
			freemsg(so->so_eaddr_mp);

		so->so_eaddr_mp = mp;
		if (tudi->ERROR_type != 0)
			error = tudi->ERROR_type;
		else
			error = ECONNRESET;
		so->so_delayed_error = (ushort_t)error;
		mutex_exit(&so->so_lock);
		return (NULL);
	}

	case T_ERROR_ACK:
		dprintso(so, 0,
			("strsock_proto: T_ERROR_ACK for %d, error %d/%d\n",
			tpr->error_ack.ERROR_prim,
			tpr->error_ack.TLI_error,
			tpr->error_ack.UNIX_error));

		if (mp->b_wptr - mp->b_rptr < sizeof (struct T_error_ack)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_ERROR_ACK. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}
		/*
		 * Check if we were waiting for the async message
		 */
		mutex_enter(&so->so_lock);
		if ((so->so_state & SS_WUNBIND) &&
		    tpr->error_ack.ERROR_prim == T_UNBIND_REQ) {
			so->so_state &= ~SS_WUNBIND;
			mutex_exit(&so->so_lock);
			freemsg(mp);
			return (NULL);
		}
		mutex_exit(&so->so_lock);
		soqueueack(so, mp);
		return (NULL);

	case T_OK_ACK:
		if (mp->b_wptr - mp->b_rptr < sizeof (struct T_ok_ack)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_OK_ACK. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}
		/*
		 * Check if we were waiting for the async message
		 */
		mutex_enter(&so->so_lock);
		if ((so->so_state & SS_WUNBIND) &&
		    tpr->ok_ack.CORRECT_prim == T_UNBIND_REQ) {
			dprintso(so, 1,
				("strsock_proto: T_OK_ACK async unbind\n"));
			so->so_state &= ~SS_WUNBIND;
			mutex_exit(&so->so_lock);
			freemsg(mp);
			return (NULL);
		}
		mutex_exit(&so->so_lock);
		soqueueack(so, mp);
		return (NULL);

	case T_INFO_ACK:
		if (mp->b_wptr - mp->b_rptr < sizeof (struct T_info_ack)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_INFO_ACK. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}
		soqueueack(so, mp);
		return (NULL);

	case T_CAPABILITY_ACK:
		/*
		 * A T_capability_ack need only be large enough to hold
		 * the PRIM_type and CAP_bits1 fields; checking for anything
		 * larger might reject a correct response from an older
		 * provider.
		 */
		if (mp->b_wptr - mp->b_rptr < 2 * sizeof (t_uscalar_t)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_CAPABILITY_ACK. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}
		soqueueack(so, mp);
		return (NULL);

	case T_BIND_ACK:
		if (mp->b_wptr - mp->b_rptr < sizeof (struct T_bind_ack)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_BIND_ACK. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}
		soqueueack(so, mp);
		return (NULL);

	case T_OPTMGMT_ACK:
		if (mp->b_wptr - mp->b_rptr < sizeof (struct T_optmgmt_ack)) {
			cmn_err(CE_WARN,
			    "sockfs: Too short T_OPTMGMT_ACK. Len = %ld\n",
			    (ptrdiff_t)(mp->b_wptr - mp->b_rptr));
			freemsg(mp);
			return (NULL);
		}
		soqueueack(so, mp);
		return (NULL);
	default:
#ifdef DEBUG
		cmn_err(CE_WARN,
			"sockfs: unknown TPI primitive %d received\n",
			tpr->type);
#endif /* DEBUG */
		freemsg(mp);
		return (NULL);
	}
}

/*
 * This routine is registered with the stream head to receive other
 * (non-data, and non-proto) messages.
 *
 * Returns NULL if the message was consumed.
 * Returns an mblk to make that mblk be processed by the stream head.
 *
 * Sets the return parameters (*wakeups, *firstmsgsigs, *allmsgsigs, and
 * *pollwakeups) for the stream head to take action on.
 */
static mblk_t *
strsock_misc(vnode_t *vp, mblk_t *mp,
		strwakeup_t *wakeups, strsigset_t *firstmsgsigs,
		strsigset_t *allmsgsigs, strpollset_t *pollwakeups)
{
	struct sonode *so;

	so = VTOSO(vp);

	dprintso(so, 1, ("strsock_misc(%p, %p, 0x%x)\n",
			vp, mp, mp->b_datap->db_type));

	/* Set default return values */
	*wakeups = *allmsgsigs = *firstmsgsigs = *pollwakeups = 0;

	switch (mp->b_datap->db_type) {
	case M_PCSIG:
		/*
		 * This assumes that an M_PCSIG for the urgent data arrives
		 * before the corresponding T_EXDATA_IND.
		 *
		 * Note: Just like in SunOS 4.X and 4.4BSD a poll will be
		 * awoken before the urgent data shows up.
		 * For OOBINLINE this can result in select returning
		 * only exceptions as opposed to except|read.
		 */
		if (*mp->b_rptr == SIGURG) {
			mutex_enter(&so->so_lock);
			dprintso(so, 1,
				("SIGURG(%p): counts %d/%d state %s\n",
				vp, so->so_oobsigcnt,
				so->so_oobcnt,
				pr_state(so->so_state, so->so_mode)));
			so_oob_sig(so, 1, allmsgsigs, pollwakeups);
			dprintso(so, 1,
				("after SIGURG(%p): counts %d/%d "
				" poll 0x%x sig 0x%x state %s\n",
				vp, so->so_oobsigcnt,
				so->so_oobcnt, *pollwakeups, *allmsgsigs,
				pr_state(so->so_state, so->so_mode)));
			mutex_exit(&so->so_lock);
		}
		freemsg(mp);
		return (NULL);

	case M_SIG:
	case M_HANGUP:
	case M_UNHANGUP:
	case M_ERROR:
		/* M_ERRORs etc are ignored */
		freemsg(mp);
		return (NULL);

	case M_FLUSH:
		/*
		 * Do not flush read queue. If the M_FLUSH
		 * arrives because of an impending T_discon_ind
		 * we still have to keep any queued data - this is part of
		 * socket semantics.
		 */
		if (*mp->b_rptr & FLUSHW) {
			*mp->b_rptr &= ~FLUSHR;
			return (mp);
		}
		freemsg(mp);
		return (NULL);

	default:
		return (mp);
	}
}

/*
 * Translate a TLI(/XTI) error into a system error as best we can.
 */
static ushort_t tli_errs[] = {
		0,		/* no error	*/
		EADDRNOTAVAIL,  /* TBADADDR	*/
		ENOPROTOOPT,	/* TBADOPT	*/
		EACCES,		/* TACCES	*/
		EBADF,		/* TBADF	*/
		EADDRNOTAVAIL,	/* TNOADDR	*/
		EPROTO,		/* TOUTSTATE	*/
		ECONNABORTED,	/* TBADSEQ	*/
		0,		/* TSYSERR - will never get	*/
		EPROTO,		/* TLOOK - should never be sent by transport */
		EMSGSIZE,	/* TBADDATA	*/
		EMSGSIZE,	/* TBUFOVFLW	*/
		EPROTO,		/* TFLOW	*/
		EWOULDBLOCK,	/* TNODATA	*/
		EPROTO,		/* TNODIS	*/
		EPROTO,		/* TNOUDERR	*/
		EINVAL,		/* TBADFLAG	*/
		EPROTO,		/* TNOREL	*/
		EOPNOTSUPP,	/* TNOTSUPPORT	*/
		EPROTO,		/* TSTATECHNG	*/
		/* following represent error namespace expansion with XTI */
		EPROTO,		/* TNOSTRUCTYPE - never sent by transport */
		EPROTO,		/* TBADNAME - never sent by transport */
		EPROTO,		/* TBADQLEN - never sent by transport */
		EADDRINUSE,	/* TADDRBUSY	*/
		EBADF,		/* TINDOUT	*/
		EBADF,		/* TPROVMISMATCH */
		EBADF,		/* TRESQLEN	*/
		EBADF,		/* TRESADDR	*/
		EPROTO,		/* TQFULL - never sent by transport */
		EPROTO,		/* TPROTO	*/
};

static int
tlitosyserr(int terr)
{
	ASSERT(terr != TSYSERR);
	if (terr > (int)sizeof (tli_errs) / (int)sizeof (ushort_t))
		return (EPROTO);
	else
		return ((int)tli_errs[terr]);
}

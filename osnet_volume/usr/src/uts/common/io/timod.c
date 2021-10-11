/* ONC_PLUS EXTRACT START */
/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)timod.c	1.67	99/05/17 SMI"	/* SVr4.0 1.11	*/

/*
 * Transport Interface Library cooperating module - issue 2
 */

/* ONC_PLUS EXTRACT END */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/suntpi.h>
#include <sys/debug.h>
#include <sys/strlog.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/strsun.h>
#include <c2/audit.h>

/*
 * This is the loadable module wrapper.
 */
#include <sys/conf.h>
#include <sys/modctl.h>

static struct streamtab timinfo;

static struct fmodsw fsw = {
	"timod",
	&timinfo,
	D_NEW|D_MTQPAIR|D_MP,
};

/*
 * Module linkage information for the kernel.
 */

static struct modlstrmod modlstrmod = {
	&mod_strmodops, "transport interface str mod", &fsw
};

static struct modlinkage modlinkage = {
	MODREV_1, &modlstrmod, NULL
};

static krwlock_t	tim_list_rwlock;

/*
 * This module keeps track of capabilities of underlying transport. Information
 * is persistent through module invocations (open/close). Currently it remembers
 * whether underlying transport supports TI_{GET,SET}{MY,PEER}NAME ioctls and
 * T_CAPABILITY_REQ message. This module either passes ioctl/messages to the
 * transport or emulates it when transport doesn't understand these
 * ioctl/messages.
 *
 * It is assumed that transport supports T_CAPABILITY_REQ when timod receives
 * T_CAPABILITY_ACK from the transport. There is no current standard describing
 * transport behaviour when it receives unknown message type, so following
 * reactions are expected and handled:
 *
 * 1) Transport drops unknown T_CAPABILITY_REQ message type. In this case timod
 *    will wait for tcap_wait time and assume that transport doesn't provide
 *    this message type. T_CAPABILITY_REQ should never travel over the wire, so
 *    timeout value should only take into consideration internal processing time
 *    for the message. From user standpoint it may mean that an application will
 *    hang for TCAP_WAIT time in the kernel the first time this message is used
 *    with some particular transport (e.g. TCP/IP) during system uptime.
 *
 * 2) Transport responds with T_ERROR_ACK specifying T_CAPABILITY_REQ as
 *    original message type. In this case it is assumed that transport doesn't
 *    support it (which may not always be true - some transports return
 *    T_ERROR_ACK in other cases like lack of system memory).
 *
 * 3) Transport responds with M_ERROR, effectively shutting down the
 *    stream. Unfortunately there is no standard way to pass the reason of
 *    M_ERROR message back to the caller, so it is assumed that if M_ERROR was
 *    sent in response to T_CAPABILITY_REQ message, transport doesn't support
 *    it.
 *
 * It is possible under certain circumstances that timod will incorrectly assume
 * that underlying transport doesn't provide T_CAPABILITY_REQ message type. In
 * this "worst-case" scenario timod will emulate its functionality by itself and
 * will provide only TC1_INFO capability. All other bits in CAP_bits1 field are
 * cleaned. TC1_INFO is emulated by sending T_INFO_REQ down to transport
 * provider.
 */

struct tim_tim {
	uint32_t	tim_flags;
	t_uscalar_t	tim_backlog;
	queue_t		*tim_rdq;
	mblk_t		*tim_iocsave;
	t_scalar_t	tim_mymaxlen;
	t_scalar_t	tim_mylen;
	caddr_t		tim_myname;
	t_scalar_t	tim_peermaxlen;
	t_scalar_t	tim_peerlen;
	caddr_t		tim_peername;
	mblk_t		*tim_consave;
	bufcall_id_t	tim_wbufcid;
	bufcall_id_t	tim_rbufcid;
	timeout_id_t	tim_wtimoutid;
	timeout_id_t	tim_rtimoutid;
	/* Protected by the global tim_list_rwlock for all instances */
	struct tim_tim	*tim_next;
	struct tim_tim	**tim_ptpn;
	t_uscalar_t	tim_acceptor;
	t_scalar_t	tim_saved_prim;		/* Primitive from message */
						/*  part of ioctl. */
	timeout_id_t	tim_tcap_timoutid;	/* For T_CAP_REQ timeout */
	tpi_provinfo_t	*tim_provinfo;		/* Transport description */
};


/*
 * Local flags used with tim_flags field in instance structure of
 * type 'struct _ti_user' declared above.
 * Historical note:
 * This namespace constants were previously declared in a
 * a very messed up namespace in timod.h
 *
 * There may be 3 states for transport:
 *
 * 1) It provides T_CAPABILITY_REQ
 * 2) It does not provide T_CAPABILITY_REQ
 * 3) It is not known yet whether transport provides T_CAPABILITY_REQ or not.
 *
 * It is assumed that the underlying transport either provides
 * T_CAPABILITY_REQ or not and this does not changes during the
 * system lifetime.
 *
 */
#define	PEEK_RDQ_EXPIND 0x0001	/* look for expinds on stream rd queues */
#define	WAITIOCACK	0x0002	/* waiting for info for ioctl act	*/
#define	CLTS		0x0004	/* connectionless transport		*/
#define	COTS		0x0008	/* connection-oriented transport	*/
#define	CONNWAIT	0x0010	/* waiting for connect confirmation	*/
#define	LOCORDREL	0x0020	/* local end has orderly released	*/
#define	REMORDREL	0x0040	/* remote end had orderly released	*/
#define	NAMEPROC	0x0080	/* processing a NAME ioctl		*/
/* ONC_PLUS EXTRACT START */
#define	DO_MYNAME	0x0100	/* timod handles TI_GETMYNAME		*/
/* ONC_PLUS EXTRACT END */
#define	DO_PEERNAME	0x0200	/* timod handles TI_GETPEERNAME		*/
#define	TI_CAP_RECVD	0x0400	/* TI_CAPABILITY received		*/
#define	CAP_WANTS_INFO	0x0800	/* TI_CAPABILITY has TC1_INFO set	*/
#define	WAIT_IOCINFOACK	0x1000	/* T_INFO_REQ generated from ioctl	*/


/* Debugging facilities */
/*
 * Logging needed for debugging timod should only appear in DEBUG kernel.
 */
#ifdef DEBUG
#define	TILOG(msg, arg)		tilog((msg), (arg))
#define	TILOGP(msg, arg)	tilogp((msg), (arg))
#else
#define	TILOG(msg, arg)
#define	TILOGP(msg, arg)
#endif


/*
 * Sleep timeout for T_CAPABILITY_REQ. This message never travels across
 * network, so timeout value should be enough to cover all internal processing
 * time.
 */
clock_t tim_tcap_wait = 2;

/* Sleep timeout in tim_recover() */
#define	TIMWAIT	(1*hz)

/*
 * Return values for ti_doname().
 */
#define	DONAME_FAIL	0	/* failing ioctl (done) */
#define	DONAME_DONE	1	/* done processing */
#define	DONAME_CONT	2	/* continue proceesing (not done yet) */

/*
 * Function prototypes
 */
static int ti_doname(queue_t *, mblk_t *, caddr_t, t_uscalar_t, caddr_t,
    t_uscalar_t);
static int ti_expind_on_rdqueues(queue_t *);
static void tim_ioctl_send_reply(queue_t *, mblk_t *, mblk_t *);
static void tim_send_ioc_error_ack(queue_t *, struct tim_tim *, mblk_t *);
static void send_iocnak(queue_t *, struct iocblk *, mblk_t *, int);
static void tim_tcap_timer(void *);
static void tim_tcap_genreply(queue_t *, struct tim_tim *);
static void tim_send_reply(queue_t *, mblk_t *, struct tim_tim *, t_scalar_t);
static void tim_answer_ti_sync(queue_t *, mblk_t *p, struct tim_tim *,
    struct iocblk *, mblk_t *, uint32_t);
static int tim_pullup(queue_t *, mblk_t *, struct iocblk *);
static void tim_send_ioctl_tpi_msg(queue_t *, mblk_t *, struct tim_tim *, int);


int
_init(void)
{
	int	error;

	rw_init(&tim_list_rwlock, NULL, RW_DRIVER, NULL);
	error = mod_install(&modlinkage);
	if (error != 0) {
		rw_destroy(&tim_list_rwlock);
		return (error);
	}

	return (0);
}

int
_fini(void)
{
	int	error;

	error = mod_remove(&modlinkage);
	if (error != 0)
		return (error);
	rw_destroy(&tim_list_rwlock);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/*
 * Hash list for all instances. Used to find tim_tim structure based on
 * ACCEPTOR_id in T_CONN_RES. Protected by tim_list_rwlock.
 */
#define	TIM_HASH_SIZE	256
#ifdef	_ILP32
#define	TIM_HASH(id) (((uintptr_t)(id) >> 8) % TIM_HASH_SIZE)
#else
#define	TIM_HASH(id) ((uintptr_t)(id) % TIM_HASH_SIZE)
#endif	/* _ILP32 */
static struct tim_tim	*tim_hash[TIM_HASH_SIZE];
int		tim_cnt = 0;

static void tilog(char *, t_scalar_t);
static void tilogp(char *, uintptr_t);
static int tim_setname(queue_t *, mblk_t *);
static mblk_t *tim_filladdr(queue_t *, mblk_t *);
static void tim_bcopy(mblk_t *, caddr_t, t_uscalar_t);
static void tim_addlink(struct tim_tim	*);
static void tim_dellink(struct tim_tim	*);
static struct tim_tim *tim_findlink(t_uscalar_t);
static void tim_recover(queue_t *, mblk_t *, t_scalar_t);

int dotilog = 0;

#define	TIMOD_ID	3

/* ONC_PLUS EXTRACT START */
static int timodopen(queue_t *, dev_t *, int, int, cred_t *);
/* ONC_PLUS EXTRACT END */
static int timodclose(queue_t *, int, cred_t *);
static void timodwput(queue_t *, mblk_t *);
static void timodrput(queue_t *, mblk_t *);
/* ONC_PLUS EXTRACT START */
static void timodrsrv(queue_t *);
/* ONC_PLUS EXTRACT END */
static void timodwsrv(queue_t *);
/* ONC_PLUS EXTRACT START */
static int timodrproc(queue_t *, mblk_t *);
static int timodwproc(queue_t *, mblk_t *);
/* ONC_PLUS EXTRACT END */

static void send_ERRORW(queue_t *, mblk_t *);

/* stream data structure definitions */

static struct module_info timod_info =
	{TIMOD_ID, "timod", 0, INFPSZ, 512, 128};
static struct qinit timodrinit = {
	(int (*)())timodrput,
	(int (*)())timodrsrv,
	timodopen,
	timodclose,
	nulldev,
	&timod_info,
	NULL
};
static struct qinit timodwinit = {
	(int (*)())timodwput,
	(int (*)())timodwsrv,
	timodopen,
	timodclose,
	nulldev,
	&timod_info,
	NULL
};
static struct streamtab timinfo = { &timodrinit, &timodwinit, NULL, NULL };

static int OK_offsets(mblk_t *, long, long);

/* ONC_PLUS EXTRACT START */
/*
 * timodopen -	open routine gets called when the module gets pushed
 *		onto the stream.
 */
/*ARGSUSED*/
static int
timodopen(
	queue_t *q,
	dev_t *devp,
	int flag,
	int sflag,
	cred_t *crp)
{
	struct tim_tim *tp;
	struct stroptions *sop;
	mblk_t *bp;

	ASSERT(q != NULL);

	if (q->q_ptr) {
		return (0);
	}

	if ((bp = allocb(sizeof (struct stroptions), BPRI_MED)) == 0)
		return (ENOMEM);

	tp = kmem_zalloc(sizeof (struct tim_tim), KM_SLEEP);
	tp->tim_rdq = q;
	tp->tim_iocsave = NULL;
	tp->tim_consave = NULL;

	q->q_ptr = (caddr_t)tp;
	WR(q)->q_ptr = (caddr_t)tp;

	tilogp("timodopen: Allocated for tp %lx\n", (uintptr_t)tp);
	tilogp("timodopen: Allocated for q %lx\n", (uintptr_t)q);

	qprocson(q);

	tp->tim_provinfo = tpi_findprov(q);

	/*
	 * Defer allocation of the buffers for the local address and
	 * the peer's address until we need them.
	 * Assume that timod has to handle getname until we here
	 * an iocack from the transport provider or we know that
	 * transport provider doesn't understand it.
	 */
	if (tp->tim_provinfo->tpi_myname != PI_YES) {
		TILOG("timodopen: setting DO_MYNAME\n", 0);
		tp->tim_flags |= DO_MYNAME;
	}

	if (tp->tim_provinfo->tpi_peername != PI_YES) {
		TILOG("timodopen: setting DO_PEERNAME\n", 0);
		tp->tim_flags |= DO_PEERNAME;
	}

#ifdef	_ILP32
	{
		queue_t *driverq;

		/*
		 * Find my driver's read queue (for T_CONN_RES handling)
		 */
		driverq = WR(q);
		while (SAMESTR(driverq))
			driverq = driverq->q_next;

		tp->tim_acceptor = (t_uscalar_t)RD(driverq);
	}
#else
	tp->tim_acceptor = (t_uscalar_t)getminor(*devp);
#endif	/* _ILP32 */

	/*
	 * Add this one to the list.
	 */
	tim_addlink(tp);

	/*
	 * Send M_SETOPTS to stream head to make sure M_PCPROTO messages
	 * are not flushed. This prevents application deadlocks.
	 */
	bp->b_datap->db_type = M_SETOPTS;
	bp->b_wptr += sizeof (struct stroptions);
	sop = (struct stroptions *)bp->b_rptr;
	sop->so_flags = SO_READOPT;
	sop->so_readopt = RFLUSHPCPROT;

	putnext(q, bp);

	return (0);
}

static void
tim_timer(void *arg)
{
	queue_t *q = arg;
	struct tim_tim *tp = (struct tim_tim *)q->q_ptr;

	ASSERT(tp);

	if (q->q_flag & QREADR) {
		ASSERT(tp->tim_rtimoutid);
		tp->tim_rtimoutid = 0;
	} else {
		ASSERT(tp->tim_wtimoutid);
		tp->tim_wtimoutid = 0;
	}
	enableok(q);
	qenable(q);
}

static void
tim_buffer(void *arg)
{
	queue_t *q = arg;
	struct tim_tim *tp = (struct tim_tim *)q->q_ptr;

	ASSERT(tp);

	if (q->q_flag & QREADR) {
		ASSERT(tp->tim_rbufcid);
		tp->tim_rbufcid = 0;
	} else {
		ASSERT(tp->tim_wbufcid);
		tp->tim_wbufcid = 0;
	}
	enableok(q);
	qenable(q);
}
/* ONC_PLUS EXTRACT END */

/*
 * timodclose - This routine gets called when the module gets popped
 * off of the stream.
 */
/*ARGSUSED*/
static int
timodclose(
	queue_t *q,
	int flag,
	cred_t *crp)
{
	struct tim_tim *tp;
	mblk_t *mp;
	mblk_t *nmp;

	ASSERT(q != NULL);

	tp = (struct tim_tim *)q->q_ptr;
	q->q_ptr = NULL;

	ASSERT(tp != NULL);

	tilogp("timodclose: Entered for tp %lx\n", (uintptr_t)tp);
	tilogp("timodclose: Entered for q %lx\n", (uintptr_t)q);

	qprocsoff(q);

	/*
	 * Cancel any outstanding bufcall
	 * or timeout requests.
	 */
	if (tp->tim_wbufcid) {
		qunbufcall(q, tp->tim_wbufcid);
		tp->tim_wbufcid = 0;
	}
	if (tp->tim_rbufcid) {
		qunbufcall(q, tp->tim_rbufcid);
		tp->tim_rbufcid = 0;
	}
	if (tp->tim_wtimoutid) {
		(void) quntimeout(q, tp->tim_wtimoutid);
		tp->tim_wtimoutid = 0;
	}
	if (tp->tim_rtimoutid) {
		(void) quntimeout(q, tp->tim_rtimoutid);
		tp->tim_rtimoutid = 0;
	}

	if (tp->tim_tcap_timoutid != 0) {
		(void) quntimeout(q, tp->tim_tcap_timoutid);
		tp->tim_tcap_timoutid = 0;
	}

	if (tp->tim_iocsave != NULL)
		freemsg(tp->tim_iocsave);
	mp = tp->tim_consave;
	while (mp) {
		nmp = mp->b_next;
		mp->b_next = NULL;
		freemsg(mp);
		mp = nmp;
	}
	ASSERT(tp->tim_mymaxlen >= 0);
	if (tp->tim_mymaxlen != 0)
		kmem_free(tp->tim_myname, (size_t)tp->tim_mymaxlen);
	ASSERT(tp->tim_peermaxlen >= 0);
	if (tp->tim_peermaxlen != 0)
		kmem_free(tp->tim_peername, (size_t)tp->tim_peermaxlen);

	q->q_ptr = WR(q)->q_ptr = NULL;
	tim_dellink(tp);


	return (0);
}

/*
 * timodrput -	Module read put procedure.  This is called from
 *		the module, driver, or stream head upstream/downstream.
 *		Handles M_FLUSH, M_DATA and some M_PROTO (T_DATA_IND,
 *		and T_UNITDATA_IND) messages. All others are queued to
 *		be handled by the service procedures.
 */
static void
timodrput(queue_t *q, mblk_t *mp)
{
	union T_primitives *pptr;

	/*
	 * During flow control and other instances when messages
	 * are on queue, queue up a non high priority message
	 */
	if (q->q_first != 0 && mp->b_datap->db_type < QPCTL) {
		(void) putq(q, mp);
		return;
	}

	/*
	 * Inline processing of data (to avoid additional procedure call).
	 * Rest is handled in timodrproc.
	 */

	switch (mp->b_datap->db_type) {
	case M_DATA:
		if (bcanput(q->q_next, mp->b_band))
			putnext(q, mp);
		else
			(void) putq(q, mp);
		break;
	case M_PROTO:
	case M_PCPROTO:
		pptr = (union T_primitives *)mp->b_rptr;
		switch (pptr->type) {
		case T_EXDATA_IND:
		case T_DATA_IND:
		case T_UNITDATA_IND:
			if (bcanput(q->q_next, mp->b_band))
				putnext(q, mp);
			else
				(void) putq(q, mp);
			break;
		default:
			(void) timodrproc(q, mp);
			break;
		}
		break;
	default:
		(void) timodrproc(q, mp);
		break;
	}
}

/* ONC_PLUS EXTRACT START */
/*
 * timodrsrv -	Module read queue service procedure.  This is called when
 *		messages are placed on an empty queue, when high priority
 *		messages are placed on the queue, and when flow control
 *		restrictions subside.  This code used to be included in a
 *		put procedure, but it was moved to a service procedure
 *		because several points were added where memory allocation
 *		could fail, and there is no reasonable recovery mechanism
 *		from the put procedure.
 */
/*ARGSUSED*/
static void
timodrsrv(queue_t *q)
{
/* ONC_PLUS EXTRACT END */
	mblk_t *mp;
	struct tim_tim *tp;

	ASSERT(q != NULL);

	tp = (struct tim_tim *)q->q_ptr;
	if (!tp)
	    return;

	while ((mp = getq(q)) != NULL) {
		if (timodrproc(q, mp)) {
			/*
			 * timodwproc did a putbq - stop processing
			 * messages.
			 */
			return;
		}
	}
/* ONC_PLUS EXTRACT START */
}

/*
 * Perform common processing when a T_CAPABILITY_ACK or T_INFO_ACK
 * arrive.  Set the queue properties and adjust the tim_flags according
 * to the service type.
 */
static void
timodprocessinfo(queue_t *q, struct tim_tim *tp, struct T_info_ack *tia)
{
	TILOG("timodprocessinfo: strqset(%d)\n", tia->TIDU_size);
	(void) strqset(q, QMAXPSZ, 0, tia->TIDU_size);
	(void) strqset(OTHERQ(q), QMAXPSZ, 0, tia->TIDU_size);

	if ((tia->SERV_type == T_COTS) || (tia->SERV_type == T_COTS_ORD))
		tp->tim_flags = (tp->tim_flags & ~CLTS) | COTS;
	else if (tia->SERV_type == T_CLTS)
		tp->tim_flags = (tp->tim_flags & ~COTS) | CLTS;
}

static int
timodrproc(queue_t *q, mblk_t *mp)
{
	union T_primitives *pptr;
	struct tim_tim *tp;
	struct iocblk *iocbp;
	mblk_t *nbp;
/* ONC_PLUS EXTRACT END */

	tp = (struct tim_tim *)q->q_ptr;

/* ONC_PLUS EXTRACT START */
	switch (mp->b_datap->db_type) {
	default:
		putnext(q, mp);
		break;

	case M_ERROR:
		TILOG("timodrproc: Got M_ERROR, flags = %x\n", tp->tim_flags);
		/*
		 * There is no specified standard response for driver when it
		 * receives unknown message type and M_ERROR is one
		 * possibility. If we send T_CAPABILITY_REQ down and transport
		 * provider responds with M_ERROR we assume that it doesn't
		 * understand this message type. This assumption may be
		 * sometimes incorrect (transport may reply with M_ERROR for
		 * some other reason) but there is no way for us to distinguish
		 * between different cases. In the worst case timod and everyone
		 * else sharing global transport description with it may end up
		 * emulating T_CAPABILITY_REQ.
		 */

		/*
		 * Check that we are waiting for T_CAPABILITY_ACK and
		 * T_CAPABILITY_REQ is not implemented by transport or emulated
		 * by timod.
		 */
		if ((tp->tim_provinfo->tpi_capability == PI_DONTKNOW) &&
		    ((tp->tim_flags & TI_CAP_RECVD) != 0)) {
			/*
			 * Good chances that this transport doesn't provide
			 * T_CAPABILITY_REQ. Mark this information  permanently
			 * for the module + transport combination.
			 */
			PI_PROVLOCK(tp->tim_provinfo);
			if (tp->tim_provinfo->tpi_capability == PI_DONTKNOW)
				tp->tim_provinfo->tpi_capability = PI_NO;
			PI_PROVUNLOCK(tp->tim_provinfo);
			if (tp->tim_tcap_timoutid != 0) {
				(void) quntimeout(q, tp->tim_tcap_timoutid);
				tp->tim_tcap_timoutid = 0;
			}
		}
		putnext(q, mp);
		break;
	case M_DATA:
		if (!bcanput(q->q_next, mp->b_band)) {
			(void) putbq(q, mp);
			return (1);
		}
		putnext(q, mp);
		break;

	case M_PROTO:
	case M_PCPROTO:
	    /* assert checks if there is enough data to determine type */

	    ASSERT((mp->b_wptr - mp->b_rptr) >= sizeof (t_scalar_t));

	    pptr = (union T_primitives *)mp->b_rptr;
	    switch (pptr->type) {
	    default:
/* ONC_PLUS EXTRACT END */

#ifdef C2_AUDIT
		if (audit_active)
		    audit_sock(T_UNITDATA_IND, q, mp, TIMOD_ID);
#endif
/* ONC_PLUS EXTRACT START */
		putnext(q, mp);
		break;
/* ONC_PLUS EXTRACT END */

	    case T_ERROR_ACK:

		tilog("timodrproc: Got T_ERROR_ACK, flags = %x\n",
		    tp->tim_flags);

		/* Restore db_type - recover() might have changed it */
		mp->b_datap->db_type = M_PCPROTO;
		if ((tp->tim_flags & WAITIOCACK) == 0)
			putnext(q, mp);
		else
			tim_send_ioc_error_ack(q, tp, mp);
		break;

	    case T_OK_ACK:

		tilog("timodrproc: Got T_OK_ACK\n", 0);

		if (pptr->ok_ack.CORRECT_prim == T_UNBIND_REQ)
			tp->tim_mylen = 0;
		tim_send_reply(q, mp, tp, pptr->ok_ack.CORRECT_prim);
		break;

/* ONC_PLUS EXTRACT START */
	    case T_BIND_ACK: {
		struct T_bind_ack *ackp =
		    (struct T_bind_ack *)mp->b_rptr;

		tilog("timodrproc: Got T_BIND_ACK\n", 0);

		ASSERT((mp->b_wptr - mp->b_rptr) >= sizeof (struct T_bind_ack));

		/* Restore db_type - recover() might have change it */
		mp->b_datap->db_type = M_PCPROTO;

		/* save negotiated backlog */
		tp->tim_backlog = ackp->CONIND_number;

		if (((tp->tim_flags & WAITIOCACK) == 0) ||
		    ((tp->tim_saved_prim != O_T_BIND_REQ) &&
		    (tp->tim_saved_prim != T_BIND_REQ))) {
			putnext(q, mp);
		} else {
		    ASSERT(tp->tim_iocsave != NULL);

		    if (tp->tim_flags & DO_MYNAME) {
			    caddr_t p;

			    ASSERT(ackp->ADDR_length >= 0);
			    if (ackp->ADDR_length > tp->tim_mymaxlen) {
				    p = kmem_alloc((size_t)ackp->ADDR_length,
					KM_NOSLEEP);
				    if (p == NULL) {
					tilog("timodrproc: kmem_alloc failed "
					    "attempt recovery\n", 0);

					tim_recover(q, mp, ackp->ADDR_length);
					return (1);
				    }
				    ASSERT(tp->tim_mymaxlen >= 0);
				    if (tp->tim_mymaxlen)
					kmem_free(tp->tim_myname,
					    (size_t)tp->tim_mymaxlen);
				    tp->tim_myname = p;
				    tp->tim_mymaxlen = ackp->ADDR_length;
			    }
			    tp->tim_mylen = ackp->ADDR_length;
			    p = (caddr_t)mp->b_rptr + ackp->ADDR_offset;
			    bcopy(p, tp->tim_myname, (size_t)tp->tim_mylen);
		    }
		    tim_ioctl_send_reply(q, tp->tim_iocsave, mp);
		    tp->tim_iocsave = NULL;
		    tp->tim_flags &= ~(WAITIOCACK | WAIT_IOCINFOACK |
			TI_CAP_RECVD | CAP_WANTS_INFO);
		}
		break;
	    }

/* ONC_PLUS EXTRACT END */
	    case T_OPTMGMT_ACK:

		tilog("timodrproc: Got T_OPTMGMT_ACK\n", 0);

		/* Restore db_type - recover() might have change it */
		mp->b_datap->db_type = M_PCPROTO;

		if (((tp->tim_flags & WAITIOCACK) == 0) ||
		    ((tp->tim_saved_prim != T_SVR4_OPTMGMT_REQ) &&
		    (tp->tim_saved_prim != T_OPTMGMT_REQ))) {
			putnext(q, mp);
		} else {
		    ASSERT(tp->tim_iocsave != NULL);
		    tim_ioctl_send_reply(q, tp->tim_iocsave, mp);
		    tp->tim_iocsave = NULL;
		    tp->tim_flags &= ~(WAITIOCACK | WAIT_IOCINFOACK |
			TI_CAP_RECVD | CAP_WANTS_INFO);
		}
		break;

	    case T_INFO_ACK: {
		    struct T_info_ack *tia = (struct T_info_ack *)pptr;

		    tilog("timodrproc: Got T_INFO_ACK, flags = %x\n",
			tp->tim_flags);

		    /* Restore db_type - recover() might have change it */
		    mp->b_datap->db_type = M_PCPROTO;

		    ASSERT((size_t)(mp->b_wptr - mp->b_rptr) ==
			sizeof (struct T_info_ack));

		    timodprocessinfo(q, tp, tia);

		    TILOG("timodrproc: flags = %x\n", tp->tim_flags);
		    if ((tp->tim_flags & WAITIOCACK) != 0) {
			size_t	expected_ack_size;
			ssize_t	deficit;
			int	ioc_cmd;
			struct T_capability_ack *tcap;
			ssize_t  size = mp->b_wptr - mp->b_rptr;

			/*
			 * The only case when T_INFO_ACK may be received back
			 * when we are waiting for ioctl to complete is when
			 * this ioctl sent T_INFO_REQ down.
			 */
			ASSERT((tp->tim_flags & WAIT_IOCINFOACK) != 0);
			ASSERT(tp->tim_iocsave != NULL);

			iocbp = (struct iocblk *)tp->tim_iocsave->b_rptr;
			ioc_cmd = iocbp->ioc_cmd;

			/*
			 * Was it sent from TI_CAPABILITY emulation?
			 */
			if (ioc_cmd == TI_CAPABILITY) {
				struct T_info_ack	saved_info;

				/*
				 * Perform sanity checks. The only case when we
				 * send T_INFO_REQ from TI_CAPABILITY is when
				 * timod emulates T_CAPABILITY_REQ and CAP_bits1
				 * has TC1_INFO set.
				 */
				ASSERT((tp->tim_flags &
				    (TI_CAP_RECVD | CAP_WANTS_INFO)) ==
				    (TI_CAP_RECVD | CAP_WANTS_INFO));

				TILOG("timodrproc: emulating TI_CAPABILITY/"
				    "info\n", 0);

				/* Save info & reuse mp for T_CAPABILITY_ACK */
				saved_info = *tia;

				mp = tpi_ack_alloc(mp,
				    sizeof (struct T_capability_ack),
				    M_PCPROTO, T_CAPABILITY_ACK);

				if (mp == NULL) {
					tilog("timodrproc: realloc failed, "
					    "no recovery attempted\n", 0);
					return (1);
				}

				/*
				 * Copy T_INFO information into T_CAPABILITY_ACK
				 */
				tcap = (struct T_capability_ack *)mp->b_rptr;
				tcap->CAP_bits1 = TC1_INFO;
				tcap->INFO_ack = saved_info;
				tp->tim_flags &= ~(WAITIOCACK |
				    WAIT_IOCINFOACK | TI_CAP_RECVD |
				    CAP_WANTS_INFO);
				tim_ioctl_send_reply(q, tp->tim_iocsave, mp);
				tp->tim_iocsave = NULL;
				break;
			}

			/*
			 * The code for TI_SYNC/TI_GETINFO is left here only for
			 * backward compatibility with staticaly linked old
			 * applications. New TLI/XTI code should use
			 * TI_CAPABILITY for getting transport info and should
			 * not use TI_GETINFO/TI_SYNC for this purpose.
			 */

			/*
			 * make sure the message sent back is the size of
			 * the "expected ack"
			 * For TI_GETINFO, expected ack size is
			 *	sizeof (T_info_ack)
			 * For TI_SYNC, expected ack size is
			 *	sizeof (struct ti_sync_ack);
			 */
			ASSERT(ioc_cmd == TI_GETINFO || ioc_cmd == TI_SYNC);

			expected_ack_size =
				sizeof (struct T_info_ack); /* TI_GETINFO */
			if (iocbp->ioc_cmd == TI_SYNC)
				expected_ack_size = sizeof (struct ti_sync_ack);
			deficit = expected_ack_size - size;

			if (deficit != 0) {
				if (mp->b_datap->db_lim - mp->b_wptr <
				    deficit) {
				    mblk_t *tmp = allocb(expected_ack_size,
					BPRI_HI);
				    if (tmp == NULL) {
					ASSERT((mp->b_datap->db_lim -
						mp->b_datap->db_base) <
						sizeof (struct T_error_ack));

					tilog("timodrproc: allocb failed no "
					    "recovery attempt\n", 0);

					mp->b_rptr = mp->b_datap->db_base;
					mp->b_wptr = mp->b_rptr +
						sizeof (struct T_error_ack);
					pptr = (union T_primitives *)
						mp->b_rptr;
					pptr->error_ack.ERROR_prim = T_INFO_ACK;
					pptr->error_ack.TLI_error = TSYSERR;
					pptr->error_ack.UNIX_error = EAGAIN;
					pptr->error_ack.PRIM_type = T_ERROR_ACK;
					mp->b_datap->db_type = M_PCPROTO;
					tim_send_ioc_error_ack(q, tp, mp);
					break;
				    } else {
					bcopy(mp->b_rptr,
						tmp->b_rptr,
						size);
					tmp->b_wptr += size;
					pptr = (union T_primitives *)
					    tmp->b_rptr;
					freemsg(mp);
					mp = tmp;
				    }
				}
			}
			/*
			 * We now have "mp" which has enough space for an
			 * appropriate ack and contains struct T_info_ack
			 * that the transport provider returned. We now
			 * stuff it with more stuff to fullfill
			 * TI_SYNC ioctl needs, as necessary
			 */
			if (iocbp->ioc_cmd == TI_SYNC) {
				/*
				 * Assumes struct T_info_ack is first embedded
				 * type in struct ti_sync_ack so it is
				 * automatically there.
				 */
				struct ti_sync_ack *tsap =
				    (struct ti_sync_ack *)mp->b_rptr;

				/*
				 * tsap->tsa_qlen needs to be set only if
				 * TSRF_QLEN_REQ flag is set, but for
				 * compatibility with statically linked
				 * applications it is set here regardless of the
				 * flag since old XTI library expected it to be
				 * set.
				 */
				tsap->tsa_qlen = tp->tim_backlog;
				tsap->tsa_flags = 0x0; /* intialize clear */
				if (tp->tim_flags & PEEK_RDQ_EXPIND) {
					/*
					 * Request to peek for EXPIND in
					 * rcvbuf.
					 */
					if (ti_expind_on_rdqueues(q)) {
						/*
						 * Expedited data is
						 * queued on the stream
						 * read side
						 */
						tsap->tsa_flags |=
						    TSAF_EXP_QUEUED;
					}
					tp->tim_flags &=
					    ~PEEK_RDQ_EXPIND;
				}
				mp->b_wptr += 2*sizeof (uint32_t);
			}
			tim_ioctl_send_reply(q, tp->tim_iocsave, mp);
			tp->tim_iocsave = NULL;
			tp->tim_flags &= ~(WAITIOCACK | WAIT_IOCINFOACK |
			    TI_CAP_RECVD | CAP_WANTS_INFO);
			break;
		    }
	    }

	    putnext(q, mp);
	    break;

	    case T_ADDR_ACK:
		tilog("timodrproc: Got T_ADDR_ACK\n", 0);
		tim_send_reply(q, mp, tp, T_ADDR_REQ);
		break;

/* ONC_PLUS EXTRACT START */
	    case T_CONN_IND:

		tilog("timodrproc: Got T_CONN_IND\n", 0);

		if (tp->tim_flags & DO_PEERNAME) {
		    if (((nbp = dupmsg(mp)) != NULL) ||
			((nbp = copymsg(mp)) != NULL)) {
			nbp->b_next = tp->tim_consave;
			tp->tim_consave = nbp;
		    } else {
			tim_recover(q, mp, (t_scalar_t)sizeof (mblk_t));
			return (1);
		    }
		}
/* ONC_PLUS EXTRACT END */
#ifdef C2_AUDIT
	if (audit_active)
		audit_sock(T_CONN_IND, q, mp, TIMOD_ID);
#endif
/* ONC_PLUS EXTRACT START */
		putnext(q, mp);
		break;

/* ONC_PLUS EXTRACT END */
	    case T_CONN_CON:

		tilog("timodrproc: Got T_CONN_CON\n", 0);

		tp->tim_flags &= ~CONNWAIT;
		putnext(q, mp);
		break;

	    case T_DISCON_IND: {
		struct T_discon_ind *disp;
		struct T_conn_ind *conp;
		mblk_t *pbp = NULL;

		if (q->q_first != 0)
			tilog("timodrput: T_DISCON_IND - flow control", 0);

		disp = (struct T_discon_ind *)mp->b_rptr;

		tilog("timodrproc: Got T_DISCON_IND Reason: %d\n",
			disp->DISCON_reason);

		tp->tim_flags &= ~(CONNWAIT|LOCORDREL|REMORDREL);
		tp->tim_peerlen = 0;
		for (nbp = tp->tim_consave; nbp; nbp = nbp->b_next) {
		    conp = (struct T_conn_ind *)nbp->b_rptr;
		    if (conp->SEQ_number == disp->SEQ_number)
			break;
		    pbp = nbp;
		}
		if (nbp) {
		    if (pbp)
			pbp->b_next = nbp->b_next;
		    else
			tp->tim_consave = nbp->b_next;
		    nbp->b_next = NULL;
		    freemsg(nbp);
		}
		putnext(q, mp);
		break;
	    }

	    case T_ORDREL_IND:

		tilog("timodrproc: Got T_ORDREL_IND\n", 0);

		if (tp->tim_flags & LOCORDREL) {
		    tp->tim_flags &= ~(LOCORDREL|REMORDREL);
		    tp->tim_peerlen = 0;
		} else {
		    tp->tim_flags |= REMORDREL;
		}
		putnext(q, mp);
		break;

	    case T_EXDATA_IND:
	    case T_DATA_IND:
	    case T_UNITDATA_IND:
		if (pptr->type == T_EXDATA_IND)
			tilog("timodrproc: Got T_EXDATA_IND\n", 0);

		if (!bcanput(q->q_next, mp->b_band)) {
			(void) putbq(q, mp);
			return (1);
		}
		putnext(q, mp);
		break;

	    case T_CAPABILITY_ACK: {
			struct T_capability_ack	*tca;

			/* This transport supports T_CAPABILITY_REQ */
			tilog("timodrproc: Got T_CAPABILITY_ACK\n", 0);

			if (tp->tim_provinfo->tpi_capability != PI_YES) {
				PI_PROVLOCK(tp->tim_provinfo);
				tp->tim_provinfo->tpi_capability = PI_YES;
				PI_PROVUNLOCK(tp->tim_provinfo);
			}

			/* Reset possible pending timeout */
			if (tp->tim_tcap_timoutid != 0) {
				(void) quntimeout(q, tp->tim_tcap_timoutid);
				tp->tim_tcap_timoutid = 0;
			}

			tca = (struct T_capability_ack *)mp->b_rptr;

			if (tca->CAP_bits1 & TC1_INFO)
				timodprocessinfo(q, tp, &tca->INFO_ack);

			tim_send_reply(q, mp, tp, T_CAPABILITY_REQ);
		}
		break;
	    }
	    break;

/* ONC_PLUS EXTRACT START */
	case M_FLUSH:

		tilog("timodrproc: Got M_FLUSH\n", 0);

		if (*mp->b_rptr & FLUSHR) {
			if (*mp->b_rptr & FLUSHBAND)
				flushband(q, *(mp->b_rptr + 1), FLUSHDATA);
			else
				flushq(q, FLUSHDATA);
		}
		putnext(q, mp);
		break;
/* ONC_PLUS EXTRACT END */

	case M_IOCACK:
	    iocbp = (struct iocblk *)mp->b_rptr;

	    tilog("timodrproc: Got M_IOCACK\n", 0);

	    if (iocbp->ioc_cmd == TI_GETMYNAME) {

		/*
		 * Transport provider supports this ioctl,
		 * so I don't have to.
		 */
		if ((tp->tim_flags & DO_MYNAME) != 0) {
			tp->tim_flags &= ~DO_MYNAME;
			PI_PROVLOCK(tp->tim_provinfo);
			tp->tim_provinfo->tpi_myname = PI_YES;
			PI_PROVUNLOCK(tp->tim_provinfo);
		}

		ASSERT(tp->tim_mymaxlen >= 0);
		if (tp->tim_mymaxlen != 0) {
		    kmem_free(tp->tim_myname, (size_t)tp->tim_mymaxlen);
		    tp->tim_myname = NULL;
		    tp->tim_mymaxlen = 0;
		    freemsg(tp->tim_iocsave);
		    tp->tim_iocsave = NULL;
		}
	    } else if (iocbp->ioc_cmd == TI_GETPEERNAME) {
		/*
		 * Transport provider supports this ioctl,
		 * so I don't have to.
		 */
		if ((tp->tim_flags & DO_PEERNAME) != 0) {
			tp->tim_flags &= ~DO_PEERNAME;
			PI_PROVLOCK(tp->tim_provinfo);
			tp->tim_provinfo->tpi_peername = PI_YES;
			PI_PROVUNLOCK(tp->tim_provinfo);
		}

		ASSERT(tp->tim_peermaxlen >= 0);
		if (tp->tim_peermaxlen != 0) {
		    mblk_t *bp;

		    kmem_free(tp->tim_peername, (size_t)tp->tim_peermaxlen);
		    tp->tim_peername = NULL;
		    tp->tim_peermaxlen = 0;
		    freemsg(tp->tim_iocsave);
		    tp->tim_iocsave = NULL;
		    bp = tp->tim_consave;
		    while (bp) {
			nbp = bp->b_next;
			bp->b_next = NULL;
			freemsg(bp);
			bp = nbp;
		    }
		    tp->tim_consave = NULL;
		}
	    }
	    putnext(q, mp);
	    break;

/* ONC_PLUS EXTRACT START */
	case M_IOCNAK:

	    tilog("timodrproc: Got M_IOCNAK\n", 0);

	    iocbp = (struct iocblk *)mp->b_rptr;
	    if (((iocbp->ioc_cmd == TI_GETMYNAME) ||
		(iocbp->ioc_cmd == TI_GETPEERNAME)) &&
		((iocbp->ioc_error == EINVAL) || (iocbp->ioc_error == 0))) {
		    PI_PROVLOCK(tp->tim_provinfo);
		    if (iocbp->ioc_cmd == TI_GETMYNAME) {
			    if (tp->tim_provinfo->tpi_myname == PI_DONTKNOW)
				    tp->tim_provinfo->tpi_myname = PI_NO;
		    } else if (iocbp->ioc_cmd == TI_GETPEERNAME) {
			    if (tp->tim_provinfo->tpi_peername == PI_DONTKNOW)
				    tp->tim_provinfo->tpi_peername = PI_NO;
		    }
		    PI_PROVUNLOCK(tp->tim_provinfo);
		    if (tp->tim_iocsave) {
			    freemsg(mp);
			    mp = tp->tim_iocsave;
			    tp->tim_iocsave = NULL;
			    tp->tim_flags |= NAMEPROC;
			    if (ti_doname(WR(q), mp, tp->tim_myname,
				tp->tim_mylen, tp->tim_peername,
				tp->tim_peerlen) != DONAME_CONT) {
				    tp->tim_flags &= ~NAMEPROC;
			    }
			    break;
		    }
	    }
	    putnext(q, mp);
	    break;
/* ONC_PLUS EXTRACT END */
	}

	return (0);
}

/* ONC_PLUS EXTRACT START */
/*
 * timodwput -	Module write put procedure.  This is called from
 *		the module, driver, or stream head upstream/downstream.
 *		Handles M_FLUSH, M_DATA and some M_PROTO (T_DATA_REQ,
 *		and T_UNITDATA_REQ) messages. All others are queued to
 *		be handled by the service procedures.
 */

static void
timodwput(queue_t *q, mblk_t *mp)
{
	union T_primitives *pptr;
	struct tim_tim *tp;

	/*
	 * During flow control and other instances when messages
	 * are on queue, queue up a non high priority message
	 */
/* ONC_PLUS EXTRACT END */
	if (q->q_first != 0 && mp->b_datap->db_type < QPCTL) {
		(void) putq(q, mp);
		return;
	}

/* ONC_PLUS EXTRACT START */
	/*
	 * Inline processing of data (to avoid additional procedure call).
	 * Rest is handled in timodwproc.
	 */

	switch (mp->b_datap->db_type) {
	case M_DATA:
		tp = (struct tim_tim *)q->q_ptr;
		ASSERT(tp);
		if (tp->tim_flags & CLTS) {
			mblk_t	*tmp;

			if ((tmp = tim_filladdr(q, mp)) == NULL) {
				(void) putq(q, mp);
				break;
			} else {
				mp = tmp;
			}
		}
		if (bcanput(q->q_next, mp->b_band))
			putnext(q, mp);
		else
			(void) putq(q, mp);
		break;
	case M_PROTO:
	case M_PCPROTO:
		pptr = (union T_primitives *)mp->b_rptr;
		switch (pptr->type) {
/* ONC_PLUS EXTRACT END */
		case T_UNITDATA_REQ:
			tp = (struct tim_tim *)q->q_ptr;
			ASSERT(tp);
			if (tp->tim_flags & CLTS) {
				mblk_t	*tmp;

				if ((tmp = tim_filladdr(q, mp)) == NULL) {
					(void) putq(q, mp);
					break;
				} else {
					mp = tmp;
				}
			}
			if (bcanput(q->q_next, mp->b_band))
				putnext(q, mp);
			else
				(void) putq(q, mp);
			break;

		case T_DATA_REQ:
		case T_EXDATA_REQ:
			if (bcanput(q->q_next, mp->b_band))
				putnext(q, mp);
			else
				(void) putq(q, mp);
			break;
		default:
			(void) timodwproc(q, mp);
			break;
		}
		break;
/* ONC_PLUS EXTRACT START */
	default:
		(void) timodwproc(q, mp);
		break;
	}
}
/*
 * timodwsrv -	Module write queue service procedure.
 *		This is called when messages are placed on an empty queue,
 *		when high priority messages are placed on the queue, and
 *		when flow control restrictions subside.  This code used to
 *		be included in a put procedure, but it was moved to a
 *		service procedure because several points were added where
 *		memory allocation could fail, and there is no reasonable
 *		recovery mechanism from the put procedure.
 */
static void
timodwsrv(queue_t *q)
{
	mblk_t *mp;

	ASSERT(q != NULL);
	if (q->q_ptr == NULL)
	    return;

	while ((mp = getq(q)) != NULL) {
		if (timodwproc(q, mp)) {
			/*
			 * timodwproc did a putbq - stop processing
			 * messages.
			 */
			return;
		}
	}
}

/*
 * Common routine to process write side messages
 */

static int
timodwproc(queue_t *q, mblk_t *mp)
{
	union T_primitives *pptr;
	struct tim_tim *tp;
	mblk_t *tmp;
	struct iocblk *iocbp;
	int rc = 0;

	tp = (struct tim_tim *)q->q_ptr;

	switch (mp->b_datap->db_type) {
	default:
	    putnext(q, mp);
	    break;
/* ONC_PLUS EXTRACT END */

	case M_DATA:
	    if (tp->tim_flags & CLTS) {
		if ((tmp = tim_filladdr(q, mp)) == NULL) {
		    tim_recover(q,
				mp,
				((t_scalar_t)sizeof (struct T_unitdata_req)) +
					tp->tim_peerlen);
		    return (1);
		} else {
		    mp = tmp;
		}
	    }
	    if (!bcanput(q->q_next, mp->b_band)) {
		(void) putbq(q, mp);
		return (1);
	    }
	    putnext(q, mp);
	    break;

/* ONC_PLUS EXTRACT START */
	case M_IOCTL:

	    iocbp = (struct iocblk *)mp->b_rptr;
	    TILOG("timodwproc: Got M_IOCTL(%d)\n", iocbp->ioc_cmd);

	    ASSERT((mp->b_wptr - mp->b_rptr) == sizeof (struct iocblk));

	    if (tp->tim_flags & WAITIOCACK) {
		TILOG("timodwproc: EPROTO, flags = %x\n", tp->tim_flags);
		send_iocnak(q, iocbp, mp, EPROTO);
		break;
	    }
/* ONC_PLUS EXTRACT END */

	    switch (iocbp->ioc_cmd) {
	    default:
		    putnext(q, mp);
		    break;

	    case TI_BIND:
	    case TI_UNBIND:
	    case TI_OPTMGMT:
	    case TI_GETADDRS:

		    TILOG("timodwproc: TI_{BIND|UNBIND|OPTMGMT|GETADDRS\n", 0);
		    rc = tim_pullup(q, mp, iocbp);
		    if (rc != 0)
			    send_iocnak(q, iocbp, mp, rc);
		    else
			    tim_send_ioctl_tpi_msg(q, mp, tp, iocbp->ioc_cmd);
		    break;

	    case TI_GETINFO:
		    TILOG("timodwproc: TI_GETINFO\n", 0);
		    rc = tim_pullup(q, mp, iocbp);
		    if (rc != 0)
			    send_iocnak(q, iocbp, mp, rc);
		    else {
			    tp->tim_flags |= WAIT_IOCINFOACK;
			    tim_send_ioctl_tpi_msg(q, mp, tp, iocbp->ioc_cmd);
		    }
		    break;

	    case TI_SYNC: {
		    mblk_t *tsr_mp = mp->b_cont;
		    struct ti_sync_req *tsr =
			(struct ti_sync_req *)tsr_mp->b_rptr;
		    uint32_t tsr_flags = tsr->tsr_flags;

		    TILOG("timodwproc: TI_SYNC(%x)\n", tsr->tsr_flags);

		    rc = tim_pullup(q, mp, iocbp);
		    if (rc != 0) {
			    send_iocnak(q, iocbp, mp, rc);
			    break;
		    }

		    if ((tsr_mp->b_wptr - tsr_mp->b_rptr) <
			sizeof (struct ti_sync_req)) {
			    send_iocnak(q, iocbp, mp, EINVAL);
			    break;
		    }

		    if ((tsr->tsr_flags & TSRF_INFO_REQ) == 0) {
			    mblk_t *ack_mp = reallocb(tsr_mp,
				sizeof (struct ti_sync_ack), 0);

			    /* Can reply immediately. */
			    mp->b_cont = NULL;
			    if (ack_mp == NULL) {
				    tilog("timodwproc:allocb failed no "
					"recovery attempt\n", 0);
				    send_iocnak(q, iocbp, mp, ENOMEM);
			    } else
				    tim_answer_ti_sync(q, mp, tp, iocbp,
					ack_mp, tsr_flags);
			    break;
		    }

		/*
		 * This code is retained for compatibility with
		 * old statically linked applications. New code
		 * should use TI_CAPABILITY for all TPI
		 * information and should not use TSRF_INFO_REQ
		 * flag.
		 *
		 * defer processsing necessary to rput procedure
		 * as we need to get information from transport
		 * driver. Set flags that will tell the read
		 * side the work needed on this request.
		 */

		    if (tsr_flags & TSRF_IS_EXP_IN_RCVBUF)
			    tp->tim_flags |= PEEK_RDQ_EXPIND;

		/*
		 * Convert message to a T_INFO_REQ message
		 *
		 * This relies on the following assertion which
		 * happens to be always true
		 * ASSERT(sizeof (struct ti_sync_req) >=
		 *	sizeof (struct T_info_req));
		 */
		    ((struct T_info_req *)tsr_mp->b_rptr)->PRIM_type =
			T_INFO_REQ;
		    tsr_mp->b_wptr = tsr_mp->b_rptr +
			sizeof (struct T_info_req);
		    tp->tim_flags |= WAIT_IOCINFOACK;
		    tim_send_ioctl_tpi_msg(q, mp, tp, iocbp->ioc_cmd);
	    }
	    break;

	    case TI_CAPABILITY: {
		    mblk_t *tcsr_mp = mp->b_cont;
		    struct T_capability_req *tcr =
			    (struct T_capability_req *)tcsr_mp->b_rptr;

		    TILOG("timodwproc: TI_CAPABILITY(CAP_bits1 = %x)\n",
			tcr->CAP_bits1);

		    rc = tim_pullup(q, mp, iocbp);
		    if (rc != 0) {
			    send_iocnak(q, iocbp, mp, rc);
			    break;
		    }

		    if ((tcsr_mp->b_wptr - tcsr_mp->b_rptr) <
			sizeof (struct T_capability_req)) {
			    TILOG("timodwproc: msg too short\n", 0);
			    send_iocnak(q, iocbp, mp, EINVAL);
			    break;
		    }

		    if (tcr->PRIM_type != T_CAPABILITY_REQ) {
			    TILOG("timodwproc: invalid msg type %d\n",
				tcr->PRIM_type);
			    send_iocnak(q, iocbp, mp, EPROTO);
			    break;
		    }

		    switch (tp->tim_provinfo->tpi_capability) {
		    case PI_YES:
			    /* Just send T_CAPABILITY_REQ down */
			    tim_send_ioctl_tpi_msg(q, mp, tp, iocbp->ioc_cmd);
			    break;

		    case PI_DONTKNOW:
			/*
			 * It is unknown yet whether transport provides
			 * T_CAPABILITY_REQ or not. Send message down &
			 * wait for reply.
			 */
			    ASSERT(tp->tim_tcap_timoutid == 0);
			    if ((tcr->CAP_bits1 & TC1_INFO) == 0) {
				    tp->tim_flags |= TI_CAP_RECVD;
			    } else {
				    tp->tim_flags |= (TI_CAP_RECVD |
					CAP_WANTS_INFO);
			    }

			    tp->tim_tcap_timoutid = qtimeout(q, tim_tcap_timer,
				(caddr_t)q, tim_tcap_wait * hz);
			    tim_send_ioctl_tpi_msg(q, mp, tp, iocbp->ioc_cmd);
			    break;

		    case PI_NO:
			/*
			 * Transport doesn't support T_CAPABILITY_REQ.
			 * Either reply immediately or send T_INFO_REQ if
			 * needed.
			 */
			    if ((tcr->CAP_bits1 & TC1_INFO) != 0) {
				    tp->tim_flags |= (TI_CAP_RECVD |
					CAP_WANTS_INFO | WAIT_IOCINFOACK);
				    TILOG("timodwproc: sending down T_INFO_REQ,"
					" flags = %x\n", tp->tim_flags);

				/*
				 * Generate T_INFO_REQ message and send
				 * it down
				 */
				    ((struct T_info_req *)tcsr_mp->b_rptr)->
					PRIM_type = T_INFO_REQ;
				    tcsr_mp->b_wptr = tcsr_mp->b_rptr +
					sizeof (struct T_info_req);
				    tim_send_ioctl_tpi_msg(q, mp, tp,
					iocbp->ioc_cmd);
				    break;
			    }

			/*
			 * Can reply immediately. Just send back
			 * T_CAPABILITY_ACK with CAP_bits1 set to 0.
			 */
			    mp->b_cont = tcsr_mp = tpi_ack_alloc(mp->b_cont,
				sizeof (struct T_capability_ack), M_PCPROTO,
				T_CAPABILITY_ACK);

			    if (tcsr_mp == NULL) {
				    tilog("timodwproc:allocb failed no "
					"recovery attempt\n", 0);
				    send_iocnak(q, iocbp, mp, ENOMEM);
				    break;
			    }

			    tp->tim_flags &= ~(WAITIOCACK | WAIT_IOCINFOACK |
				TI_CAP_RECVD | CAP_WANTS_INFO);
			    ((struct T_capability_ack *)
				tcsr_mp->b_rptr)->CAP_bits1 = 0;
			    tim_ioctl_send_reply(q, mp, tcsr_mp);
			    tp->tim_iocsave = NULL;
			    break;

		    default:
			    cmn_err(CE_PANIC,
				"timodwproc: impossible tpi_capability value:"
				" %d\n", tp->tim_provinfo->tpi_capability);
			    break;
		    }
		}
		break;

/* ONC_PLUS EXTRACT START */
	    case TI_GETMYNAME:

		tilog("timodwproc: Got TI_GETMYNAME\n", 0);

		if (tp->tim_provinfo->tpi_myname == PI_YES) {
		    putnext(q, mp);
		    break;
		}
		goto getname;

	    case TI_GETPEERNAME:

		tilog("timodwproc: Got TI_GETPEERNAME\n", 0);

		if (tp->tim_provinfo->tpi_peername == PI_YES) {
		    putnext(q, mp);
		    break;
		}
getname:
		if ((tmp = copymsg(mp)) == NULL) {
		    t_scalar_t i = 0;

		    for (tmp = mp; tmp; tmp = tmp->b_cont)
			i += (t_scalar_t)(tmp->b_wptr - tmp->b_rptr);
		    tim_recover(q, mp, i);
		    return (1);
		}
		tp->tim_iocsave = mp;
		putnext(q, tmp);
		break;

	    case TI_SETMYNAME:

		tilog("timodwproc: Got TI_SETMYNAME\n", 0);

		if (tp->tim_provinfo->tpi_myname == PI_YES) {
			/*
			 * The transport provider supports the TI_GETMYNAME
			 * ioctl, so setting the name here won't be of any use.
			 */
			send_iocnak(q, iocbp, mp, EBUSY);
		} else if (iocbp->ioc_uid != 0) {
			/*
			 * Kludge ioctl for root only.  If TIMOD is pushed
			 * on a stream that is already "bound", we want
			 * to be able to support the TI_GETMYNAME ioctl if the
			 * transport provider doesn't support it.
			 */
			send_iocnak(q, iocbp, mp, EPERM);
		} else if (!tim_setname(q, mp)) {
			return (1);
		}
		break;

	    case TI_SETPEERNAME:

		tilog("timodwproc: Got TI_SETPEERNAME\n", 0);

		if (tp->tim_provinfo->tpi_peername == PI_YES) {
			/*
			 * The transport provider supports the TI_GETPEERNAME
			 * ioctl, so setting the name here won't be of any use.
			 */
			send_iocnak(q, iocbp, mp, EBUSY);
		} else if (iocbp->ioc_uid != 0) {
			/*
			 * Kludge ioctl for root only.  If TIMOD is pushed
			 * on a stream that is already "bound", we want
			 * to be able to support the TI_SETPEERNAME ioctl if the
			 * transport provider doesn't support it.
			 */
			send_iocnak(q, iocbp, mp, EPERM);
		} else if (!tim_setname(q, mp)) {
			return (1);
		}
		break;
	    }
	    break;

	case M_IOCDATA:

	    tilog("timodwproc: Got TI_SETPEERNAME\n", 0);

	    if (tp->tim_flags & NAMEPROC) {
		if (ti_doname(q, mp, tp->tim_myname, tp->tim_mylen,
		    tp->tim_peername, tp->tim_peerlen) != DONAME_CONT) {
			tp->tim_flags &= ~NAMEPROC;
		}
	    } else
		putnext(q, mp);
	    break;

	case M_PROTO:
	case M_PCPROTO:
	    /* assert checks if there is enough data to determine type */
	    if ((mp->b_wptr - mp->b_rptr) < sizeof (t_scalar_t)) {
		send_ERRORW(q, mp);
		return (1);
	    }

	    pptr = (union T_primitives *)mp->b_rptr;
	    switch (pptr->type) {
	    default:
		putnext(q, mp);
		break;

	    case T_EXDATA_REQ:
	    case T_DATA_REQ:
		if (pptr->type == T_EXDATA_REQ)
			tilog("timodwproc: Got T_EXDATA_REQ\n", 0);

		if (!bcanput(q->q_next, mp->b_band)) {
			(void) putbq(q, mp);
			return (1);
		}
		putnext(q, mp);
		break;
/* ONC_PLUS EXTRACT END */

	    case T_UNITDATA_REQ:
		if (tp->tim_flags & CLTS) {
		    if ((tmp = tim_filladdr(q, mp)) == NULL) {
			tim_recover(q, mp,
			    ((t_scalar_t)sizeof (struct T_unitdata_req)) +
							tp->tim_peerlen);
			return (1);
		    } else {
			mp = tmp;
		    }
		}
#ifdef C2_AUDIT
		if (audit_active)
		    audit_sock(T_UNITDATA_REQ, q, mp, TIMOD_ID);
#endif
		if (!bcanput(q->q_next, mp->b_band)) {
		    (void) putbq(q, mp);
		    return (1);
		}
		putnext(q, mp);
		break;

/* ONC_PLUS EXTRACT START */
	    case T_CONN_REQ: {
		struct T_conn_req *reqp = (struct T_conn_req *)mp->b_rptr;
		caddr_t p;

		tilog("timodwproc: Got T_CONN_REQ\n", 0);

		if ((mp->b_wptr - mp->b_rptr) < sizeof (struct T_conn_req)) {
			send_ERRORW(q, mp);
			return (1);
		}

		if (tp->tim_flags & DO_PEERNAME) {
		    if (!OK_offsets(mp, reqp->DEST_offset, reqp->DEST_length)) {
			send_ERRORW(q, mp);
			return (1);
		    }
		    ASSERT(reqp->DEST_length >= 0);
		    if (reqp->DEST_length > tp->tim_peermaxlen) {
			p = kmem_alloc((size_t)reqp->DEST_length, KM_NOSLEEP);
			if (p == NULL) {

				tilog(
			"timodwproc: kmem_alloc failed attempt recovery\n", 0);

			    tim_recover(q, mp, reqp->DEST_length);
			    return (1);
			}
			if (tp->tim_peermaxlen)
				kmem_free(tp->tim_peername,
				    (size_t)tp->tim_peermaxlen);
			tp->tim_peername = p;
			tp->tim_peermaxlen = reqp->DEST_length;
		    }
		    tp->tim_peerlen = reqp->DEST_length;
		    p = (caddr_t)mp->b_rptr + reqp->DEST_offset;
		    bcopy(p, tp->tim_peername, (size_t)tp->tim_peerlen);
		    if (tp->tim_flags & COTS)
			tp->tim_flags |= CONNWAIT;
		}
/* ONC_PLUS EXTRACT END */
#ifdef C2_AUDIT
	if (audit_active)
		audit_sock(T_CONN_REQ, q, mp, TIMOD_ID);
#endif
/* ONC_PLUS EXTRACT START */
		putnext(q, mp);
		break;
	    }

	    case O_T_CONN_RES:
	    case T_CONN_RES: {
		struct T_conn_res *resp;
		struct T_conn_ind *indp;
		mblk_t *pmp = NULL;
		struct tim_tim *ntp;
		caddr_t p;

		if ((mp->b_wptr - mp->b_rptr) < sizeof (struct T_conn_res)) {
			send_ERRORW(q, mp);
			return (1);
		}

		resp = (struct T_conn_res *)mp->b_rptr;
		for (tmp = tp->tim_consave; tmp; tmp = tmp->b_next) {
			indp = (struct T_conn_ind *)tmp->b_rptr;
			if (indp->SEQ_number == resp->SEQ_number)
				break;
			pmp = tmp;
		}
		if (!tmp)
			goto cresout;
		if (pmp)
			pmp->b_next = tmp->b_next;
		else
			tp->tim_consave = tmp->b_next;
		tmp->b_next = NULL;

		rw_enter(&tim_list_rwlock, RW_READER);
		if ((ntp = tim_findlink(resp->ACCEPTOR_id)) == NULL) {
			rw_exit(&tim_list_rwlock);
			goto cresout;
		}
		if (ntp->tim_flags & DO_PEERNAME) {
		    ASSERT(indp->SRC_length >= 0);
		    if (indp->SRC_length > ntp->tim_peermaxlen) {
			p = kmem_alloc((size_t)indp->SRC_length, KM_NOSLEEP);
			if (p == NULL) {

				tilog(
			"timodwproc: kmem_alloc failed attempt recovery\n", 0);

			    tmp->b_next = tp->tim_consave;
			    tp->tim_consave = tmp;
			    tim_recover(q, mp, indp->SRC_length);
			    rw_exit(&tim_list_rwlock);
			    return (1);
			}
			if (ntp->tim_peermaxlen)
			    kmem_free(ntp->tim_peername,
					(size_t)ntp->tim_peermaxlen);
			ntp->tim_peername = p;
			ntp->tim_peermaxlen = indp->SRC_length;
		    }
		    ntp->tim_peerlen = indp->SRC_length;
		    p = (caddr_t)tmp->b_rptr + indp->SRC_offset;
		    bcopy(p, ntp->tim_peername, (size_t)ntp->tim_peerlen);
		}
		rw_exit(&tim_list_rwlock);
cresout:
		freemsg(tmp);
		putnext(q, mp);
		break;
	    }

/* ONC_PLUS EXTRACT END */
	    case T_DISCON_REQ: {
		struct T_discon_req *disp;
		struct T_conn_ind *conp;
		mblk_t *pmp = NULL;

		if ((mp->b_wptr - mp->b_rptr) < sizeof (struct T_discon_req)) {
			send_ERRORW(q, mp);
			return (1);
		}

		disp = (struct T_discon_req *)mp->b_rptr;
		tp->tim_flags &= ~(CONNWAIT|LOCORDREL|REMORDREL);
		tp->tim_peerlen = 0;

		/*
		 * If we are already connected, there won't
		 * be any messages on tim_consave.
		 */
		for (tmp = tp->tim_consave; tmp; tmp = tmp->b_next) {
		    conp = (struct T_conn_ind *)tmp->b_rptr;
		    if (conp->SEQ_number == disp->SEQ_number)
			break;
		    pmp = tmp;
		}
		if (tmp) {
		    if (pmp)
			pmp->b_next = tmp->b_next;
		    else
			tp->tim_consave = tmp->b_next;
		    tmp->b_next = NULL;
		    freemsg(tmp);
		}
		putnext(q, mp);
		break;
	    }

	    case T_ORDREL_REQ:
		if (tp->tim_flags & REMORDREL) {
		    tp->tim_flags &= ~(LOCORDREL|REMORDREL);
		    tp->tim_peerlen = 0;
		} else {
		    tp->tim_flags |= LOCORDREL;
		}
		putnext(q, mp);
		break;

	    case T_CAPABILITY_REQ:
		tilog("timodwproc: Got T_CAPABILITY_REQ\n", 0);
		/*
		 * XXX: We may know at this point whether transport provides
		 * T_CAPABILITY_REQ or not and we may utilise this knowledge
		 * here.
		 */
		putnext(q, mp);
		break;
/* ONC_PLUS EXTRACT START */
	    }
	    break;
	case M_FLUSH:

	    tilog("timodwproc: Got M_FLUSH\n", 0);

	    if (*mp->b_rptr & FLUSHW) {
		if (*mp->b_rptr & FLUSHBAND)
			flushband(q, *(mp->b_rptr + 1), FLUSHDATA);
		else
			flushq(q, FLUSHDATA);
	    }
	    putnext(q, mp);
	    break;
	}

	return (0);
}

static int
OK_offsets(mblk_t *mp, long offset, long length)
{
	caddr_t p;

	p = (caddr_t)mp->b_rptr + offset;
	if (p < (caddr_t)mp->b_rptr || p > (caddr_t)mp->b_wptr)
		return (0);

	if (length < 0)
		return (0);

	p += length;
	if (p < (caddr_t)mp->b_rptr || p > (caddr_t)mp->b_wptr)
		return (0);

	return (1);
}

static void
send_ERRORW(queue_t *q, mblk_t *mp)
{
	mblk_t *mp1;

	if ((MBLKSIZE(mp) < 1) || (DB_REF(mp) > 1)) {
		mp1 = allocb(1, BPRI_HI);
		if (!mp1) {
		    tilog(
			"timodrproc: allocb failed attempt recovery\n", 0);
		    tim_recover(q, mp, 1);
		    return;
		}
		freemsg(mp);
		mp = mp1;
	} else if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	}

	mp->b_datap->db_type = M_ERROR;
	mp->b_rptr = mp->b_datap->db_base;
	*mp->b_rptr = EPROTO;
	mp->b_wptr = mp->b_rptr + sizeof (char);
	qreply(q, mp);
}

static void
tilog(char *str, t_scalar_t arg)
{
	if (dotilog) {
		if (dotilog & 2)
			cmn_err(CE_CONT, str, arg);
		if (dotilog & 4)
			(void) strlog(TIMOD_ID, -1, 0, SL_TRACE | SL_ERROR,
			    str, arg);
		else
			(void) strlog(TIMOD_ID, -1, 0, SL_TRACE, str, arg);
	}
}

static void
tilogp(char *str, uintptr_t arg)
{
	if (dotilog) {
		if (dotilog & 2)
			cmn_err(CE_CONT, str, arg);
		if (dotilog & 4)
			(void) strlog(TIMOD_ID, -1, 0, SL_TRACE | SL_ERROR,
			    str, arg);
		else
			(void) strlog(TIMOD_ID, -1, 0, SL_TRACE, str, arg);
	}
}


/*
 * Process the TI_GETNAME ioctl.  If no name exists, return len = 0
 * in strbuf structures.  The state transitions are determined by what
 * is hung of cq_private (cp_private) in the copyresp (copyreq) structure.
 * The high-level steps in the ioctl processing are as follows:
 *
 * 1) we recieve an transparent M_IOCTL with the arg in the second message
 *	block of the message.
 * 2) we send up an M_COPYIN request for the strbuf structure pointed to
 *	by arg.  The block containing arg is hung off cq_private.
 * 3) we receive an M_IOCDATA response with cp->cp_private->b_cont == NULL.
 *	This means that the strbuf structure is found in the message block
 *	mp->b_cont.
 * 4) we send up an M_COPYOUT request with the strbuf message hung off
 *	cq_private->b_cont.  The address we are copying to is strbuf.buf.
 *	we set strbuf.len to 0 to indicate that we should copy the strbuf
 *	structure the next time.  The message mp->b_cont contains the
 *	address info.
 * 5) we receive an M_IOCDATA with cp_private->b_cont != NULL and
 *	strbuf.len == 0.  Restore strbuf.len to either llen ot rlen.
 * 6) we send up an M_COPYOUT request with a copy of the strbuf message
 *	hung off mp->b_cont.  In the strbuf structure in the message hung
 *	off cq_private->b_cont, we set strbuf.len to 0 and strbuf.maxlen
 *	to 0.  This means that the next step is to ACK the ioctl.
 * 7) we receive an M_IOCDATA message with cp_private->b_cont != NULL and
 *	strbuf.len == 0 and strbuf.maxlen == 0.  Free up cp->private and
 *	send an M_IOCACK upstream, and we are done.
 *
 */
static int
ti_doname(
	queue_t *q,		/* queue message arrived at */
	mblk_t *mp,		/* M_IOCTL or M_IOCDATA message only */
	caddr_t lname,		/* local name */
	t_uscalar_t llen,	/* length of local name (0 if not set) */
	caddr_t rname,		/* remote name */
	t_uscalar_t rlen)	/* length of remote name (0 if not set) */
{
	struct iocblk *iocp;
	struct copyreq *cqp;
	struct copyresp *csp;
	struct strbuf *np;
	int ret;
	mblk_t *bp;

	ASSERT((t_scalar_t)llen >= 0);
	ASSERT((t_scalar_t)rlen >= 0);

	switch (mp->b_datap->db_type) {
	case M_IOCTL:
		iocp = (struct iocblk *)mp->b_rptr;
		if ((iocp->ioc_cmd != TI_GETMYNAME) &&
		    (iocp->ioc_cmd != TI_GETPEERNAME)) {
			cmn_err(CE_WARN, "ti_doname: bad M_IOCTL command\n");
			send_iocnak(q, iocp, mp, EINVAL);
			ret = DONAME_FAIL;
			break;
		}
		if ((iocp->ioc_count != TRANSPARENT) ||
		    (mp->b_cont == NULL) || ((mp->b_cont->b_wptr -
		    mp->b_cont->b_rptr) != sizeof (intptr_t))) {
			send_iocnak(q, iocp, mp, EINVAL);
			ret = DONAME_FAIL;
			break;
		}
		cqp = (struct copyreq *)mp->b_rptr;
		cqp->cq_private = mp->b_cont;
		cqp->cq_addr = (caddr_t)*(intptr_t *)mp->b_cont->b_rptr;
		mp->b_cont = NULL;
		cqp->cq_size = sizeof (struct strbuf);
		cqp->cq_flag = 0;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
		qreply(q, mp);
		ret = DONAME_CONT;
		break;

	case M_IOCDATA:
		csp = (struct copyresp *)mp->b_rptr;
		iocp = (struct iocblk *)mp->b_rptr;
		cqp = (struct copyreq *)mp->b_rptr;
		if ((csp->cp_cmd != TI_GETMYNAME) &&
		    (csp->cp_cmd != TI_GETPEERNAME)) {
			cmn_err(CE_WARN, "ti_doname: bad M_IOCDATA command\n");
			send_iocnak(q, iocp, mp, EINVAL);
			ret = DONAME_FAIL;
			break;
		}
		if (csp->cp_rval) {	/* error */
			freemsg(csp->cp_private);
			freemsg(mp);
			ret = DONAME_FAIL;
			break;
		}
		ASSERT(csp->cp_private != NULL);
		if (csp->cp_private->b_cont == NULL) {	/* got strbuf */
			ASSERT(mp->b_cont);
			np = (struct strbuf *)mp->b_cont->b_rptr;
			if (csp->cp_cmd == TI_GETMYNAME) {
				if (llen == 0) {
					np->len = 0;	/* copy just strbuf */
				} else if (llen > np->maxlen) {
					freemsg(csp->cp_private);
					send_iocnak(q, iocp, mp, ENAMETOOLONG);
					ret = DONAME_FAIL;
					break;
				} else {
					np->len = llen;	/* copy buffer */
				}
			} else {	/* REMOTENAME */
				if (rlen == 0) {
					np->len = 0;	/* copy just strbuf */
				} else if (rlen > np->maxlen) {
					send_iocnak(q, iocp, mp, ENAMETOOLONG);
					ret = DONAME_FAIL;
					break;
				} else {
					np->len = rlen;	/* copy buffer */
				}
			}
			csp->cp_private->b_cont = mp->b_cont;
			mp->b_cont = NULL;
		}
		np = (struct strbuf *)csp->cp_private->b_cont->b_rptr;
		if (np->len == 0) {
			if (np->maxlen == 0) {

				/*
				 * ack the ioctl
				 */
				freemsg(csp->cp_private);
				tim_ioctl_send_reply(q, mp, NULL);
				ret = DONAME_DONE;
				break;
			}

			/*
			 * copy strbuf to user
			 */
			if (csp->cp_cmd == TI_GETMYNAME)
				np->len = llen;
			else	/* TI_GETPEERNAME */
				np->len = rlen;
			if ((bp = allocb(sizeof (struct strbuf), BPRI_MED))
			    == NULL) {

				tilog(
			"ti_doname: allocb failed no recovery attempt\n", 0);

				freemsg(csp->cp_private);
				send_iocnak(q, iocp, mp, EAGAIN);
				ret = DONAME_FAIL;
				break;
			}
			bp->b_wptr += sizeof (struct strbuf);
			bcopy(np, bp->b_rptr, sizeof (struct strbuf));
			cqp->cq_addr =
			    (caddr_t)*(intptr_t *)csp->cp_private->b_rptr;
			cqp->cq_size = sizeof (struct strbuf);
			cqp->cq_flag = 0;
			mp->b_datap->db_type = M_COPYOUT;
			mp->b_cont = bp;
			np->len = 0;
			np->maxlen = 0; /* ack next time around */
			qreply(q, mp);
			ret = DONAME_CONT;
			break;
		}

		/*
		 * copy the address to the user
		 */
		if ((bp = allocb((size_t)np->len, BPRI_MED)) == NULL) {

		    tilog("ti_doname: allocb failed no recovery attempt\n", 0);

		    freemsg(csp->cp_private);
		    send_iocnak(q, iocp, mp, EAGAIN);
		    ret = DONAME_FAIL;
		    break;
		}
		bp->b_wptr += np->len;
		if (csp->cp_cmd == TI_GETMYNAME)
			bcopy(lname, bp->b_rptr, (size_t)llen);
		else	/* TI_GETPEERNAME */
			bcopy(rname, bp->b_rptr, (size_t)rlen);
		cqp->cq_addr = (caddr_t)np->buf;
		cqp->cq_size = np->len;
		cqp->cq_flag = 0;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_cont = bp;
		np->len = 0;	/* copy the strbuf next time around */
		qreply(q, mp);
		ret = DONAME_CONT;
		break;

	default:
		cmn_err(CE_WARN,
		    "ti_doname: freeing bad message type = %d\n",
		    mp->b_datap->db_type);
		freemsg(mp);
		ret = DONAME_FAIL;
		break;
	}
	return (ret);
}

static int
tim_setname(queue_t *q, mblk_t *mp)
{
	struct iocblk *iocp;
	struct copyreq *cqp;
	struct copyresp *csp;
	struct tim_tim *tp;
	struct strbuf *netp;
	t_uscalar_t len;
	caddr_t p;

	tp = (struct tim_tim *)q->q_ptr;
	iocp = (struct iocblk *)mp->b_rptr;
	cqp = (struct copyreq *)mp->b_rptr;
	csp = (struct copyresp *)mp->b_rptr;

	switch (mp->b_datap->db_type) {
	case M_IOCTL:
		if ((iocp->ioc_cmd != TI_SETMYNAME) &&
		    (iocp->ioc_cmd != TI_SETPEERNAME)) {
			cmn_err(CE_PANIC, "ti_setname: bad M_IOCTL command\n");
		}
		if ((iocp->ioc_count != TRANSPARENT) ||
		    (mp->b_cont == NULL) || ((mp->b_cont->b_wptr -
		    mp->b_cont->b_rptr) != sizeof (intptr_t))) {
			send_iocnak(q, iocp, mp, EINVAL);
			break;
		}
		cqp->cq_addr = (caddr_t)*(intptr_t *)mp->b_cont->b_rptr;
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
		cqp->cq_size = sizeof (struct strbuf);
		cqp->cq_flag = 0;
		cqp->cq_private = NULL;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
		qreply(q, mp);
		break;

	case M_IOCDATA:
		if (csp->cp_rval) {
			freemsg(mp);
			break;
		}
		if (csp->cp_private == NULL) {	/* got strbuf */
			netp = (struct strbuf *)mp->b_cont->b_rptr;
			csp->cp_private = mp->b_cont;
			mp->b_cont = NULL;
			cqp->cq_addr = netp->buf;
			cqp->cq_size = netp->len;
			cqp->cq_flag = 0;
			mp->b_datap->db_type = M_COPYIN;
			mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
			qreply(q, mp);
			break;
		} else {			/* got addr */
			len = (t_uscalar_t)msgdsize(mp->b_cont);
			if (csp->cp_cmd == TI_SETMYNAME) {
				ASSERT(tp->tim_mymaxlen >= 0);
				if (len > tp->tim_mymaxlen) {
					p = kmem_alloc((size_t)len, KM_NOSLEEP);
					if (p == NULL) {

				tilog(
			"tim_setname: kmem_alloc failed attempt recovery\n", 0);

					    tim_recover(q, mp, len);
					    return (0);
					}
					if (tp->tim_mymaxlen)
					    kmem_free(tp->tim_myname,
						(size_t)tp->tim_mymaxlen);
					tp->tim_myname = p;
					tp->tim_mymaxlen = len;
				}
				tp->tim_mylen = len;
				tim_bcopy(mp->b_cont, tp->tim_myname, len);
			} else if (csp->cp_cmd == TI_SETPEERNAME) {
				ASSERT(tp->tim_peermaxlen >= 0);
				if (len > tp->tim_peermaxlen) {
					p = kmem_alloc((size_t)len, KM_NOSLEEP);
					if (p == NULL) {

				tilog(
			"tim_setname: kmem_alloc failed attempt recovery\n", 0);

						tim_recover(q, mp, len);
						return (0);
					}
					if (tp->tim_peermaxlen)
					    kmem_free(tp->tim_peername,
						(size_t)tp->tim_peermaxlen);
					tp->tim_peername = p;
					tp->tim_peermaxlen = len;
				}
				tp->tim_peerlen = len;
				tim_bcopy(mp->b_cont, tp->tim_peername, len);
			} else {
				cmn_err(CE_PANIC,
				    "ti_setname: bad M_IOCDATA command\n");
			}
			freemsg(csp->cp_private);
			tim_ioctl_send_reply(q, mp, NULL);
			qreply(q, mp);
		}
		break;

	default:
		cmn_err(CE_PANIC, "ti_setname: bad message type = %d\n",
		    mp->b_datap->db_type);
	}
	return (1);
}

/*
 * Copy data from a message to a buffer taking into account
 * the possibility of the data being split between multiple
 * message blocks.
 */
static void
tim_bcopy(mblk_t *frommp, caddr_t to, t_uscalar_t len)
{
	mblk_t *mp;
	t_uscalar_t size;

	mp = frommp;
	while (mp && len) {
		size = MIN((t_uscalar_t)(mp->b_wptr - mp->b_rptr), len);
		bcopy(mp->b_rptr, to, (size_t)size);
		len -= size;
		to += size;
		mp = mp->b_cont;
	}
}
/* ONC_PLUS EXTRACT END */

/*
 * Fill in the address of a connectionless data packet if a connect
 * had been done on this endpoint.
 */
static mblk_t *
tim_filladdr(queue_t *q, mblk_t *mp)
{
	mblk_t *bp;
	struct tim_tim *tp;
	struct T_unitdata_req *up;
	struct T_unitdata_req *nup;

	tp = (struct tim_tim *)q->q_ptr;
	if (mp->b_datap->db_type == M_DATA) {
		bp = allocb(sizeof (struct T_unitdata_req) + tp->tim_peerlen,
		    BPRI_MED);
		if (bp == NULL) {

		    tilog("tim_filladdr: allocb failed no recovery attempt\n",
				0);

		    return (bp);
		}
		bp->b_datap->db_type = M_PROTO;
		up = (struct T_unitdata_req *)bp->b_rptr;
		up->PRIM_type = T_UNITDATA_REQ;
		up->DEST_length = tp->tim_peerlen;
		bp->b_wptr += sizeof (struct T_unitdata_req);
		up->DEST_offset = (t_scalar_t)sizeof (struct T_unitdata_req);
		up->OPT_length = 0;
		up->OPT_offset = 0;
		if (tp->tim_peerlen) {
			bcopy(tp->tim_peername, bp->b_wptr,
			    (size_t)tp->tim_peerlen);
			bp->b_wptr += tp->tim_peerlen;
		}
		bp->b_cont = mp;
		return (bp);
	} else {
		ASSERT(mp->b_datap->db_type == M_PROTO);
		up = (struct T_unitdata_req *)mp->b_rptr;
		ASSERT(up->PRIM_type == T_UNITDATA_REQ);
		if (up->DEST_length != 0)
			return (mp);
		bp = allocb(sizeof (struct T_unitdata_req) + up->OPT_length +
		    tp->tim_peerlen, BPRI_MED);
		if (bp == NULL) {

		    tilog("tim_filladdr: allocb failed no recovery attempt\n",
				0);

			return (NULL);
		}
		bp->b_datap->db_type = M_PROTO;
		nup = (struct T_unitdata_req *)bp->b_rptr;
		nup->PRIM_type = T_UNITDATA_REQ;
		nup->DEST_length = tp->tim_peerlen;
		bp->b_wptr += sizeof (struct T_unitdata_req);
		nup->DEST_offset = (t_scalar_t)sizeof (struct T_unitdata_req);
		if (tp->tim_peerlen) {
		    bcopy(tp->tim_peername, bp->b_wptr,
					(size_t)tp->tim_peerlen);
		    bp->b_wptr += tp->tim_peerlen;
		}
		if (up->OPT_length == 0) {
			nup->OPT_length = 0;
			nup->OPT_offset = 0;
		} else {
			nup->OPT_length = up->OPT_length;
			nup->OPT_offset =
				(t_scalar_t)sizeof (struct T_unitdata_req) +
					tp->tim_peerlen;
			bcopy((mp->b_wptr + up->OPT_offset),
			    bp->b_wptr, (size_t)up->OPT_length);
			bp->b_wptr += up->OPT_length;
		}
		bp->b_cont = mp->b_cont;
		mp->b_cont = NULL;
		freeb(mp);
		return (bp);
	}
}

static void
tim_addlink(struct tim_tim *tp)
{
	struct tim_tim **tpp;
	struct tim_tim	*next;

	tpp = &tim_hash[TIM_HASH(tp->tim_acceptor)];
	rw_enter(&tim_list_rwlock, RW_WRITER);

	if ((next = *tpp) != NULL)
		next->tim_ptpn = &tp->tim_next;
	tp->tim_next = next;
	tp->tim_ptpn = tpp;
	*tpp = tp;

	tim_cnt++;

	rw_exit(&tim_list_rwlock);
}

static void
tim_dellink(struct tim_tim *tp)
{
	struct tim_tim	*next;

	rw_enter(&tim_list_rwlock, RW_WRITER);

	if ((next = tp->tim_next) != NULL)
		next->tim_ptpn = tp->tim_ptpn;
	*(tp->tim_ptpn) = next;

	tim_cnt--;
	if (tp->tim_rdq != NULL)
		tp->tim_rdq->q_ptr = WR(tp->tim_rdq)->q_ptr = NULL;

	kmem_free(tp, sizeof (struct tim_tim));

	rw_exit(&tim_list_rwlock);
}

static struct tim_tim *
tim_findlink(t_uscalar_t id)
{
	struct tim_tim	*tp;

	ASSERT(rw_lock_held(&tim_list_rwlock));

	for (tp = tim_hash[TIM_HASH(id)]; tp != NULL; tp = tp->tim_next) {
		if (tp->tim_acceptor == id) {
			break;
		}
	}
	return (tp);
}

/* ONC_PLUS EXTRACT START */
static void
tim_recover(queue_t *q, mblk_t *mp, t_scalar_t size)
{
	struct tim_tim	*tp;
	bufcall_id_t	bid;
	timeout_id_t	tid;

	tp = (struct tim_tim *)q->q_ptr;

	/*
	 * Avoid re-enabling the queue.
	 */
	if (mp->b_datap->db_type == M_PCPROTO)
		mp->b_datap->db_type = M_PROTO;
	noenable(q);
	(void) putbq(q, mp);

	/*
	 * Make sure there is at most one outstanding request per queue.
	 */
	if (q->q_flag & QREADR) {
		if (tp->tim_rtimoutid || tp->tim_rbufcid)
			return;
	} else {
		if (tp->tim_wtimoutid || tp->tim_wbufcid)
			return;
	}
	if (!(bid = qbufcall(RD(q), (size_t)size, BPRI_MED, tim_buffer, q))) {
		tid = qtimeout(RD(q), tim_timer, q, TIMWAIT);
		if (q->q_flag & QREADR)
			tp->tim_rtimoutid = tid;
		else
			tp->tim_wtimoutid = tid;
	} else	{
		if (q->q_flag & QREADR)
			tp->tim_rbufcid = bid;
		else
			tp->tim_wbufcid = bid;
	}
}

/*
 * Inspect the data on read queues starting from read queues passed as
 * paramter (timod read queue) and traverse until
 * q_next is NULL (stream head). Look for a TPI T_EXDATA_IND message
 * reutrn 1 if found, 0 if not found.
 */
static int
ti_expind_on_rdqueues(queue_t *rq)
{
	mblk_t *bp;

	do {
		/*
		 * Hold QLOCK while referencing data on queues
		 */
		mutex_enter(QLOCK(rq));
		bp = rq->q_first;
		while (bp != NULL) {
			/*
			 * Walk the messages on the queue looking
			 * for a possible T_EXDATA_IND
			 */
			if ((bp->b_datap->db_type == M_PROTO) &&
			    ((bp->b_wptr - bp->b_rptr) >=
				sizeof (struct T_exdata_ind)) &&
			    (((struct T_exdata_ind *)bp->b_rptr)->PRIM_type
				== T_EXDATA_IND)) {
				/* bp is T_EXDATA_IND */
				mutex_exit(QLOCK(rq));
				return (1); /* expdata is on a read queue */
			}
			bp = bp->b_next; /* next message */
		}
		mutex_exit(QLOCK(rq));
		rq = rq->q_next;	/* next upstream queue */
	} while (rq != NULL);
	return (0);		/* no expdata on read queues */
}

/* ONC_PLUS EXTRACT END */
static void
tim_tcap_timer(void *q_ptr)
{
	queue_t *q = (queue_t *)q_ptr;
	struct tim_tim *tp = (struct tim_tim *)q->q_ptr;

	ASSERT(tp != NULL && tp->tim_tcap_timoutid != 0);
	ASSERT((tp->tim_flags & TI_CAP_RECVD) != 0);

	tp->tim_tcap_timoutid = 0;
	TILOG("tim_tcap_timer: fired\n", 0);
	tim_tcap_genreply(q, tp);
}

/*
 * tim_tcap_genreply() is called either from timeout routine or when
 * T_ERROR_ACK is received. In both cases it means that underlying
 * transport doesn't provide T_CAPABILITY_REQ.
 */
static void
tim_tcap_genreply(queue_t *q, struct tim_tim *tp)
{
	mblk_t		*mp = tp->tim_iocsave;
	struct iocblk	*iocbp;

	TILOG("timodrproc: tim_tcap_genreply\n", 0);

	ASSERT(tp == (struct tim_tim *)q->q_ptr);
	ASSERT(mp != NULL);

	iocbp = (struct iocblk *)mp->b_rptr;
	ASSERT(iocbp != NULL);
	ASSERT((mp->b_wptr - mp->b_rptr) == sizeof (struct iocblk));
	ASSERT(iocbp->ioc_cmd == TI_CAPABILITY);

	/* Save this information permanently in the module */
	PI_PROVLOCK(tp->tim_provinfo);
	if (tp->tim_provinfo->tpi_capability == PI_DONTKNOW)
		tp->tim_provinfo->tpi_capability = PI_NO;
	PI_PROVUNLOCK(tp->tim_provinfo);

	if (tp->tim_tcap_timoutid != 0) {
		(void) quntimeout(q, tp->tim_tcap_timoutid);
		tp->tim_tcap_timoutid = 0;
	}

	if ((tp->tim_flags & CAP_WANTS_INFO) != 0) {
		/* Send T_INFO_REQ down */
		mblk_t *tirmp = tpi_ack_alloc(mp->b_cont,
		    sizeof (struct T_info_req), M_PCPROTO, T_INFO_REQ);

		if (tirmp != NULL) {
			/* Emulate TC1_INFO */
			TILOG("emulate_tcap_ioc_req: sending T_INFO_REQ\n", 0);
			tp->tim_flags |= WAIT_IOCINFOACK;
			putnext(WR(q), tirmp);
		} else {
			tilog("emulate_tcap_req: allocb fail, "
			    "no recovery attmpt\n", 0);
			tp->tim_flags &= ~(TI_CAP_RECVD | WAITIOCACK |
			    CAP_WANTS_INFO | WAIT_IOCINFOACK);
			send_iocnak(q, iocbp, mp, ENOMEM);
		}
	} else {
		/* Reply immediately */
		mblk_t *ackmp = tpi_ack_alloc(mp->b_cont,
		    sizeof (struct T_capability_ack), M_PCPROTO,
		    T_CAPABILITY_ACK);

		mp->b_cont = ackmp;

		if (ackmp != NULL) {
			((struct T_capability_ack *)
			    ackmp->b_rptr)->CAP_bits1 = 0;
			tim_ioctl_send_reply(q, mp, ackmp);
			tp->tim_iocsave = NULL;
			tp->tim_flags &= ~(WAITIOCACK | WAIT_IOCINFOACK |
			    TI_CAP_RECVD | CAP_WANTS_INFO);
		} else {
			tilog("timodwproc:allocb failed no "
			    "recovery attempt\n", 0);
			tp->tim_flags &= ~(TI_CAP_RECVD | WAITIOCACK |
			    CAP_WANTS_INFO | WAIT_IOCINFOACK);
			send_iocnak(q, iocbp, mp, ENOMEM);
		}
	}
}


static void
tim_ioctl_send_reply(queue_t *q, mblk_t *ioc_mp, mblk_t *mp)
{
	struct iocblk	*iocbp;
	mblk_t		*tmp = mp;

	ASSERT(q != NULL && ioc_mp != NULL);

	ioc_mp->b_datap->db_type = M_IOCACK;
	if (mp != NULL)
		mp->b_datap->db_type = M_DATA;

	if (ioc_mp->b_cont != mp) {
		/* It is safe to call freemsg for NULL pointers */
		freemsg(ioc_mp->b_cont);
		ioc_mp->b_cont = mp;
	}
	iocbp = (struct iocblk *)ioc_mp->b_rptr;
	iocbp->ioc_error = 0;
	iocbp->ioc_rval = 0;
	/*
	 * All ioctl's may return more data than was specified by
	 * count arg. For TI_CAPABILITY count is treated as maximum data size.
	 */
	if (mp == NULL)
		iocbp->ioc_count = 0;
	else if (iocbp->ioc_cmd != TI_CAPABILITY)
		for (iocbp->ioc_count = 0; tmp != NULL; tmp = tmp->b_cont)
			iocbp->ioc_count += tmp->b_wptr - tmp->b_rptr;
	else {
		iocbp->ioc_count = MIN(mp->b_wptr - mp->b_rptr,
		    iocbp->ioc_count);
		/* Truncate message if too large */
		mp->b_wptr = mp->b_rptr + iocbp->ioc_count;
	}

	TILOG("iosendreply: ioc_cmd = %d, ", iocbp->ioc_cmd);
	putnext(RD(q), ioc_mp);
}

/*
 * Send M_IOCACK for errors.
 */
static void
tim_send_ioc_error_ack(queue_t *q, struct tim_tim *tp, mblk_t *mp)
{
	struct T_error_ack *tea = (struct T_error_ack *)mp->b_rptr;
	t_scalar_t error_prim;

	ASSERT((mp->b_wptr - mp->b_rptr) == sizeof (struct T_error_ack));
	error_prim = tea->ERROR_prim;

	ASSERT(tp->tim_iocsave != NULL);

	/* mp may be reused b_cont part of saved ioctl. */
	if (tp->tim_iocsave->b_cont != mp)
		freemsg(tp->tim_iocsave->b_cont);
	tp->tim_iocsave->b_cont = NULL;

	/* Always send this to the read side of the queue */
	q = RD(q);

	TILOG("tim_send_ioc_error_ack: prim = %d\n", tp->tim_saved_prim);

	if (tp->tim_saved_prim != error_prim)
		putnext(q, mp);
	else if (error_prim == T_CAPABILITY_REQ) {
		TILOG("timodrproc: T_ERROR_ACK/T_CAPABILITY_REQ\n", 0);
		tim_tcap_genreply(q, tp);
	} else {
		struct iocblk *iocbp = (struct iocblk *)tp->tim_iocsave->b_rptr;

		TILOG("tim_send_ioc_error_ack: T_ERROR_ACK: prim %d\n",
		    error_prim);

		switch (error_prim) {
		default:
			TILOG("timodrproc: Unknown T_ERROR_ACK:  tlierror %d\n",
			    tea->TLI_error);

			putnext(q, mp);
			break;

		case T_INFO_REQ:
		case T_SVR4_OPTMGMT_REQ:
		case T_OPTMGMT_REQ:
		case O_T_BIND_REQ:
		case T_BIND_REQ:
		case T_UNBIND_REQ:
		case T_ADDR_REQ:
		case T_CAPABILITY_REQ:

			TILOG("ioc_err_ack: T_ERROR_ACK: tlierror %x\n",
			    tea->TLI_error);

			/* get saved ioctl msg and set values */
			iocbp->ioc_count = 0;
			iocbp->ioc_error = 0;
			iocbp->ioc_rval = tea->TLI_error;
			if (iocbp->ioc_rval == TSYSERR)
				iocbp->ioc_rval |= tea->UNIX_error << 8;
			tp->tim_iocsave->b_datap->db_type = M_IOCACK;
			freemsg(mp);
			putnext(q, tp->tim_iocsave);
			tp->tim_iocsave = NULL;
			tp->tim_saved_prim = 0;
			tp->tim_flags &= ~(WAITIOCACK | TI_CAP_RECVD |
			    CAP_WANTS_INFO | WAIT_IOCINFOACK);
			break;
		}
	}
}

/*
 * Send negative ack upstream.
 * Requires: all pointers are not NULL.
 */
static void
send_iocnak(queue_t *q, struct iocblk *iocbp, mblk_t *mp, int ioc_err)
{
	ASSERT(q != NULL && iocbp != NULL && mp != NULL);
	mp->b_datap->db_type = M_IOCNAK;
	iocbp->ioc_error = ioc_err;
	if (mp->b_cont != NULL) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	}
	putnext(RD(q), mp);
}

/*
 * Send reply to a usual message or ioctl message upstream.
 * Should be called from the read side only.
 */
static void
tim_send_reply(queue_t *q, mblk_t *mp, struct tim_tim *tp, t_scalar_t prim)
{
	ASSERT(mp != NULL && q != NULL && tp != NULL);
	ASSERT(q == RD(q));

	/* Restore db_type - recover() might have changed it */
	mp->b_datap->db_type = M_PCPROTO;

	if (((tp->tim_flags & WAITIOCACK) == 0) || (tp->tim_saved_prim != prim))
		putnext(q, mp);
	else {
		ASSERT(tp->tim_iocsave != NULL);
		tim_ioctl_send_reply(q, tp->tim_iocsave, mp);
		tp->tim_iocsave = NULL;
		tp->tim_flags &= ~(WAITIOCACK | WAIT_IOCINFOACK |
		    TI_CAP_RECVD | CAP_WANTS_INFO);
	}
}

/*
 * Reply to TI_SYNC reequest without sending anything downstream.
 */
static void
tim_answer_ti_sync(queue_t *q, mblk_t *mp, struct tim_tim *tp,
    struct iocblk *iocbp, mblk_t *ackmp, uint32_t tsr_flags)
{
	struct ti_sync_ack *tsap;

	ASSERT(q != NULL && q == WR(q) && ackmp != NULL);

	tsap = (struct ti_sync_ack *)ackmp->b_rptr;
	bzero(tsap, sizeof (struct ti_sync_ack));
	ackmp->b_wptr = ackmp->b_rptr + sizeof (struct ti_sync_ack);

	if (tsr_flags == 0 ||
	    (tsr_flags & ~(TSRF_QLEN_REQ | TSRF_IS_EXP_IN_RCVBUF)) != 0) {
		/*
		 * unsupported/bad flag setting
		 * or no flag set.
		 */
		TILOG("timodwproc: unsupported/bad flag setting %x\n",
		    tsr_flags);
		freemsg(ackmp);
		send_iocnak(q, iocbp, mp, EINVAL);
	}

	if ((tsr_flags & TSRF_QLEN_REQ) != 0)
		tsap->tsa_qlen = tp->tim_backlog;

	if ((tsr_flags & TSRF_IS_EXP_IN_RCVBUF) != 0 &&
	    ti_expind_on_rdqueues(RD(q))) {
		/*
		 * Expedited data is queued on
		 * the stream read side
		 */
		tsap->tsa_flags |= TSAF_EXP_QUEUED;
	}

	tim_ioctl_send_reply(q, mp, ackmp);
	tp->tim_iocsave = NULL;
	tp->tim_flags &= ~(WAITIOCACK | WAIT_IOCINFOACK |
	    TI_CAP_RECVD | CAP_WANTS_INFO);
}

/*
 * pullup b_cont part of ioctl. Return 0 on success, EINVAl if transparent ioctl
 * and EAGAIN if can't pullup.
 */
static int
tim_pullup(queue_t *q, mblk_t *mp, struct iocblk *iocbp)
{
	ASSERT(q != NULL && mp != NULL && iocbp != NULL);

	if ((iocbp->ioc_count == TRANSPARENT) || (mp->b_cont == NULL)) {
		TILOG("timodwproc: EINVAL\n", 0);
		send_iocnak(q, iocbp, mp, EINVAL);
		return (EINVAL);
	}
	if (!pullupmsg(mp->b_cont, -1)) {
		TILOG("timodwproc: EAGAIN\n", 0);
		send_iocnak(q, iocbp, mp, EAGAIN);
		return (EAGAIN);
	}
	return (0);
}

/*
 * Send TPI message from IOCTL message, ssave original ioctl header and TPI
 * message type. Should be called from write side only.
 */
static void
tim_send_ioctl_tpi_msg(queue_t *q, mblk_t *mp, struct tim_tim *tp, int ioc_cmd)
{
	mblk_t *tmp;

	ASSERT(q != NULL && mp != NULL && tp != NULL);
	ASSERT(q == WR(q));
	ASSERT(mp->b_cont != NULL);

	tp->tim_iocsave = mp;
	tmp = mp->b_cont;

	mp->b_cont = NULL;
	tp->tim_flags |= WAITIOCACK;
	tp->tim_saved_prim = ((union T_primitives *)tmp->b_rptr)->type;

	/*
	 * For TI_GETINFO, the attached message is a T_INFO_REQ
	 * For TI_SYNC, we generate the T_INFO_REQ message above
	 * For TI_CAPABILITY the attached message is either
	 * T_CAPABILITY_REQ or T_INFO_REQ.
	 * Among TPI request messages possible,
	 *	T_INFO_REQ/T_CAPABILITY_ACK messages are a M_PCPROTO, rest
	 *	are M_PROTO
	 */
	if (ioc_cmd == TI_GETINFO || ioc_cmd == TI_SYNC ||
	    ioc_cmd == TI_CAPABILITY) {
		tmp->b_datap->db_type = M_PCPROTO;
	}
	else
		tmp->b_datap->db_type = M_PROTO;

	TILOG("timodwproc: sending down %d\n", tp->tim_saved_prim);
	putnext(q, tmp);
}

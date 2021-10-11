/*
 * "Copyright (c) 1994-1997 by Sun Microsystems, Inc. All rights reserved.
 * This product and related documentation are protected by copyright
 * and distributed under licenses restricting their use, copying,
 * distribution and decompilation.  No part of this product may be
 * reproduced in any form by any means without prior written
 * authorization by Sun and its licensors, if any."
 *
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the
 * Government is subject to restrictions as set forth in subparagraph
 * (c) (1) (ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 52.227-7013 and in similar clauses in the FAR and
 * NASA FAR Supplement.
 */

#pragma ident	"@(#)rlmod.c	1.22	99/01/28 SMI"

/*
 * This module implements the services provided by the rlogin daemon
 * after the connection is set up.  Mainly this means responding to
 * interrupts and window size changes.  It begins operation in "disabled"
 * state, and sends a T_DATA_REQ to the daemon to indicate that it is
 * in place and ready to be enabled.  The daemon can then know when all
 * data which sneaked passed rlmod (before it was pushed) has been received.
 * The daemon may process this data, or send data back to be inserted in
 * the read queue at the head with the RL_IOC_ENABLE ioctl.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/tihdr.h>
#include <sys/ptem.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/modctl.h>
#include <sys/vtrace.h>
#include <sys/rlioctl.h>
#include <sys/strlog.h>
#include <sys/termios.h>
#include <sys/termio.h>
#include <sys/byteorder.h>

extern struct streamtab rloginmodinfo;

static struct fmodsw fsw = {
	"rlmod",
	&rloginmodinfo,
	D_NEW | D_MTQPAIR | D_MP
};

/*
 * Module linkage information for the kernel.
 */

static struct modlstrmod modlstrmod = {
	&mod_strmodops,
	"rloginmod module",
	&fsw
};

static struct modlinkage modlinkage = {
	MODREV_1, &modlstrmod, NULL
};


_init()
{
	return (mod_install(&modlinkage));
}

_fini()
{
	return (mod_remove(&modlinkage));
}

_info(
	struct modinfo *modinfop
)
{
	return (mod_info(&modlinkage, modinfop));
}

struct rlmod_info; /* forward reference for function prototype */

static int	rlmodopen(queue_t *, dev_t *, int, int, cred_t *);
static int	rlmodclose(queue_t *, int, cred_t *);
static int	rlmodrput(queue_t *, mblk_t *);
static int	rlmodrsrv(queue_t *);
static int	rlmodwput(queue_t *, mblk_t *);
static int	rlmodwsrv(queue_t *);
static int	rlmodrmsg(queue_t *, mblk_t *);
static mblk_t	*make_expmblk(char);
static void 	rlsendnak(queue_t *, mblk_t *, int);
static void 	rlack(queue_t *, mblk_t *, mblk_t *, size_t);
static int 	rlwinctl(queue_t *, mblk_t *);
static mblk_t	*rlwinsetup(queue_t *, mblk_t *, unsigned char *);

static void	rlmod_timer(void *);
static void	rlmod_buffer(void *);
static void	recover(queue_t *, mblk_t *, size_t);
static void	recover1(queue_t *, size_t);
static int	tty_flow(queue_t *, struct rlmod_info *, mblk_t *);

#define	RLMOD_ID	106
#define	SIMWAIT		(1*hz)

/*
 * Stream module data structure definitions.
 * generally pushed onto tcp by rlogin daemon
 *
 */
static	struct	module_info	rloginmodiinfo = {
	RLMOD_ID,				/* module id number */
	"rlmod",				/* module name */
	0,					/* minimum packet size */
	INFPSZ,					/* maximum packet size */
	512,					/* hi-water mark */
	256					/* lo-water mark */
};

static	struct	qinit	rloginmodrinit = {
	rlmodrput,
	rlmodrsrv,
	rlmodopen,
	rlmodclose,
	nulldev,
	&rloginmodiinfo,
	NULL
};

static	struct	qinit	rloginmodwinit = {
	rlmodwput,
	rlmodwsrv,
	NULL,
	NULL,
	nulldev,
	&rloginmodiinfo,
	NULL
};

struct	streamtab	rloginmodinfo = {
	&rloginmodrinit,
	&rloginmodwinit,
	NULL,
	NULL
};

/*
 * Per-instance state struct for the rloginmod module.
 */
struct rlmod_info
{
	int		flags;
	bufcall_id_t	wbufcid;
	bufcall_id_t	rbufcid;
	timeout_id_t	wtimoutid;
	timeout_id_t	rtimoutid;
	int		rl_expdat;
	int		stopmode;
	mblk_t		*unbind_mp;
	char		startc;
	char		stopc;
	char		oobdata[1];
};

#ifdef DEBUG
int	rlmoddebug = 1;
#endif
/*
 * Flag used in flags
 */
#define	RL_DISABLED	0x1

/*ARGSUSED*/
static void
dummy_callback(void *arg)
{}

/*
 * rlmodopen - open routine gets called when the
 *	    module gets pushed onto the stream.
 */
/*ARGSUSED*/
static int
rlmodopen(queue_t *q, dev_t *devp, int oflag, int sflag, cred_t *cred)
{
	struct rlmod_info	*rmip;
	union T_primitives *tp;
	mblk_t *bp;
	int	error;

	if (sflag != MODOPEN)
		return (EINVAL);

	if (q->q_ptr != NULL) {
		/* It's already attached. */
		return (0);
	}

	/*
	 * Allocate state structure.
	 */
	rmip = kmem_zalloc(sizeof (*rmip), KM_SLEEP);

	/*
	 * Cross-link.
	 */
	q->q_ptr = rmip;
	WR(q)->q_ptr = rmip;
	rmip->rl_expdat = 0;
	rmip->stopmode = TIOCPKT_DOSTOP;
	rmip->startc = CTRL('q');
	rmip->stopc = CTRL('s');
	rmip->oobdata[0] = (char)TIOCPKT_WINDOW;
	/*
	 * noenable the q and set the flag in rlmod_info structure.
	 * The queue will be enabled and the flag will be reset by a
	 * special ioctl later.
	 */
	noenable(q);
	rmip->flags |= RL_DISABLED;

	qprocson(q);

	/*
	 * Since TCP operates in the TLI-inspired brain-dead fashion,
	 * the connection will revert to bound state if the connection
	 * is reset by the client.  We must send a T_UNBIND_REQ in
	 * that case so the port doesn't get "wedged" (preventing
	 * inetd from being able to restart the listener).  Allocate
	 * it here, so that we don't need to worry about allocb()
	 * failures later.
	 */
	while ((rmip->unbind_mp = allocb(sizeof (union T_primitives),
	    BPRI_HI)) == NULL) {
		bufcall_id_t id = qbufcall(q, sizeof (union T_primitives),
		    BPRI_HI, dummy_callback, NULL);
		if (!qwait_sig(q)) {
			qunbufcall(q, id);
			error = EINTR;
			goto fail;
		}
		qunbufcall(q, id);
	}
	rmip->unbind_mp->b_wptr = rmip->unbind_mp->b_rptr +
	    sizeof (struct T_unbind_req);
	rmip->unbind_mp->b_datap->db_type = M_PROTO;
	tp = (union T_primitives *)rmip->unbind_mp->b_rptr;
	tp->type = T_UNBIND_REQ;

	/*
	 * Send a M_PROTO msg of type T_DATA_REQ (this is unique for
	 * read queue since only write queue can get T_DATA_REQ).
	 * Readstream routine in the daemon will do a getmsg() till
	 * it receives this proto message.
	 */
	while ((bp = allocb(sizeof (union T_primitives), BPRI_HI)) == NULL) {
		bufcall_id_t id = qbufcall(q, sizeof (union T_primitives),
		    BPRI_HI, dummy_callback, NULL);
		if (!qwait_sig(q)) {
			qunbufcall(q, id);
			error = EINTR;
			goto fail;
		}
		qunbufcall(q, id);
	}
	bp->b_datap->db_type = M_PROTO;
	bp->b_wptr = bp->b_rptr + sizeof (union T_primitives);
	tp = (union T_primitives *)bp->b_rptr;
	tp->type = T_DATA_REQ;
	tp->data_req.MORE_flag = 0;

	putnext(q, bp);
	return (0);
fail:
	qprocsoff(q);
	if (rmip->unbind_mp != NULL) {
		freemsg(rmip->unbind_mp);
	}
	kmem_free(rmip, sizeof (struct rlmod_info));
	q->q_ptr = NULL;
	WR(q)->q_ptr = NULL;
	return (error);
}


/*
 * rlmodclose - This routine gets called when the module
 *	gets popped off of the stream.
 */

/*ARGSUSED*/
static int
rlmodclose(queue_t *q, int flag, cred_t *credp)
{
	struct rlmod_info   *rmip = (struct rlmod_info *)q->q_ptr;
	mblk_t  *mp;

	/*
	 * Flush any write side data  downstream
	 */
	while (mp = getq(WR(q)))
		putnext(WR(q), mp);

	qprocsoff(q);
	if (rmip->wbufcid) {
		qunbufcall(q, rmip->wbufcid);
		rmip->wbufcid = 0;
	}
	if (rmip->rbufcid) {
		qunbufcall(q, rmip->rbufcid);
		rmip->rbufcid = 0;
	}
	if (rmip->wtimoutid) {
		(void) quntimeout(q, rmip->wtimoutid);
		rmip->wtimoutid = 0;
	}
	if (rmip->rtimoutid) {
		(void) quntimeout(q, rmip->rtimoutid);
		rmip->rtimoutid = 0;
	}

	if (rmip->unbind_mp != NULL) {
		freemsg(rmip->unbind_mp);
	}

	kmem_free(q->q_ptr, sizeof (struct rlmod_info));
	q->q_ptr = WR(q)->q_ptr = NULL;
	return (0);
}

/*
 * rlmodrput - Module read queue put procedure.
 *	This is called from the module or
 *	driver downstream.
 */

static int
rlmodrput(queue_t *q, mblk_t *mp)
{
	struct rlmod_info    *rmip = (struct rlmod_info *)q->q_ptr;
	union T_primitives *tip;

	TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_RPUT_IN, "rlmodrput start: "
	    "q %p, mp %p, db_type 0%o", q, mp, mp->b_datap->db_type);


	if ((mp->b_datap->db_type < QPCTL) &&
	    ((q->q_first) || (rmip->flags & RL_DISABLED))) {
		(void) putq(q, mp);
		TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_RPUT_OUT,
		    "rlmodrput end: q %p, mp %p, %s", q, mp, "flow");
		return (0);
	}
	switch (mp->b_datap->db_type) {

	case M_PROTO:
	case M_PCPROTO:
		tip = (union T_primitives *)mp->b_rptr;
		switch (tip->type) {

		case T_ORDREL_IND:
		case T_DISCON_IND:
			/* Make into M_HANGUP and putnext */
			mp->b_datap->db_type = M_HANGUP;
			mp->b_wptr = mp->b_rptr;
			if (mp->b_cont) {
				freemsg(mp->b_cont);
				mp->b_cont = NULL;
			}
			/*
			 * If we haven't already, send T_UNBIND_REQ to prevent
			 * TCP from going into "BOUND" state and locking up the
			 * port.
			 */
			if (tip->type == T_DISCON_IND && rmip->unbind_mp !=
			    NULL) {
				putnext(q, mp);
				qreply(q, rmip->unbind_mp);
				rmip->unbind_mp = NULL;
			} else {
				putnext(q, mp);
			}
			break;

		/*
		 * We only get T_OK_ACK when we issue the unbind, and it can
		 * be ignored safely.
		 */
		case T_OK_ACK:
			ASSERT(rmip->unbind_mp == NULL);
			freemsg(mp);
			break;

		default:
#ifdef DEBUG
			if (rlmoddebug)
				debug_enter(NULL);
#else
			(void) strlog(RLMOD_ID, -1, 0, SL_ERROR,
			    "rlmodrput: got 0x%x type M_PROTO/M_PCPROTO\n",
			    tip->type);
#endif
			freemsg(mp);
		}
		break;

	case M_DATA:
		if (canputnext(q) && q->q_first == NULL) {
			(void) rlmodrmsg(q, mp);
		} else {
			(void) putq(q, mp);
		}
		break;

	case M_FLUSH:
		/*
		 * Since M_FLUSH came from TCP, we mark it bound for
		 * daemon, not tty.  This only happens when TCP expects
		 * to do a connection reset.
		 */
		mp->b_flag |= MSGMARK;
		if (*mp->b_rptr & FLUSHR)
			flushq(q, FLUSHALL);

		putnext(q, mp);
		break;

	case M_PCSIG:
	case M_ERROR:
	case M_IOCACK:
	case M_IOCNAK:
	case M_SETOPTS:
		if (mp->b_datap->db_type <= QPCTL && !canputnext(q))
			(void) putq(q, mp);
		else
			putnext(q, mp);
		break;

	default:
#ifdef DEBUG
		if (rlmoddebug)
			debug_enter(NULL);
#else
		(void) strlog(RLMOD_ID, -1, 0, SL_ERROR,
		    "rlmodrput: unexpected 0x%x msg type\n",
		    mp->b_datap->db_type);
#endif
		freemsg(mp);

	}
	TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_RPUT_OUT, "rlmodrput end: q %p, "
		"mp %p, %s", q, mp, "done");
	return (0);
}

/*
 * rlmodrsrv - module read service procedure
 */
static int
rlmodrsrv(queue_t *q)
{
	mblk_t	*mp;
	struct rlmod_info    *rmip = (struct rlmod_info *)q->q_ptr;
	union T_primitives *tip;

	TRACE_1(TR_FAC_RLOGINP, TR_RLOGINP_RSRV_IN, "rlmodrsrv start: "
	    "q %p", q);
	while ((mp = getq(q)) != NULL) {

		if (rmip->flags & RL_DISABLED) {
			(void) putbq(q, mp);
			TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_RSRV_OUT,
			    "rlmodrsrv end: q %p, mp %p, %s", q, mp,
			    "disabled");
			return (0);
		}
		switch (mp->b_datap->db_type) {
		case M_DATA:
			if (!canputnext(q)) {
				(void) putbq(q, mp);
				TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_RSRV_OUT,
				    "rlmodrsrv end: q %p, mp %p, %s",
				    q, mp, "!canputnext");
				return (0);
			}
			if (!rlmodrmsg(q, mp)) {
				TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_RSRV_OUT,
				    "rlmodrsrv end: q %p, mp %p, %s",
				    q, mp, "!rlmodrmsg");
				return (0);
			}
			break;

		case M_PROTO:
			tip = (union T_primitives *)mp->b_rptr;
			switch (tip->type) {

			case T_ORDREL_IND:
			case T_DISCON_IND:
				/* Make into M_HANGUP and putnext */
				mp->b_datap->db_type = M_HANGUP;
				mp->b_wptr = mp->b_rptr;
				if (mp->b_cont) {
					freemsg(mp->b_cont);
					mp->b_cont = NULL;
				}
				/*
				 * If we haven't already, send T_UNBIND_REQ
				 * to prevent TCP from going into "BOUND"
				 * state and locking up the port.
				 */
				if (tip->type == T_DISCON_IND &&
				    rmip->unbind_mp != NULL) {
					putnext(q, mp);
					qreply(q, rmip->unbind_mp);
					rmip->unbind_mp = NULL;
				} else {
					putnext(q, mp);
				}
				break;

			/*
			 * We only get T_OK_ACK when we issue the unbind, and
			 * it can be ignored safely.
			 */
			case T_OK_ACK:
				ASSERT(rmip->unbind_mp == NULL);
				freemsg(mp);
				break;

			default:
#ifdef DEBUG
				if (rlmoddebug)
					debug_enter(NULL);
#else
				(void) strlog(RLMOD_ID, -1, 0, SL_ERROR,
				    "rlmodrsrv: got 0x%lx type PROTO\n",
				    tip->type);
#endif
				freemsg(mp);
			}
			break;

		case M_SETOPTS:
			if (!canputnext(q)) {
				(void) putbq(q, mp);
				TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_RSRV_OUT,
				    "rlmodrsrv end: q %p, mp %p, %s",
				    q, mp, "!canputnext M_SETOPTS");
				return (0);
			}
			putnext(q, mp);
			break;

		default:
#ifdef DEBUG
			if (rlmoddebug)
				debug_enter(NULL);
#else
			(void) strlog(RLMOD_ID, -1, 0, SL_ERROR,
			    "rlmodrsrv: unexpected 0x%x msg type\n",
			    mp->b_datap->db_type);
#endif
			freemsg(mp);

		}
	}

	TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_RSRV_OUT, "rlmodrsrv end: q %p, "
	    "mp %p, %s", q, mp, "empty");

	return (0);
}

/*
 * rlmodwput - Module write queue put procedure.
 *	All non-zero messages are send downstream unchanged
 */
static int
rlmodwput(queue_t *q, mblk_t *mp)
{
	struct iocblk *ioc;
	char cntl;
	struct rlmod_info *rmip = (struct rlmod_info *)q->q_ptr;
	mblk_t *tmpmp;
	int rw;

	TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_WPUT_IN, "rlmodwput start: "
	    "q %p, mp %p, db_type 0%o", q, mp, mp->b_datap->db_type);

	if (rmip->rl_expdat) {
		/*
		 * call make_expmblk to create an expedited
		 * message block.
		 */
		cntl = rmip->oobdata[0] | TIOCPKT_FLUSHWRITE;

		if (!canputnext(q)) {
			(void) putq(q, mp);
			TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_WPUT_OUT,
			    "rlmodwput end: q %p, mp %p, %s",
			    q, mp, "expdata && !canputnext");
			return (0);
		}
		if ((tmpmp = make_expmblk(cntl))) {
			putnext(q, tmpmp);
			rmip->rl_expdat = 0;
		} else {
			recover1(q, sizeof (mblk_t)); /* XXX.sparker */
		}
	}

	if ((q->q_first || rmip->rl_expdat) && mp->b_datap->db_type < QPCTL) {
		(void) putq(q, mp);
		TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_WPUT_OUT, "rlmodwput end: "
		    "q %p, mp %p, %s", q, mp, "queued data");
		return (0);
	}
	switch (mp->b_datap->db_type) {

	case M_DATA:
		if (!canputnext(q))
			(void) putq(q, mp);
		else
			putnext(q, mp);
		break;

	case M_FLUSH:
		/*
		 * We must take care to create and forward out-of-band data
		 * indicating the flush to the far side.
		 */
		rw = *mp->b_rptr;
		*mp->b_rptr &= ~FLUSHW;
		qreply(q, mp);
		if (rw & FLUSHW) {
			/*
			 * Since all rlogin protocol data is sent in this
			 * direction as urgent data, and TCP does not flush
			 * urgent data, it is okay to actually forward this
			 * flush.  (telmod cannot.)
			 */
			flushq(q, FLUSHDATA);
			mp = allocb(1, BPRI_HI);
			/*
			 * Ideally if allocb fails we might set a state bit
			 * and reschedule ourselves when memory becomes
			 * available, so we make sure not to miss sending
			 * the FLUSHW to TCP before the urgent byte.  Not
			 * doing this just means in some cases a bit more
			 * trash passes before the flush takes hold.
			 */
			if (mp) {
				mp->b_datap->db_type = M_FLUSH;
				*mp->b_rptr = FLUSHW;
				putnext(q, mp);
			}
			/*
			 * Notify peer of the write flush request.
			 */
			cntl = rmip->oobdata[0] | TIOCPKT_FLUSHWRITE;
			if (!canputnext(q)) {
				TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_WPUT_OUT,
				    "rlmodwput end: q %p, mp %p, %s",
				    q, mp, "flushw && !canputnext");
				return (0);
			}
			if ((mp = make_expmblk(cntl)) == NULL) {
				rmip->rl_expdat = 1;
				recover1(q, sizeof (mblk_t));
				TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_WPUT_OUT,
				    "rlmodwput end: q %p, mp %p, %s",
				    q, mp, "!make_expmblk");
				return (0);
			}
			putnext(q, mp);
		}
		break;

	case M_IOCTL:
		ioc = (struct iocblk *)mp->b_rptr;
		switch (ioc->ioc_cmd) {

		/*
		 * This is a special ioctl to reenable the queue.
		 * The initial data read from the stream head is
		 * put back on the queue.
		 */
		case RL_IOC_ENABLE:
			/*
			 * Send negative ack if RL_DISABLED flag is not set
			 */

			if (!(rmip->flags & RL_DISABLED)) {
				rlsendnak(q, mp, EINVAL);
				break;
			}
			if (mp->b_cont) {
				(void) putbq(RD(q), mp->b_cont);
				mp->b_cont = 0;
			}

			if (rmip->flags & RL_DISABLED)
				rmip->flags &= ~RL_DISABLED;
			qenable(RD(q));
			enableok(RD(q));
			rlack(q, mp, NULL, 0);
			TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_WPUT_OUT,
			    "rlmodwput end: q %p, mp %p, %s",
			    q, mp, "IOCACK enable");
			return (0);

		/*
		 * If it is a tty ioctl, save the output flow
		 * control flag and the start and stop flow control
		 * characters if they are available.
		 */
		case TCSETS:
		case TCSETSW:
		case TCSETSF:
		case TCSETA:
		case TCSETAW:
		case TCSETAF:
			/*
			 * tty_flow tells us whether it queued or processed
			 * the message, but we don't care.
			 */
			if (!canputnext(q)) {
				(void) putq(q, mp);
				TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_WPUT_OUT,
				    "rlmodwput end: q %p, mp %p, %s",
				    q, mp, "!canputnext TCSET");
				return (0);
			}
			(void) tty_flow(q, rmip, mp);
			break;

		case TIOCSWINSZ:
		case TIOCSTI:
		case TCSBRK:
			freemsg(mp);
			break;

		default:
			freemsg(mp); /* XXX.sparker diagnostic */
			break;
		}
		break;

	case M_PROTO:
		switch (((union T_primitives *)mp->b_rptr)->type) {
		case T_EXDATA_REQ:
		case T_ORDREL_REQ:
		case T_DISCON_REQ:
			putnext(q, mp);
			break;

		default:
#ifdef DEBUG
			if (rlmoddebug)
				debug_enter(NULL);
#else
			(void) strlog(RLMOD_ID, -1, 0, SL_ERROR, "rlmodwput: "
			    "unexpected TPI primitive %d.\n",
			    ((union T_primitives *)mp->b_rptr)->type);
#endif
			freemsg(mp);
		}
		break;

	case M_PCPROTO:
		if (((struct T_exdata_req *)mp->b_rptr)->PRIM_type ==
		    T_DISCON_REQ) {
			putnext(q, mp);
		} else {
			/* XXX.sparker Log unexpected message */
			freemsg(mp);
		}
		break;

	default:
#ifdef DEBUG
		if (rlmoddebug)
			debug_enter(NULL);
#else
		(void) strlog(RLMOD_ID, -1, 0, SL_ERROR,
		    "rlmodwput: unexpected 0x%x msg type\n",
		    mp->b_datap->db_type);
#endif
		freemsg(mp);
		break;
	}
	TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_WPUT_OUT, "rlmodwput end: "
	    "q %p, mp %p, %s", q, mp, "done");
	return (0);
}

/*
 * rlmodwsrv - module write service procedure
 */
static int
rlmodwsrv(queue_t *q)
{
	mblk_t	*mp, *tmpmp;
	char cntl;
	struct rlmod_info *rmip = (struct rlmod_info *)q->q_ptr;

	TRACE_1(TR_FAC_RLOGINP, TR_RLOGINP_WSRV_IN, "rlmodwsrv "
	    "start: q %X", q);
	if (rmip->rl_expdat) {
		/*
		 * call make_expmblk to create an expedited
		 * message block.
		 */
		cntl = rmip->oobdata[0] | TIOCPKT_FLUSHWRITE;
		if (!canputnext(q)) {
			TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_WSRV_OUT,
			    "rlmodwsrv end: q %p, mp %p, %s",
			    q, NULL, "!canputnext && expdat");
			return (0);
		}
		if ((tmpmp = make_expmblk(cntl))) {
			putnext(q, tmpmp);
			rmip->rl_expdat = 0;
		} else {
			recover1(q, sizeof (mblk_t));
			TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_WSRV_OUT,
			    "rlmodwsrv end: q %p, mp %p, %s",
			    q, NULL, "!make_expmblk");
			return (0);
		}
	}
	while ((mp = getq(q)) != NULL) {

		if (!canputnext(q) || rmip->rl_expdat) {
			(void) putbq(q, mp);
			TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_WSRV_OUT,
			    "rlmodwsrv end: q %p, mp %p, %s",
			    q, mp, "!canputnext || expdat");
			return (0);
		}
		if (mp->b_datap->db_type == M_IOCTL) {
			if (!tty_flow(q, rmip, mp)) {
				TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_WSRV_OUT,
				    "rlmodwsrv end: q %p, mp %p, %s",
				    q, mp, "!tty_flow");
				return (0);
			}
			continue;
		}
		putnext(q, mp);
	}
	TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_WSRV_OUT, "rlmodwsrv end: q %p, "
	    "mp %p, %s", q, mp, "done");
	return (0);
}

/*
 * Send a negative acknowledgement for the ioctl denoted by mp through the
 * queue q, specifying the error code err.
 *
 * This routine could be a macro or in-lined, except that space is more
 * critical than time in error cases.
 */
static void
rlsendnak(queue_t *q, mblk_t *mp, int err)
{
	struct iocblk  *iocp = (struct iocblk *)mp->b_rptr;

	mp->b_datap->db_type = M_IOCNAK;
	iocp->ioc_count = 0;
	iocp->ioc_error = err;
	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	}
	qreply(q, mp);
}

/*
 * Convert the M_IOCTL or M_IOCDATA mesage denoted by mp into an M_IOCACK.
 * Free any data associated with the message and replace it with dp if dp is
 * non-NULL, adjusting dp's write pointer to match size.
 */
static void
rlack(queue_t *q, mblk_t *mp, mblk_t *dp, size_t size)
{
	struct iocblk  *iocp = (struct iocblk *)mp->b_rptr;

	mp->b_datap->db_type = M_IOCACK;
	iocp->ioc_count = size;
	iocp->ioc_error = 0;
	iocp->ioc_rval = 0;
	if (mp->b_cont != NULL)
		freemsg(mp->b_cont);
	if (dp != NULL) {
		mp->b_cont = dp;
		dp->b_wptr += size;
	} else {
		mp->b_cont = NULL;
	}
	qreply(q, mp);
}

/*
 * This routine returns a message block with an expedited
 * data request
 */
static mblk_t *
make_expmblk(char cntl)
{
	mblk_t *mp;
	mblk_t *bp;
	struct T_exdata_req	*data_req;

	bp = allocb(sizeof (struct T_exdata_req), BPRI_MED);
	if (bp == NULL)
		return (NULL);
	if ((mp = allocb(sizeof (char), BPRI_MED)) == NULL) {
		freeb(bp);
		return (NULL);
	}
	bp->b_datap->db_type = M_PROTO;
	data_req = (struct T_exdata_req *)bp->b_rptr;
	data_req->PRIM_type = T_EXDATA_REQ;
	data_req->MORE_flag = 0;

	bp->b_wptr += sizeof (struct T_exdata_req);
	/*
	 * Send a 1 byte data message block with appropriate
	 * control character.
	 */
	mp->b_datap->db_type = M_DATA;
	mp->b_wptr = mp->b_rptr + 1;
	(*(char *)(mp->b_rptr)) = cntl;
	bp->b_cont = mp;
	return (bp);
}
/*
 * This routine parses M_DATA messages checking for window size protocol
 * from a given message block.  It returns TRUE if no resource exhaustion
 * conditions are found.  This is for use in the service procedure, which
 * needs to know whether to continue, or stop processing the queue.
 */
static int
rlmodrmsg(queue_t *q, mblk_t *mp)
{
	unsigned char *tmp, *tmp1;
	mblk_t	*newmp;
	size_t	sz;
	ssize_t	count, newcount = 0;

	newmp = mp;
	/*
	 * Eliminate any zero length messages here, so we don't filter EOFs
	 * accidentally.
	 */
	if (msgdsize(mp) == 0) {
		goto out;
	}
	while (mp) {
		tmp = mp->b_rptr;
		/*
		 * scan through the entire message block
		 */
		while (tmp < mp->b_wptr) {
			/*
			 * check for FF (rlogin magic escape sequence)
			 */
			if (tmp[0] == RLOGIN_MAGIC) {
				/*
				 * Check to see if the entire protocol
				 * fits in this block, else do pullupmsg.
				 * This is a historic bug in BSD and its
				 * derivatives.  There is no guarantee this
				 * chunk will arrive in a single TCP segment
				 * so we really need to drive a state
				 * machine to process it correctly when
				 * split across segments. XXX.sparker
				 */
				count = newcount + tmp - mp->b_rptr;
				if ((pullupmsg(newmp, -1)) == NULL) {
					sz = msgdsize(newmp);
					recover(q, newmp, sz);
					return (NULL);
				}

				/*
				 * pullupmsg gives new newmp
				 */
				mp = newmp;

				/*
				 * adjust tmp to where we
				 * stopped - count keeps track
				 * of bytes read so far.
				 * reset newcount = 0.
				 */
				tmp = mp->b_rptr + count;
				newcount = 0;

				/*
				 * Use the variable tmp1 to compute where
				 * the end of the window escape (currently
				 * the only rlogin protocol sequence), then
				 * check to see if we got all those bytes.
				 */
				tmp1 = tmp + 4 + sizeof (struct winsize);

				if (tmp1 > mp->b_wptr) {
					/*
					 * protocol error - keep
					 * scanning for next block.
					 */
					(void) strlog(RLMOD_ID, -1, 0, SL_ERROR,
					    "rlmodrmsg: window size split "
					    "across multiple segments\n");
					break;
				}
				/*
				 * check for FF FF s s pattern
				 */
				if ((tmp[1] == RLOGIN_MAGIC) &&
				    (tmp[2] == 's') && (tmp[3] == 's')) {

					/*
					 * If rlwinsetup returns an error,
					 * we do recover with newmp which
					 * points to new chain of mblks after
					 * doing window control ioctls.
					 * rlwinsetup returns newmp which
					 * contains only data part.
					 * Note that buried inside rlwinsetup
					 * is where we do the putnext.
					 */
					if (rlwinsetup(q, mp, tmp) == NULL) {
						sz = msgdsize(mp);
						recover(q, mp, sz);
						return (NULL);
					}
					/*
					 * We have successfully consumed the
					 * window sequence, but rlwinsetup()
					 * and its children have moved memory
					 * up underneath us.  This means that
					 * the byte underneath *tmp has not
					 * been scanned now.  We will now need
					 * to rescan it.
					 */
					continue;
				}
			}
			tmp++;
		}
		/*
		 * bump newcount to include size of this particular block.
		 */
		newcount += (mp->b_wptr - mp->b_rptr);
		mp = mp->b_cont;
	}
	/*
	 * If we trimmed the message down to nothing to forward, don't
	 * send any M_DATA message.  (Don't want to send EOF!)
	 */
	if (msgdsize(newmp) == 0) {
		freemsg(newmp);
		newmp = NULL;
	}
out:
	if (newmp) {
		if (!canputnext(q)) {
			(void) putbq(q, newmp);
			return (NULL);
		} else {
			putnext(q, newmp);
		}
	}
	return (TRUE);
}


/*
 * This routine is called to handle window size changes.
 * The routine returns 1 on success and 0 on error (allocb failure).
 */
static int
rlwinctl(queue_t *q, mblk_t *mp)
{
	mblk_t	*rl_msgp;
	struct	iocblk	*iocbp;
	struct	rlmod_info	*rmip = (struct rlmod_info *)q->q_ptr;

	TRACE_3(TR_FAC_RLOGINP, TR_RLOGINP_WINCTL_IN, "rlwinctl start: q %p, "
	    "mp %p, db_type 0%o", q, mp, mp->b_datap->db_type);

	rmip->oobdata[0] &= ~TIOCPKT_WINDOW; /* we know he heard */

	if ((rl_msgp = mkiocb(TIOCSWINSZ)) == NULL) {
		TRACE_2(TR_FAC_RLOGINP, TR_RLOGINP_WINCTL_OUT, "rlwinctl end: "
		    "q %p, mp %p, allocb failed", q, mp);
		return (0);
	}

	/*
	 * create an M_IOCTL message type.
	 */
	rl_msgp->b_cont = mp;
	iocbp = (struct iocblk *)rl_msgp->b_rptr;
	iocbp->ioc_count = msgdsize(mp);

	putnext(q, rl_msgp);
	TRACE_2(TR_FAC_RLOGINP, TR_RLOGINP_WINCTL_OUT, "rlwinctl end: "
	    "q %p, mp %p, done", q, mp);
	return (1);
}

/*
 * This routine sets up window size change protocol.
 * The routine returns the new mblk after issuing rlwinctl
 * for window size changes. New mblk contains only data part
 * of the message block. The routine returns 0 on error.
 */
static mblk_t *
rlwinsetup(queue_t *q, mblk_t *mp, unsigned char *blk)
{
	mblk_t		*mp1;
	unsigned char	*jmpmp;
	ssize_t		left = 0;
	struct winsize	win;

	/*
	 * Set jmpmp to where to jump, to get just past the end of the
	 * window size protocol sequence.
	 */
	jmpmp = (blk + 4 + sizeof (struct winsize));
	left = mp->b_wptr - jmpmp;

	if ((mp1 = allocb(sizeof (struct winsize), BPRI_MED)) == NULL)
		return (0);
	mp1->b_datap->db_type = M_DATA;
	mp1->b_wptr = mp1->b_rptr + sizeof (struct winsize);
	bcopy(blk + 4, &win, sizeof (struct winsize));
	win.ws_row = ntohs(win.ws_row);
	win.ws_col = ntohs(win.ws_col);
	win.ws_xpixel = ntohs(win.ws_xpixel);
	win.ws_ypixel = ntohs(win.ws_ypixel);
	bcopy(&win, mp1->b_rptr, sizeof (struct winsize));

	if ((rlwinctl(q, mp1)) == NULL) {
		freeb(mp1);
		return (0);
	}
	if (left > 0) {
		/*
		 * Must delete the window size protocol sequence.  We do
		 * this by sliding all the stuff after the sequence (jmpmp)
		 * to where the sequence itself began (blk).
		 */
		bcopy(jmpmp, blk, left);
		mp->b_wptr = blk + left;
	} else
		mp->b_wptr = blk;
	return (mp);
}

/*
 * When an ioctl changes software flow control on the tty, we must notify
 * the rlogin client, so it can adjust its behavior appropriately.  This
 * routine, called from either the put or service routine, determines if
 * the flow handling has changed.  If so, it tries to send the indication
 * to the client.  It returns true or false depending upon whether the
 * message was fully processed.  If it wasn't fully processed it queues
 * the message for retry later when resources
 * (allocb/canputnext) are available.
 */
static int
tty_flow(queue_t *q, struct rlmod_info *rmip, mblk_t *mp)
{
	struct iocblk *ioc;
	struct termios *tp;
	struct termio *ti;
	int stop, ixon;
	mblk_t *tmpmp;
	char cntl;

	ioc = (struct iocblk *)mp->b_rptr;
	switch (ioc->ioc_cmd) {

	/*
	 * If it is a tty ioctl, save the output flow
	 * control flag and the start and stop flow control
	 * characters if they are available.
	 */
	case TCSETS:
	case TCSETSW:
	case TCSETSF:
		if (msgdsize(mp->b_cont) < sizeof (struct termios)) {
			rlsendnak(q, mp, EINVAL);
			return (1);
		}
		tp = (struct termios *)(mp->b_cont->b_rptr);
		rmip->stopc = tp->c_cc[VSTOP];
		rmip->startc = tp->c_cc[VSTART];
		ixon = tp->c_iflag & IXON;
		break;

	case TCSETA:
	case TCSETAW:
	case TCSETAF:
		if (msgdsize(mp->b_cont) < sizeof (struct termio)) {
			rlsendnak(q, mp, EINVAL);
			return (1);
		}
		ti = (struct termio *)(mp->b_cont->b_rptr);
		ixon = ti->c_iflag & IXON;
		break;

	case TCSBRK:
	case TIOCSWINSZ:
	case TIOCSTI:
		freemsg(mp);
		return (1);

	default:
		/*
		 * An M_IOCTL is only queued if it si one of the above
		 * listed ones, so this should never happen.
		 */
		ASSERT(0);
		(void) strlog(RLMOD_ID, -1, 0, SL_ERROR,
		    "rloginmod: tty_flow: bad ioctl %x\n", ioc->ioc_cmd);
		break;
	}
	/*
	 * If tty ioctl processing is done, check for stopmode
	 */
	stop = (ixon && (rmip->stopc == CTRL('s')) &&
		(rmip->startc == CTRL('q')));
	if (rmip->stopmode == TIOCPKT_NOSTOP) {
		if (stop) {
			cntl = rmip->oobdata[0] | TIOCPKT_DOSTOP;
			if ((tmpmp = make_expmblk(cntl)) == NULL) {
				recover(q, mp, sizeof (mblk_t));
				return (0);
			}
			if (!canputnext(q))
				(void) putbq(q, tmpmp);
			else
				putnext(q, tmpmp);
			rmip->stopmode = TIOCPKT_DOSTOP;
		}
	} else {
		if (!stop) {
			cntl = rmip->oobdata[0] | TIOCPKT_NOSTOP;
			if ((tmpmp = make_expmblk(cntl))
				== NULL) {
				recover(q, mp, sizeof (mblk_t));
				return (0);
			}
			if (!canputnext(q))
				(void) putbq(q, tmpmp);
			else
				putnext(q, tmpmp);
			rmip->stopmode = TIOCPKT_NOSTOP;
		}
	}
	freemsg(mp);
	return (1);
}

static void
rlmod_timer(void *arg)
{
	queue_t *q = arg;
	struct rlmod_info	*rmip = (struct rlmod_info *)q->q_ptr;

	ASSERT(rmip);
	if (q->q_flag & QREADR) {
		ASSERT(rmip->rtimoutid);
		rmip->rtimoutid = 0;
	} else {
		ASSERT(rmip->wtimoutid);
		rmip->wtimoutid = 0;
	}
	enableok(q);
	qenable(q);
}

static void
rlmod_buffer(void *arg)
{
	queue_t *q = arg;
	struct rlmod_info	*rmip = (struct rlmod_info *)q->q_ptr;

	ASSERT(rmip);
	if (q->q_flag & QREADR) {
		ASSERT(rmip->rbufcid);
		rmip->rbufcid = 0;
	} else {
		ASSERT(rmip->wbufcid);
		rmip->wbufcid = 0;
	}
	enableok(q);
	qenable(q);
}

static void
recover(queue_t *q, mblk_t *mp, size_t size)
{
	/*
	 * Avoid re-enabling the queue.
	 */
	ASSERT(mp->b_datap->db_type < QPCTL);

	noenable(q);
	(void) putbq(q, mp);
	recover1(q, size);
}

static void
recover1(queue_t *q, size_t size)
{
	struct rlmod_info	*rmip = (struct rlmod_info *)q->q_ptr;
	timeout_id_t	tid;
	bufcall_id_t	bid;

	/*
	 * Make sure there is at most one outstanding request per queue.
	 */
	if (q->q_flag & QREADR) {
		if (rmip->rtimoutid || rmip->rbufcid)
			return;
	} else {
		if (rmip->wtimoutid || rmip->wbufcid)
			return;
	}
	if (!(bid = qbufcall(RD(q), size, BPRI_MED, rlmod_buffer, q))) {
		tid = qtimeout(RD(q), rlmod_timer, q, SIMWAIT);
		if (q->q_flag & QREADR)
			rmip->rtimoutid = tid;
		else
			rmip->wtimoutid = tid;
	} else	{
		if (q->q_flag & QREADR)
			rmip->rbufcid = bid;
		else
			rmip->wbufcid = bid;
	}
}

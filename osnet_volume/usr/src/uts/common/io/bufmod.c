/*
 * Copyright (c) 1991,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bufmod.c	1.28	99/01/10 SMI"

/*
 * STREAMS Buffering module
 *
 * This streams module collects incoming messages from modules below
 * it on the stream and buffers them up into a smaller number of
 * aggregated messages.  Its main purpose is to reduce overhead by
 * cutting down on the number of read (or getmsg) calls its client
 * user process makes.
 *  - only M_DATA is buffered.
 *  - multithreading assumes configured as D_MTQPAIR
 *  - packets are lost only if flag SB_NO_HEADER is clear and buffer
 *    allocation fails.
 *  - in order message transmission. This is enforced for messages other
 *    than high priority messages.
 *  - zero length messages on the read side are not passed up the
 *    stream but used internally for synchronization.
 * FLAGS:
 * - SB_NO_PROTO_CVT - no conversion of M_PROTO messages to M_DATA.
 *   (conversion is the default for backwards compatibility
 *    hence the negative logic).
 * - SB_NO_HEADER - no headers in buffered data.
 *   (adding headers is the default for backwards compatibility
 *    hence the negative logic).
 * - SB_DEFER_CHUNK - provides improved response time in question-answer
 *   applications. Buffering is not enabled until the second message
 *   is received on the read side within the sb_ticks interval.
 *   This option will often be used in combination with flag SB_SEND_ON_WRITE.
 * - SB_SEND_ON_WRITE - a write message results in any pending buffered read
 *   data being immediately sent upstream.
 * - SB_NO_DROPS - bufmod behaves transparently in flow control and propagates
 *   the blocked flow condition downstream. If this flag is clear (default)
 *   messages will be dropped if the upstream flow is blocked.
 */


#include	<sys/types.h>
#include	<sys/errno.h>
#include	<sys/debug.h>
#include	<sys/stropts.h>
#include	<sys/time.h>
#include	<sys/stream.h>
#include	<sys/conf.h>
#include	<sys/ddi.h>
#include	<sys/sunddi.h>
#include	<sys/kmem.h>
#include	<sys/strsun.h>
#include	<sys/bufmod.h>
#include	<sys/modctl.h>

/*
 * Per-Stream state information.
 *
 * If sb_ticks is negative, we don't deliver chunks until they're
 * full.  If it's zero, we deliver every packet as it arrives.  (In
 * this case we force sb_chunk to zero, to make the implementation
 * easier.)  Otherwise, sb_ticks gives the number of ticks in a
 * buffering interval. The interval begins when the a read side data
 * message is received and a timeout is not active. If sb_snap is
 * zero, no truncation of the msg is done.
 */
struct sb {
	queue_t	*sb_rq;		/* our rq */
	mblk_t	*sb_mp;		/* partial chunk */
	mblk_t	*sb_tail;	/* first mblk of last message appended */
	u_int	sb_mlen;	/* sb_mp length */
	u_int	sb_mcount;	/* input msg count in sb_mp */
	u_int	sb_chunk;	/* max chunk size */
	clock_t	sb_ticks;	/* timeout interval */
	timeout_id_t sb_timeoutid; /* qtimeout() id */
	u_int	sb_drops;	/* cumulative # discarded msgs */
	u_int	sb_snap;	/* snapshot length */
	u_int	sb_flags;	/* flags field */
	u_int	sb_state;	/* state variable */
};

/*
 * Function prototypes.
 */
static	int	sbopen(queue_t *, dev_t *, int, int, cred_t *);
static	int	sbclose(queue_t *, int, cred_t *);
static	void	sbwput(queue_t *, mblk_t *);
static	void	sbrput(queue_t *, mblk_t *);
static	void	sbrsrv(queue_t *);
static	void	sbioctl(queue_t *, mblk_t *);
static	mblk_t	*sbaugmsg(mblk_t *);
static	void	sbaddmsg(queue_t *, mblk_t *);
static	void	sbtick(void *);
static	void	sbclosechunk(struct sb *);
static	void	sbsendit(queue_t *, mblk_t *);

static struct module_info	sb_minfo = {
	21,		/* mi_idnum */
	"bufmod",	/* mi_idname */
	0,		/* mi_minpsz */
	INFPSZ,		/* mi_maxpsz */
	1,		/* mi_hiwat */
	0		/* mi_lowat */
};

static struct qinit	sb_rinit = {
	(int (*)())sbrput,	/* qi_putp */
	(int (*)())sbrsrv,	/* qi_srvp */
	sbopen,			/* qi_qopen */
	sbclose,		/* qi_qclose */
	NULL,			/* qi_qadmin */
	&sb_minfo,		/* qi_minfo */
	NULL			/* qi_mstat */
};

static struct qinit	sb_winit = {
	(int (*)())sbwput,	/* qi_putp */
	NULL,			/* qi_srvp */
	NULL,			/* qi_qopen */
	NULL,			/* qi_qclose */
	NULL,			/* qi_qadmin */
	&sb_minfo,		/* qi_minfo */
	NULL			/* qi_mstat */
};

static struct streamtab	sb_info = {
	&sb_rinit,	/* st_rdinit */
	&sb_winit,	/* st_wrinit */
	NULL,		/* st_muxrinit */
	NULL		/* st_muxwinit */
};


/*
 * This is the loadable module wrapper.
 */

static struct fmodsw fsw = {
	"bufmod",
	&sb_info,
	D_NEW | D_MTQPAIR | D_MP
};

/*
 * Module linkage information for the kernel.
 */

static struct modlstrmod modlstrmod = {
	&mod_strmodops, "streams buffer mod", &fsw
};

static struct modlinkage modlinkage = {
	MODREV_1, &modlstrmod, NULL
};


int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}


/* ARGSUSED */
static int
sbopen(queue_t *rq, dev_t *dev, int oflag, int sflag, cred_t *crp)
{
	struct sb	*sbp;

	ASSERT(rq);

	if (sflag != MODOPEN)
		return (EINVAL);

	if (rq->q_ptr)
		return (0);

	/*
	 * Allocate and initialize per-Stream structure.
	 */
	sbp = kmem_alloc(sizeof (struct sb), KM_SLEEP);
	sbp->sb_rq = rq;
	sbp->sb_ticks = -1;
	sbp->sb_chunk = SB_DFLT_CHUNK;
	sbp->sb_tail = sbp->sb_mp = NULL;
	sbp->sb_mlen = 0;
	sbp->sb_mcount = 0;
	sbp->sb_timeoutid = 0;
	sbp->sb_drops = 0;
	sbp->sb_snap = 0;
	sbp->sb_flags = 0;
	sbp->sb_state = 0;

	rq->q_ptr = WR(rq)->q_ptr = sbp;

	qprocson(rq);


	return (0);
}

/* ARGSUSED1 */
static int
sbclose(queue_t *rq, int flag, cred_t *credp)
{
	struct	sb	*sbp = (struct sb *)rq->q_ptr;

	ASSERT(sbp);

	qprocsoff(rq);
	/*
	 * Cancel an outstanding timeout
	 */
	if (sbp->sb_timeoutid != 0) {
		(void) quntimeout(rq, sbp->sb_timeoutid);
		sbp->sb_timeoutid = 0;
	}
	/*
	 * Free the current chunk.
	 */
	if (sbp->sb_mp) {
		freemsg(sbp->sb_mp);
		sbp->sb_tail = sbp->sb_mp = NULL;
		sbp->sb_mlen = 0;
	}

	/*
	 * Free the per-Stream structure.
	 */
	kmem_free((caddr_t)sbp, sizeof (struct sb));
	rq->q_ptr = WR(rq)->q_ptr = NULL;

	return (0);
}

/*
 * the correction factor is introduced to compensate for
 * whatever assumptions the modules below have made about
 * how much traffic is flowing through the stream and the fact
 * that bufmod may be snipping messages with the sb_snap length.
 */
#define	SNIT_HIWAT(msgsize, fudge)	((4 * msgsize * fudge) + 512)
#define	SNIT_LOWAT(msgsize, fudge)	((2 * msgsize * fudge) + 256)


static void
sbioc(queue_t *wq, mblk_t *mp)
{
	struct iocblk *iocp;
	struct sb *sbp = (struct sb *)wq->q_ptr;
	clock_t	ticks;
	mblk_t	*mop;

	iocp = (struct iocblk *)mp->b_rptr;

	switch (iocp->ioc_cmd) {
	case SBIOCGCHUNK:
	case SBIOCGSNAP:
	case SBIOCGFLAGS:
	case SBIOCGTIME:
		mp->b_datap->db_type = M_IOCACK;
		iocp->ioc_rval = 0;
		miocack(wq, mp, 0, 0);
		return;

	case SBIOCSTIME:
#ifdef _SYSCALL32_IMPL
		if ((iocp->ioc_flag & IOC_MODELS) != IOC_NATIVE) {
			struct timeval32 *t32;

			t32 = (struct timeval32 *)mp->b_cont->b_rptr;
			if (t32->tv_sec < 0 || t32->tv_usec < 0) {
				miocnak(wq, mp, 0, EINVAL);
				break;
			}
			ticks = TIMEVAL_TO_TICK(t32);
		} else
#endif /* _SYSCALL32_IMPL */
		{
			struct timeval *tb;

			tb = (struct timeval *)mp->b_cont->b_rptr;

			if (tb->tv_sec < 0 || tb->tv_usec < 0) {
				miocnak(wq, mp, 0, EINVAL);
				break;
			}
			ticks = TIMEVAL_TO_TICK(tb);
		}
		sbp->sb_ticks = ticks;
		if (ticks == 0)
			sbp->sb_chunk = 0;
		iocp->ioc_rval = 0;
		miocack(wq, mp, 0, 0);
		sbclosechunk(sbp);
		return;

	case SBIOCSCHUNK:
		/*
		 * set up hi/lo water marks on stream head read queue.
		 * unlikely to run out of resources. Fix at later date.
		 */
		if (mop = allocb(sizeof (struct stroptions),
		    BPRI_MED)) {
			struct stroptions *sop;
			u_int chunk;

			chunk = *(u_int *)mp->b_cont->b_rptr;
			mop->b_datap->db_type = M_SETOPTS;
			mop->b_wptr += sizeof (struct stroptions);
			sop = (struct stroptions *)mop->b_rptr;
			sop->so_flags = SO_HIWAT | SO_LOWAT;
			sop->so_hiwat = SNIT_HIWAT(chunk, 1);
			sop->so_lowat = SNIT_LOWAT(chunk, 1);
			sop->so_hiwat = (sop->so_hiwat > (SB_DFLT_CHUNK * 8)) ?
			    (SB_DFLT_CHUNK * 8) : sop->so_hiwat;
			sop->so_lowat = (sop->so_lowat > (SB_DFLT_CHUNK * 4)) ?
			    (SB_DFLT_CHUNK * 4) : sop->so_lowat;
			qreply(wq, mop);
		}

		sbp->sb_chunk = *(u_int *)mp->b_cont->b_rptr;
		iocp->ioc_rval = 0;
		miocack(wq, mp, 0, 0);
		sbclosechunk(sbp);
		return;

	case SBIOCSFLAGS:
		sbp->sb_flags = *(u_int *)mp->b_cont->b_rptr;
		iocp->ioc_rval = 0;
		miocack(wq, mp, 0, 0);
		return;

	case SBIOCSSNAP:
		/*
		 * if chunking dont worry about effects of
		 * snipping of message size on head flow control
		 * since it has a relatively small bearing on the
		 * data rate onto the streamn head.
		 */
		if (!sbp->sb_chunk) {
			/*
			 * set up hi/lo water marks on stream head read queue.
			 * unlikely to run out of resources. Fix at later date.
			 */
			if (mop = allocb(sizeof (struct stroptions),
			    BPRI_MED)) {
				struct stroptions *sop;
				u_int snap;
				int fudge;

				snap = *(u_int *)mp->b_cont->b_rptr;
				mop->b_datap->db_type = M_SETOPTS;
				mop->b_wptr += sizeof (struct stroptions);
				sop = (struct stroptions *)mop->b_rptr;
				sop->so_flags = SO_HIWAT | SO_LOWAT;
				fudge = snap <= 100 ?   4 :
				    snap <= 400 ?   2 :
				    1;
				sop->so_hiwat = SNIT_HIWAT(snap, fudge);
				sop->so_lowat = SNIT_LOWAT(snap, fudge);
				sop->so_hiwat =
				    (sop->so_hiwat > (SB_DFLT_CHUNK * 8)) ?
				    (SB_DFLT_CHUNK * 8) : sop->so_hiwat;
				sop->so_lowat =
				    (sop->so_lowat > (SB_DFLT_CHUNK * 4)) ?
				    (SB_DFLT_CHUNK * 4) : sop->so_lowat;
				qreply(wq, mop);
			}
		}

		sbp->sb_snap = *(u_int *)mp->b_cont->b_rptr;
		iocp->ioc_rval = 0;
		miocack(wq, mp, 0, 0);
		return;

	default:
		ASSERT(0);
		return;
	}
}

/*
 * Write-side put procedure.  Its main task is to detect ioctls
 * for manipulating the buffering state and hand them to sbioctl.
 * Other message types are passed on through.
 */
static void
sbwput(queue_t *wq, mblk_t *mp)
{
	struct	sb	*sbp = (struct sb *)wq->q_ptr;
	struct copyresp *resp;

	if (sbp->sb_flags & SB_SEND_ON_WRITE)
		sbclosechunk(sbp);
	switch (mp->b_datap->db_type) {
	case M_IOCTL:
		sbioctl(wq, mp);
		break;

	case M_IOCDATA:
		resp = (struct copyresp *)mp->b_rptr;
		if (resp->cp_rval) {
			/*
			 * Just free message on failure.
			 */
			freemsg(mp);
			break;
		}

		switch (resp->cp_cmd) {
		case SBIOCSTIME:
		case SBIOCSCHUNK:
		case SBIOCSFLAGS:
		case SBIOCSSNAP:
		case SBIOCGTIME:
		case SBIOCGCHUNK:
		case SBIOCGSNAP:
		case SBIOCGFLAGS:
			sbioc(wq, mp);
			break;

		default:
			putnext(wq, mp);
			break;
		}
		break;

	default:
		putnext(wq, mp);
		break;
	}
}

/*
 * Read-side put procedure.  It's responsible for buffering up incoming
 * messages and grouping them into aggregates according to the current
 * buffering parameters.
 */
static void
sbrput(queue_t *rq, mblk_t *mp)
{
	struct	sb	*sbp = (struct sb *)rq->q_ptr;

	ASSERT(sbp);

	switch (mp->b_datap->db_type) {
	case M_PROTO:
		if (sbp->sb_flags & SB_NO_PROTO_CVT) {
			sbclosechunk(sbp);
			sbsendit(rq, mp);
			break;
		} else {
			/*
			 * Convert M_PROTO to M_DATA.
			 */
			mp->b_datap->db_type = M_DATA;
		}
		/* FALLTHRU */

	case M_DATA:
		if ((sbp->sb_flags & SB_DEFER_CHUNK) &&
		    !(sbp->sb_state & SB_FRCVD)) {
			sbclosechunk(sbp);
			sbsendit(rq, mp);
			sbp->sb_state |= SB_FRCVD;
		} else
			sbaddmsg(rq, mp);

		if ((sbp->sb_ticks > 0) && !(sbp->sb_timeoutid))
			sbp->sb_timeoutid = qtimeout(sbp->sb_rq, sbtick,
			    sbp, sbp->sb_ticks);

		break;

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			/*
			 * Reset timeout, flush the chunk currently in
			 * progress, and start a new chunk.
			 */
			if (sbp->sb_timeoutid) {
				(void) quntimeout(sbp->sb_rq,
				    sbp->sb_timeoutid);
				sbp->sb_timeoutid = 0;
			}
			if (sbp->sb_mp) {
				freemsg(sbp->sb_mp);
				sbp->sb_tail = sbp->sb_mp = NULL;
				sbp->sb_mlen = 0;
				sbp->sb_mcount = 0;
			}
			flushq(rq, FLUSHALL);
		}
		putnext(rq, mp);
		break;

	case M_CTL:
		/*
		 * Zero-length M_CTL means our timeout() popped.
		 */
		if (MBLKL(mp) == 0) {
			freemsg(mp);
			sbclosechunk(sbp);
		} else {
			sbclosechunk(sbp);
			sbsendit(rq, mp);
		}
		break;

	default:
		if (mp->b_datap->db_type <= QPCTL) {
			sbclosechunk(sbp);
			sbsendit(rq, mp);
		} else {
			/* Note: out of band */
			putnext(rq, mp);
		}
		break;
	}
}

/*
 *  read service procedure.
 */
/* ARGSUSED */
static void
sbrsrv(queue_t *rq)
{
	mblk_t	*mp;

	/*
	 * High priority messages shouldn't get here but if
	 * one does, jam it through to avoid infinite loop.
	 */
	while ((mp = getq(rq)) != NULL) {
		if (!canputnext(rq) && (mp->b_datap->db_type <= QPCTL)) {
			/* should only get here if SB_NO_SROPS */
			(void) putbq(rq, mp);
			return;
		}
		putnext(rq, mp);
	}
}


/*
 * Convert mp to an M_COPYIN or M_COPYOUT message (as specified by type)
 * requesting size bytes.  Assumes mp denotes a TRANSPARENT M_IOCTL or
 * M_IOCDATA message.  If dp is non-NULL, it is assumed to point to data to be
 * copied out and is linked onto mp.
 */
static void
sbcopy(mblk_t *mp, mblk_t *dp, size_t size, unsigned char type)
{
	struct copyreq *cp = (struct copyreq *)mp->b_rptr;

	cp->cq_private = (mblk_t *)1;
	cp->cq_flag = 0;
	cp->cq_size = size;
	cp->cq_addr = (caddr_t)(*(intptr_t *)(mp->b_cont->b_rptr));
	if (mp->b_cont != NULL)
		freeb(mp->b_cont);
	if (dp != NULL) {
		mp->b_cont = dp;
		dp->b_wptr += size;
	} else
		mp->b_cont = NULL;
	mp->b_datap->db_type = type;
	mp->b_wptr = mp->b_rptr + sizeof (*cp);
}


/*
 * Handle write-side M_IOCTL messages.
 */
static void
sbioctl(queue_t *wq, mblk_t *mp)
{
	struct	sb	*sbp = (struct sb *)wq->q_ptr;
	struct iocblk	*iocp = (struct iocblk *)mp->b_rptr;
	struct	timeval	*t;
	clock_t	ticks;
	mblk_t	*mop;
	int	transparent = iocp->ioc_count;
	mblk_t	*tmp;

	switch (iocp->ioc_cmd) {
	case SBIOCSTIME:
		if (iocp->ioc_count == TRANSPARENT) {
#ifdef _SYSCALL32_IMPL
			if ((iocp->ioc_flag & IOC_MODELS) != IOC_NATIVE) {
				sbcopy(mp, NULL, sizeof (struct timeval32),
				    M_COPYIN);
			} else
#endif /* _SYSCALL32_IMPL */
			{
				sbcopy(mp, NULL, sizeof (*t), M_COPYIN);
			}
			qreply(wq, mp);
		} else {
			/*
			 * Verify argument length.
			 */
#ifdef _SYSCALL32_IMPL
			if ((iocp->ioc_flag & IOC_MODELS) != IOC_NATIVE) {
				struct timeval32 *t32;

				if (iocp->ioc_count != sizeof (*t32)) {
					miocnak(wq, mp, 0, EINVAL);
					break;
				}
				t32 = (struct timeval32 *)mp->b_cont->b_rptr;
				if (t32->tv_sec < 0 || t32->tv_usec < 0) {
					miocnak(wq, mp, 0, EINVAL);
					break;
				}
				ticks = TIMEVAL_TO_TICK(t32);
			} else
#endif /* _SYSCALL32_IMPL */
			{
				if (iocp->ioc_count != sizeof (*t)) {
					miocnak(wq, mp, 0, EINVAL);
					break;
				}

				t = (struct timeval *)mp->b_cont->b_rptr;
				if (t->tv_sec < 0 || t->tv_usec < 0) {
					miocnak(wq, mp, 0, EINVAL);
					break;
				}
				ticks = TIMEVAL_TO_TICK(t);
			}
			sbp->sb_ticks = ticks;
			if (ticks == 0)
				sbp->sb_chunk = 0;
			miocack(wq, mp, 0, 0);
			sbclosechunk(sbp);
		}
		break;

	case SBIOCGTIME: {
		struct timeval *t;

		/*
		 * Verify argument length.
		 */
		if (transparent != TRANSPARENT) {
#ifdef _SYSCALL32_IMPL
			if ((iocp->ioc_flag & IOC_MODELS) != IOC_NATIVE) {
				if (iocp->ioc_count !=
				    sizeof (struct timeval32)) {
					miocnak(wq, mp, 0, EINVAL);
					break;
				}
			} else
#endif /* _SYSCALL32_IMPL */
			if (iocp->ioc_count != sizeof (*t)) {
				miocnak(wq, mp, 0, EINVAL);
				break;
			}
		}

		/*
		 * If infinite timeout, return range error
		 * for the ioctl.
		 */
		if (sbp->sb_ticks < 0) {
			miocnak(wq, mp, 0, ERANGE);
			break;
		}

#ifdef _SYSCALL32_IMPL
		if ((iocp->ioc_flag & IOC_MODELS) != IOC_NATIVE) {
			struct timeval32 *t32;

			tmp = allocb(sizeof (*t32), BPRI_MED);
			if (tmp == NULL) {
				miocnak(wq, mp, 0, EAGAIN);
				break;
			}

			if (transparent == TRANSPARENT)
				sbcopy(mp, tmp, sizeof (*t32), M_COPYOUT);
			t32 = (struct timeval32 *)mp->b_cont->b_rptr;
			TICK_TO_TIMEVAL32(sbp->sb_ticks, t32);

			if (transparent == TRANSPARENT) {
				qreply(wq, mp);
			} else {
				miocack(wq, mp, sizeof (*t32), 0);
			}
		} else
#endif /* _SYSCALL32_IMPL */
		{
			tmp = allocb(sizeof (*t), BPRI_MED);
			if (tmp == NULL) {
				miocnak(wq, mp, 0, EAGAIN);
				break;
			}

			if (transparent == TRANSPARENT)
				sbcopy(mp, tmp, sizeof (*t), M_COPYOUT);
			t = (struct timeval *)mp->b_cont->b_rptr;
			TICK_TO_TIMEVAL(sbp->sb_ticks, t);

			if (transparent == TRANSPARENT) {
				qreply(wq, mp);
			} else {
				miocack(wq, mp, sizeof (*t), 0);
			}
		}
		break;
	}

	case SBIOCCTIME:
		sbp->sb_ticks = -1;
		miocack(wq, mp, 0, 0);
		break;

	case SBIOCSCHUNK:
		if (iocp->ioc_count == TRANSPARENT) {
			sbcopy(mp, (mblk_t *)NULL, sizeof (u_int), M_COPYIN);
			qreply(wq, mp);
		} else {
			/*
			 * Verify argument length.
			 */
			if (iocp->ioc_count != sizeof (u_int)) {
				miocnak(wq, mp, 0, EINVAL);
				break;
			}

			/*
			 * set up hi/lo water marks on stream head read queue.
			 * unlikely to run out of resources. Fix at later date.
			 */
			if (mop = allocb(sizeof (struct stroptions),
			    BPRI_MED)) {
				struct stroptions *sop;
				u_int chunk;

				chunk = *(u_int *)mp->b_cont->b_rptr;
				mop->b_datap->db_type = M_SETOPTS;
				mop->b_wptr += sizeof (struct stroptions);
				sop = (struct stroptions *)mop->b_rptr;
				sop->so_flags = SO_HIWAT | SO_LOWAT;
				sop->so_hiwat = SNIT_HIWAT(chunk, 1);
				sop->so_lowat = SNIT_LOWAT(chunk, 1);
				sop->so_hiwat =
				    (sop->so_hiwat > (SB_DFLT_CHUNK * 8)) ?
				    (SB_DFLT_CHUNK * 8) : sop->so_hiwat;
				sop->so_lowat =
				    (sop->so_lowat > (SB_DFLT_CHUNK * 4)) ?
				    (SB_DFLT_CHUNK * 4) : sop->so_lowat;
				qreply(wq, mop);
			}

			sbp->sb_chunk = *(u_int *)mp->b_cont->b_rptr;
			miocack(wq, mp, 0, 0);
			sbclosechunk(sbp);
		}
		break;

	case SBIOCGCHUNK:
		/*
		 * Verify argument length.
		 */
		if (transparent != TRANSPARENT) {
			if (iocp->ioc_count != sizeof (u_int)) {
				miocnak(wq, mp, 0, EINVAL);
				break;
			}
		}

		tmp = allocb(sizeof (u_int), BPRI_MED);
		if (tmp == NULL) {
			miocnak(wq, mp, 0, EAGAIN);
			break;
		}

		if (transparent == TRANSPARENT)
			sbcopy(mp, tmp, sizeof (u_int), M_COPYOUT);

		*(u_int *)mp->b_cont->b_rptr = sbp->sb_chunk;

		if (transparent == TRANSPARENT) {
			qreply(wq, mp);
		} else {
			miocack(wq, mp, sizeof (u_int), 0);
		}
		break;

	case SBIOCSSNAP:
		if (iocp->ioc_count == TRANSPARENT) {
			sbcopy(mp, NULL, sizeof (u_int), M_COPYIN);
			qreply(wq, mp);
		} else {
			/*
			 * Verify argument length.
			 */
			if (iocp->ioc_count != sizeof (u_int)) {
				miocnak(wq, mp, 0, EINVAL);
				break;
			}

			/*
			 * if chunking dont worry about effects of
			 * snipping of message size on head flow control
			 * since it has a relatively small bearing on the
			 * data rate onto the streamn head.
			 */
			if (!sbp->sb_chunk) {
				/*
				 * set up hi/lo water marks on stream
				 * head read queue.  unlikely to run out
				 * of resources. Fix at later date.
				 */
				if (mop = allocb(sizeof (struct stroptions),
				    BPRI_MED)) {
					struct stroptions *sop;
					u_int snap;
					int fudge;

					snap = *(u_int *)mp->b_cont->b_rptr;
					mop->b_datap->db_type = M_SETOPTS;
					mop->b_wptr += sizeof (*sop);
					sop = (struct stroptions *)mop->b_rptr;
					sop->so_flags = SO_HIWAT | SO_LOWAT;
					fudge = (snap <= 100) ? 4 :
					    (snap <= 400) ? 2 : 1;
					sop->so_hiwat = SNIT_HIWAT(snap, fudge);
					sop->so_lowat = SNIT_LOWAT(snap, fudge);
					sop->so_hiwat =
					    (sop->so_hiwat >
						(SB_DFLT_CHUNK * 8)) ?
					    (SB_DFLT_CHUNK * 8) : sop->so_hiwat;
					sop->so_lowat =
					    (sop->so_lowat >
						(SB_DFLT_CHUNK * 4)) ?
					    (SB_DFLT_CHUNK * 4) : sop->so_lowat;
					qreply(wq, mop);
				}
			}

			sbp->sb_snap = *(u_int *)mp->b_cont->b_rptr;

			miocack(wq, mp, 0, 0);
		}
		break;

	case SBIOCGSNAP:
		/*
		 * Verify argument length
		 */
		if (transparent != TRANSPARENT) {
			if (iocp->ioc_count != sizeof (u_int)) {
				miocnak(wq, mp, 0, EINVAL);
				break;
			}
		}

		tmp = allocb(sizeof (u_int), BPRI_MED);
		if (tmp == NULL) {
			miocnak(wq, mp, 0, EAGAIN);
			break;
		}

		if (transparent == TRANSPARENT)
			sbcopy(mp, tmp, sizeof (u_int), M_COPYOUT);

		*(u_int *)mp->b_cont->b_rptr = sbp->sb_snap;

		if (transparent == TRANSPARENT) {
			qreply(wq, mp);
		} else {
			miocack(wq, mp, sizeof (u_int), 0);
		}
		break;

	case SBIOCSFLAGS:
		/*
		 * set the flags.
		 */
		if (iocp->ioc_count == TRANSPARENT) {
			sbcopy(mp, (mblk_t *)NULL, sizeof (u_int), M_COPYIN);
			qreply(wq, mp);
		} else {
			if (iocp->ioc_count != sizeof (u_int)) {
				miocnak(wq, mp, 0, EINVAL);
				break;
			}
			sbp->sb_flags = *(u_int *)mp->b_cont->b_rptr;
			miocack(wq, mp, 0, 0);
		}
		break;

	case SBIOCGFLAGS:
		/*
		 * Verify argument length
		 */
		if (transparent != TRANSPARENT) {
			if (iocp->ioc_count != sizeof (u_int)) {
				miocnak(wq, mp, 0, EINVAL);
				break;
			}
		}

		tmp = allocb(sizeof (u_int), BPRI_MED);
		if (tmp == NULL) {
			miocnak(wq, mp, 0, EAGAIN);
			break;
		}

		if (transparent == TRANSPARENT)
			sbcopy(mp, tmp, sizeof (u_int), M_COPYOUT);

		*(u_int *)mp->b_cont->b_rptr = sbp->sb_flags;

		if (transparent == TRANSPARENT) {
			qreply(wq, mp);
		} else {
			miocack(wq, mp, sizeof (u_int), 0);
		}
		break;


	default:
		putnext(wq, mp);
		break;
	}
}

/*
 * Given a length l, calculate the amount of extra storage
 * required to round it up to the next multiple of the alignment a.
 */
#define	RoundUpAmt(l, a)	((l) % (a) ? (a) - ((l) % (a)) : 0)
/*
 * Calculate additional amount of space required for alignment.
 */
#define	Align(l)		RoundUpAmt(l, sizeof (u_long))

/*
 * Augment the message given as argument with a sb_hdr header
 * and with enough trailing padding to satisfy alignment constraints.
 * Return a pointer to the augmented message, or NULL if allocation
 * failed.
 */
static mblk_t *
sbaugmsg(mblk_t *mp)
{
	struct sb_hdr *hp;
	size_t	pad;
	mblk_t	*tmp;
	size_t	len;

	len = msgdsize(mp);

	/*
	 * Get space for the header.  If there's room for it in the
	 * message's leading dblk and it would be correctly aligned,
	 * stash it there; otherwise hook in a fresh one.
	 *
	 * Some slight sleaze: the last clause of the test relies on
	 * sizeof (struct sb_hdr) being a multiple of sizeof (u_int).
	 *
	 * The implicit "sleaze" is that sizeof (struct sb_hdr) is a
	 * multiple of the worst alignment requirement (8) that we can
	 * currently suffer. If an alignment requirement of a 16-byte
	 * boundary becomes possible, then this "sleaze" would stop
	 * working on such data.
	 */

	if ((MBLKHEAD(mp) >= sizeof (*hp)) &&
	    (DB_REF(mp) == 1) &&
	    ((uintptr_t)mp->b_rptr & (sizeof (u_int) - 1)) == 0) {
		/*
		 * Adjust data pointers to precede old beginning.
		 */
		mp->b_rptr -= sizeof (*hp);
	} else {
		tmp = allocb(sizeof (*hp), BPRI_MED);
		if (tmp == NULL) {
			freemsg(mp);
			return ((mblk_t *)NULL);
		}
		tmp->b_wptr += sizeof (*hp);
		tmp->b_cont = mp;
		mp = tmp;
	}

	/*
	 * Fill it in.
	 *
	 * We maintain the invariant: sb_m_len % sizeof (u_long) == 0.
	 * Since x % a == 0 && y % a == 0 ==> (x + y) % a == 0, we can
	 * pad the message out to the correct alignment boundary without
	 * having to worry about how long the currently accumulating
	 * chunk is already.
	 */
	hp = (struct sb_hdr *)mp->b_rptr;
	hp->sbh_msglen = len;
	len += sizeof (*hp);
	pad = Align(len);
	hp->sbh_totlen = len + pad;
	hp->sbh_drops = 0;

	/*
	 * Add padding.  If there's room at the end of the message
	 * simply extend the wptr of the last mblk.  Otherwise,
	 * allocate a new mblk and tack it on.
	 */
	if (pad) {
		/*
		 * Find the message's last mblk.
		 */
		for (tmp = mp; tmp->b_cont; tmp = tmp->b_cont)
			;

		if ((MBLKTAIL(tmp) >= pad) &&
			(DB_REF(tmp) == 1))
			tmp->b_wptr += pad;
		else {
			tmp = allocb(pad, BPRI_MED);
			if (tmp == NULL) {
				freemsg(mp);
				return ((mblk_t *)NULL);
			}
			tmp->b_wptr += pad;
			linkb(mp, tmp);
		}
	}

	return (mp);
}

/*
 * Process a read-side M_DATA message.
 *
 * First condition the message for inclusion in a chunk, by adding
 * a sb_hdr header and trailing alignment padding.
 *
 * If the currently accumulating chunk doesn't have enough room
 * for the message, close off the chunk, pass it upward, and start
 * a new one.  Then add the message to the current chunk, taking
 * account of the possibility that the message's size exceeds the
 * chunk size.
 */
static void
sbaddmsg(queue_t *rq, mblk_t *mp)
{
	struct sb *sbp = (struct sb *)rq->q_ptr;
	struct timeval t;
	struct sb_hdr *sbhp;
	size_t mlen, mnew;
	size_t origlen;

	uniqtime(&t);

	origlen = msgdsize(mp);

	/*
	 * Truncate the message.
	 */
	if ((sbp->sb_snap > 0) && (origlen > sbp->sb_snap))
		(void) adjmsg(mp, -(origlen - sbp->sb_snap));

	if (!(sbp->sb_flags & SB_NO_HEADER)) {
		/*
		 * Add header and padding to the message.
		 */
		if ((mp = sbaugmsg(mp)) == NULL) {
			/*
			 * Memory allocation failure.
			 * This will need to be revisited
			 * since using certain flag combinations
			 * can result in messages being dropped
			 * silently.
			 */
			sbp->sb_drops++;
			return;
		}

		/*
		 * Fill in origlen, timestamp, and dropcount fields.
		 */
		sbhp = (struct sb_hdr *)mp->b_rptr;
		sbhp->sbh_origlen = origlen;
		TIMEVAL_TO_TIMEVAL32(&sbhp->sbh_timestamp, &t);
		sbhp->sbh_drops = sbp->sb_drops;
		mlen = ((struct sb_hdr *)mp->b_rptr)->sbh_totlen;
	} else
		mlen = origlen;

	/*
	 * If the padded message won't fit in the current chunk,
	 * close the chunk off and start a new one.
	 */
	mnew = sbp->sb_mlen + mlen;
	if (mnew > sbp->sb_chunk)
		sbclosechunk(sbp);

	/*
	 * If it still doesn't fit, pass it on directly, bypassing
	 * the chunking mechanism.  Note that if we're not chunking
	 * things up at all (chunk == 0), we'll pass the message
	 * upward here.
	 */
	if ((mlen > sbp->sb_chunk) || (sbp->sb_chunk == 0)) {
		sbsendit(rq, mp);
		return;
	}

	/*
	 * We now know that the msg will fit in the chunk.
	 * Link it onto the end of the chunk.
	 * Since linkb() walks the entire chain, we keep a pointer to
	 * the first mblk of the last msgb added and call linkb on that
	 * tail of the mblk chain rather than performing the O(n) linkb()
	 * operation on the whole chain.
	 * Note that sb_tail doesn't point to the last msgb,
	 * as the messages are typically multiple msgb's long because
	 * of the bufmod header and padding added.
	 */
	if (sbp->sb_mp)
		linkb(sbp->sb_tail, mp);
	else
		sbp->sb_mp = mp;
	sbp->sb_tail = mp;
	sbp->sb_mlen += mlen;
	sbp->sb_mcount++;
}

/*
 * Called from timeout().
 * Signal a timeout by passing a zero-length M_CTL msg in the read-side
 * to synchronize with any active module threads (open, close, wput, rput).
 */
static void
sbtick(void *arg)
{
	struct sb *sbp = arg;
	queue_t	*rq;

	ASSERT(sbp);

	rq = sbp->sb_rq;
	sbp->sb_timeoutid = 0;		/* timeout has fired */

	if (putctl(rq, M_CTL) == 0)	/* failure */
		sbp->sb_timeoutid = qtimeout(rq, sbtick, sbp, sbp->sb_ticks);
}

/*
 * Close off the currently accumulating chunk and pass
 * it upward.  Takes care of resetting timers as well.
 *
 * This routine is called both directly and as a result
 * of the chunk timeout expiring.
 */
static void
sbclosechunk(struct sb *sbp)
{
	mblk_t	*mp;
	queue_t	*rq;

	ASSERT(sbp);

	if (sbp->sb_timeoutid) {
		(void) quntimeout(sbp->sb_rq, sbp->sb_timeoutid);
		sbp->sb_timeoutid = 0;
	}

	mp = sbp->sb_mp;
	rq = sbp->sb_rq;

	/*
	 * If there's currently a chunk in progress, close it off
	 * and try to send it up.
	 */
	if (mp) {
		sbsendit(rq, mp);
	}

	/*
	 * Clear old chunk.  Ready for new msgs.
	 */
	sbp->sb_tail = sbp->sb_mp = NULL;
	sbp->sb_mlen = 0;
	sbp->sb_mcount = 0;
	if (sbp->sb_flags & SB_DEFER_CHUNK)
		sbp->sb_state &= ~SB_FRCVD;

}

static void
sbsendit(queue_t *rq, mblk_t *mp)
{
	struct	sb	*sbp = (struct sb *)rq->q_ptr;

	if (!canputnext(rq)) {
		if (sbp->sb_flags & SB_NO_DROPS)
			(void) putq(rq, mp);
		else {
			freemsg(mp);
			sbp->sb_drops += sbp->sb_mcount;
		}
		return;
	}
	/*
	 * If there are messages on the q already, keep
	 * queueing them since they need to be processed in order.
	 */
	if (qsize(rq) > 0) {
		/* should only get here if SB_NO_DROPS */
		(void) putq(rq, mp);
	}
	else
		putnext(rq, mp);
}

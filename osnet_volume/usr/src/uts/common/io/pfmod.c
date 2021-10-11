/*
 * Copyright (c) 1991,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pfmod.c	1.13	99/03/21 SMI"

/*
 * STREAMS Packet Filter Module
 *
 * This module applies a filter to messages arriving on its read
 * queue, passing on messages that the filter accepts adn discarding
 * the others.  It supports ioctls for setting the filter.
 *
 * On the write side, the module simply passes everything through
 * unchanged.
 *
 * Based on SunOS 4.x version.  This version has minor changes:
 *	- general SVR4 porting stuff
 * 	- change name and prefixes from "nit" buffer to streams buffer
 *	- multithreading assumes configured as D_MTQPAIR
 */

#include	<sys/types.h>
#include	<sys/sysmacros.h>
#include	<sys/errno.h>
#include	<sys/debug.h>
#include	<sys/time.h>
#include	<sys/stropts.h>
#include	<sys/stream.h>
#include	<sys/conf.h>
#include	<sys/ddi.h>
#include	<sys/sunddi.h>
#include	<sys/kmem.h>
#include	<sys/strsun.h>
#include	<sys/pfmod.h>

/*
 * Expanded version of the Packetfilt structure that includes
 * some additional fields that aid filter execution efficiency.
 */
struct epacketfilt {
	struct Pf_ext_packetfilt	pf;
#define	pf_Priority	pf.Pf_Priority
#define	pf_FilterLen	pf.Pf_FilterLen
#define	pf_Filter	pf.Pf_Filter
	/* pointer to word immediately past end of filter */
	ushort_t		*pf_FilterEnd;
	/* length in bytes of packet prefix the filter examines */
	ushort_t		pf_PByteLen;
};

/*
 * (Internal) packet descriptor for FilterPacket
 */
struct packdesc {
	ushort_t	*pd_hdr;	/* header starting address */
	uint_t		pd_hdrlen;	/* header length in shorts */
	ushort_t	*pd_body;	/* body starting address */
	uint_t		pd_bodylen;	/* body length in shorts */
};


/*
 * Function prototypes.
 */
static	int	pfopen(queue_t *, dev_t *, int, int, cred_t *);
static	int	pfclose(queue_t *);
static void	pfioctl(queue_t *wq, mblk_t *mp);
static	int	FilterPacket(struct packdesc *, struct epacketfilt *);
/*
 * To save instructions, since STREAMS ignores the return value
 * from these functions, they are defined as void here. Kind of icky, but...
 */
static void	pfwput(queue_t *, mblk_t *);
static void	pfrput(queue_t *, mblk_t *);

struct module_info	pf_minfo = {
	22,		/* mi_idnum */
	"pfmod",	/* mi_idname */
	0,		/* mi_minpsz */
	INFPSZ,		/* mi_maxpsz */
	0,		/* mi_hiwat */
	0		/* mi_lowat */
};

struct qinit	pf_rinit = {
	(int (*)())pfrput,	/* qi_putp */
	NULL,
	pfopen,			/* qi_qopen */
	pfclose,		/* qi_qclose */
	NULL,			/* qi_qadmin */
	&pf_minfo,		/* qi_minfo */
	NULL			/* qi_mstat */
};

struct qinit	pf_winit = {
	(int (*)())pfwput,	/* qi_putp */
	NULL,			/* qi_srvp */
	NULL,			/* qi_qopen */
	NULL,			/* qi_qclose */
	NULL,			/* qi_qadmin */
	&pf_minfo,		/* qi_minfo */
	NULL			/* qi_mstat */
};

struct streamtab	pf_info = {
	&pf_rinit,	/* st_rdinit */
	&pf_winit,	/* st_wrinit */
	NULL,		/* st_muxrinit */
	NULL		/* st_muxwinit */
};

/*
 * This is the loadable module wrapper.
 */
#include <sys/conf.h>
#include <sys/modctl.h>

static struct fmodsw fsw = {
	"pfmod",
	&pf_info,
	D_NEW | D_MTQPAIR | D_MP
};

/*
 * Module linkage information for the kernel.
 */

static struct modlstrmod modlstrmod = {
	&mod_strmodops, "streams packet filter module", &fsw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlstrmod, NULL
};


_init()
{
	return (mod_install(&modlinkage));
}

_fini()
{
	return (mod_remove(&modlinkage));
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

/* ARGSUSED */
static
pfopen(rq, dev, oflag, sflag, crp)
queue_t	*rq;
dev_t	*dev;
int	oflag;
int	sflag;
cred_t	*crp;
{
	struct epacketfilt	*pfp;

	ASSERT(rq);

	if (sflag != MODOPEN)
		return (EINVAL);

	if (rq->q_ptr)
		return (0);

	/*
	 * Allocate and initialize per-Stream structure.
	 */
	pfp = kmem_alloc(sizeof (struct epacketfilt), KM_SLEEP);
	rq->q_ptr = WR(rq)->q_ptr = (char *)pfp;

	qprocson(rq);

	return (0);
}

static
pfclose(rq)
queue_t	*rq;
{
	struct	epacketfilt	*pfp = (struct epacketfilt *)rq->q_ptr;

	ASSERT(pfp);

	qprocsoff(rq);

	kmem_free((caddr_t)pfp, sizeof (struct epacketfilt));
	rq->q_ptr = WR(rq)->q_ptr = NULL;

	return (0);
}

/*
 * Write-side put procedure.  Its main task is to detect ioctls.
 * Other message types are passed on through.
 */
static void
pfwput(queue_t *wq, mblk_t *mp)
{
	switch (mp->b_datap->db_type) {
		case M_IOCTL:
			pfioctl(wq, mp);
			break;

		default:
			putnext(wq, mp);
			break;
	}
}

/*
 * Read-side put procedure.  It's responsible for applying the
 * packet filter and passing upstream message on or discarding it
 * depending upon the results.
 *
 * Upstream messages can start with zero or more M_PROTO mblks
 * which are skipped over before executing the packet filter
 * on any remaining M_DATA mblks.
 */
static void
pfrput(queue_t *rq, mblk_t *mp)
{
	struct	epacketfilt	*pfp =
		(struct epacketfilt *)rq->q_ptr;
	mblk_t	*mbp, *mpp;
	struct	packdesc	pd;
	int	need;

	ASSERT(pfp);

	switch (DB_TYPE(mp)) {

	case M_PROTO:
	case M_DATA:
		/*
		 * Skip over protocol information and find the start
		 * of the message body, saving the overall message
		 * start in mpp.
		 */
		for (mpp = mp; mp && (DB_TYPE(mp) == M_PROTO); mp = mp->b_cont)
			;

		/*
		 * Null body (exclusive of M_PROTO blocks) ==> accept.
		 * Note that a null body is not the same as an empty body.
		 */
		if (mp == NULL) {
			putnext(rq, mpp);
			break;
		}

		/*
		 * Pull the packet up to the length required by
		 * the filter.  Note that doing so destroys sharing
		 * relationships, which is unfortunate, since the
		 * results of pulling up here are likely to be useful
		 * for shared messages applied to a filter on a sibling
		 * stream.
		 *
		 * Most packet sources will provide the packet in two
		 * logical pieces: an initial header in a single mblk,
		 * and a body in a sequence of mblks hooked to the
		 * header.  We're prepared to deal with variant forms,
		 * but in any case, the pullup applies only to the body
		 * part.
		 */
		mbp = mp->b_cont;
		need = pfp->pf_PByteLen;
		if (mbp && (MBLKL(mbp) < need)) {
			int	len = msgdsize(mbp);

			/* XXX discard silently on pullupmsg failure */
			if (pullupmsg(mbp, MIN(need, len)) == 0) {
				freemsg(mpp);
				break;
			}
		}

		/*
		 * Misaliagnment (not on short boundary) ==> reject.
		 */
		if (((uintptr_t)mp->b_rptr & (sizeof (ushort_t) - 1)) ||
		    (mbp != NULL &&
		    ((uintptr_t)mbp->b_rptr & (sizeof (ushort_t) - 1)))) {
			freemsg(mpp);
			break;
		}

		/*
		 * These assignments are distasteful, but necessary,
		 * since the packet filter wants to work in terms of
		 * shorts.  Odd bytes at the end of header or data can't
		 * participate in the filtering operation.
		 */
		pd.pd_hdr = (ushort_t *)mp->b_rptr;
		pd.pd_hdrlen = (mp->b_wptr - mp->b_rptr) / sizeof (ushort_t);
		if (mbp) {
			pd.pd_body = (ushort_t *)mbp->b_rptr;
			pd.pd_bodylen = (mbp->b_wptr - mbp->b_rptr) /
							sizeof (ushort_t);
		} else {
			pd.pd_body = NULL;
			pd.pd_bodylen = 0;
		}

		/*
		 * Apply the filter.
		 */
		if (FilterPacket(&pd, pfp))
			putnext(rq, mpp);
		else
			freemsg(mpp);

		break;

	default:
		putnext(rq, mp);
		break;
	}

}

/*
 * Handle write-side M_IOCTL messages.
 */
static void
pfioctl(queue_t *wq, mblk_t *mp)
{
	struct	epacketfilt	*pfp = (struct epacketfilt *)wq->q_ptr;
	struct	Pf_ext_packetfilt	*upfp;
	struct	packetfilt	*opfp;
	ushort_t	*fwp;
	int	maxoff, arg;
	struct iocblk	*iocp = (struct iocblk *)mp->b_rptr;

	switch (iocp->ioc_cmd) {
	case PFIOCSETF:
		/*
		 * Verify argument length. Since the size of packet filter
		 * got increased (ENMAXFILTERS was bumped up to 2047), to
		 * maintain backwards binary compatibility, we need to
		 * check for both possible sizes.
		 */

		if (iocp->ioc_count == sizeof (struct Pf_ext_packetfilt)) {
			mblk_t *mp1 = mp->b_cont;
			int  mlen = msgdsize(mp1);
			char  *ptr;
			upfp = (struct Pf_ext_packetfilt *)mp1->b_rptr;
			if (upfp->Pf_FilterLen > PF_MAXFILTERS ||
				mlen != sizeof (struct Pf_ext_packetfilt)) {
				miocnak(wq, mp, 0, EINVAL);
				break;
			}

			/*
			 * Install the new filter.
			 */
			ptr = (caddr_t)pfp;
			for (; mp1 != NULL && mlen > 0; mp1 = mp1->b_cont) {
				int cnt;

				cnt = mp1->b_wptr - mp1->b_rptr;
				bcopy(mp1->b_rptr, ptr, cnt);
				ptr += cnt;
				mlen -= cnt;
			}
			pfp->pf_FilterEnd = &pfp->pf_Filter[pfp->pf_FilterLen];
		} else {
			/*
			 * Check if its old packet filter.
			 */
			if (iocp->ioc_count == sizeof (struct packetfilt)) {
				opfp = (struct packetfilt *)
					mp->b_cont->b_rptr;
				if (opfp->Pf_FilterLen > ENMAXFILTERS ||
				    (mp->b_cont->b_wptr - mp->b_cont->b_rptr !=
					sizeof (struct packetfilt))) {
					miocnak(wq, mp, 0, EINVAL);
					break;
				}

				pfp->pf.Pf_Priority = opfp->Pf_Priority;
				pfp->pf.Pf_FilterLen =
					(unsigned int)opfp->Pf_FilterLen;

				/*
				 * Install the new filter.
				 */
				bcopy((caddr_t)opfp->Pf_Filter,
					(caddr_t)pfp->pf.Pf_Filter,
					sizeof (struct packetfilt));
				pfp->pf_FilterEnd =
					&pfp->pf_Filter[pfp->pf_FilterLen];
			} else {
				/* NAK it */
				miocnak(wq, mp, 0, EINVAL);
				break;
			}
		}

		/*
		 * Find and record maximum byte offset that the
		 * filter users.  We use this when executing the
		 * filter to determine how much of the packet
		 * body to pull up.  This code depends on the
		 * filter encoding.
		 */
		maxoff = 0;
		for (fwp = pfp->pf_Filter; fwp < pfp->pf_FilterEnd; fwp++) {
			arg = *fwp & ((1 << ENF_NBPA) - 1);
			switch (arg) {
			default:
				if ((arg -= ENF_PUSHWORD) > maxoff)
					maxoff = arg;
				break;

			case ENF_PUSHLIT:
				/* Skip over the literal. */
				fwp++;
				break;

			case ENF_PUSHZERO:
			case ENF_PUSHONE:
			case ENF_PUSHFFFF:
			case ENF_PUSHFF00:
			case ENF_PUSH00FF:
			case ENF_NOPUSH:
				break;
			}
		}

		/*
		 * Convert word offset to length in bytes.
		 */
		pfp->pf_PByteLen = (maxoff + 1) * sizeof (ushort_t);

		miocack(wq, mp, 0, 0);
		break;

	default:
		putnext(wq, mp);
		break;
	}
}

/* #define	DEBUG	1 */
/* #define	INNERDEBUG	1 */

#ifdef	INNERDEBUG
#define	enprintf(flags)	if (enDebug & (flags)) printf

/*
 * Symbolic definitions for enDebug flag bits
 *	ENDBG_TRACE should be 1 because it is the most common
 *	use in the code, and the compiler generates faster code
 *	for testing the low bit in a word.
 */

#define	ENDBG_TRACE	1	/* trace most operations */
#define	ENDBG_DESQ	2	/* trace descriptor queues */
#define	ENDBG_INIT	4	/* initialization info */
#define	ENDBG_SCAV	8	/* scavenger operation */
#define	ENDBG_ABNORM	16	/* abnormal events */

int	enDebug = /* ENDBG_ABNORM | ENDBG_INIT | ENDBG_TRACE */ -1;
#endif /* INNERDEBUG */

/*
 * Apply the packet filter given by pfp to the packet given by
 * pp.  Return nonzero iff the filter accepts the packet.
 *
 * The packet comes in two pieces, a header and a body, since
 * that's the most convenient form for our caller.  The header
 * is in contiguous memory, whereas the body is in a mbuf.
 * Our caller will have adjusted the mbuf chain so that its first
 * min(MLEN, length(body)) bytes are guaranteed contiguous.  For
 * the sake of efficiency (and some laziness) the filter is prepared
 * to examine only these two contiguous pieces.  Furthermore, it
 * assumes that the header length is even, so that there's no need
 * to glue the last byte of header to the first byte of data.
 */

#define	opx(i)	((i) >> ENF_NBPA)

static
FilterPacket(pp, pfp)
struct	packdesc	*pp;
struct	epacketfilt	*pfp;
{
	int		maxhdr = pp->pd_hdrlen;
	int		maxword = maxhdr + pp->pd_bodylen;
	ushort_t	*sp;
	ushort_t	*fp;
	ushort_t	*fpe;
	unsigned	op;
	unsigned	arg;
	ushort_t	stack[ENMAXFILTERS+1];

	fp = &pfp->pf_Filter[0];
	fpe = pfp->pf_FilterEnd;

#ifdef	INNERDEBUG
	enprintf(ENDBG_TRACE)("FilterPacket(%x, %x, %x, %x):\n",
		pp, pfp, fp, fpe);
#endif

	/*
	 * Push TRUE on stack to start.  The stack size is chosen such
	 * that overflow can't occur -- each operation can push at most
	 * one item on the stack, and the stack size equals the maximum
	 * program length.
	 */
	sp = &stack[ENMAXFILTERS];
	*sp = 1;

	while (fp < fpe) {
	op = *fp >> ENF_NBPA;
	arg = *fp & ((1 << ENF_NBPA) - 1);
	fp++;

	switch (arg) {
	    default:
		arg -= ENF_PUSHWORD;
		/*
		 * Since arg is unsigned,
		 * if it were less than ENF_PUSHWORD before,
		 * it would now be huge.
		 */
		if (arg < maxhdr)
		    *--sp = pp->pd_hdr[arg];
		else if (arg < maxword)
		    *--sp = pp->pd_body[arg - maxhdr];
		else {
#ifdef	INNERDEBUG
		    enprintf(ENDBG_TRACE)("=>0(len)\n");
#endif
		    return (0);
		}
		break;
	    case ENF_PUSHLIT:
		*--sp = *fp++;
		break;
	    case ENF_PUSHZERO:
		*--sp = 0;
		break;
	    case ENF_PUSHONE:
		*--sp = 1;
		break;
	    case ENF_PUSHFFFF:
		*--sp = 0xffff;
		break;
	    case ENF_PUSHFF00:
		*--sp = 0xff00;
		break;
	    case ENF_PUSH00FF:
		*--sp = 0x00ff;
		break;
	    case ENF_NOPUSH:
		break;
	}
	if (sp < &stack[2]) {	/* check stack overflow: small yellow zone */
#ifdef	INNERDEBUG
	    enprintf(ENDBG_TRACE)("=>0(--sp)\n");
#endif
	    return (0);
	}
	if (op == ENF_NOP)
	    continue;
	/*
	 * all non-NOP operators binary, must have at least two operands
	 * on stack to evaluate.
	 */
	if (sp > &stack[ENMAXFILTERS-2]) {
#ifdef	INNERDEBUG
	    enprintf(ENDBG_TRACE)("=>0(sp++)\n");
#endif
	    return (0);
	}
	arg = *sp++;
	switch (op) {
	    default:
#ifdef	INNERDEBUG
		enprintf(ENDBG_TRACE)("=>0(def)\n");
#endif
		return (0);
	    case opx(ENF_AND):
		*sp &= arg;
		break;
	    case opx(ENF_OR):
		*sp |= arg;
		break;
	    case opx(ENF_XOR):
		*sp ^= arg;
		break;
	    case opx(ENF_EQ):
		*sp = (*sp == arg);
		break;
	    case opx(ENF_NEQ):
		*sp = (*sp != arg);
		break;
	    case opx(ENF_LT):
		*sp = (*sp < arg);
		break;
	    case opx(ENF_LE):
		*sp = (*sp <= arg);
		break;
	    case opx(ENF_GT):
		*sp = (*sp > arg);
		break;
	    case opx(ENF_GE):
		*sp = (*sp >= arg);
		break;

	    /* short-circuit operators */

	    case opx(ENF_COR):
		if (*sp++ == arg) {
#ifdef	INNERDEBUG
		    enprintf(ENDBG_TRACE)("=>COR %x\n", *sp);
#endif
		    return (1);
		}
		break;
	    case opx(ENF_CAND):
		if (*sp++ != arg) {
#ifdef	INNERDEBUG
		    enprintf(ENDBG_TRACE)("=>CAND %x\n", *sp);
#endif
		    return (0);
		}
		break;
	    case opx(ENF_CNOR):
		if (*sp++ == arg) {
#ifdef	INNERDEBUG
		    enprintf(ENDBG_TRACE)("=>COR %x\n", *sp);
#endif
		    return (0);
		}
		break;
	    case opx(ENF_CNAND):
		if (*sp++ != arg) {
#ifdef	INNERDEBUG
		    enprintf(ENDBG_TRACE)("=>CNAND %x\n", *sp);
#endif
		    return (1);
		}
		break;
	}
	}
#ifdef	INNERDEBUG
	enprintf(ENDBG_TRACE)("=>%x\n", *sp);
#endif
	return (*sp);
}

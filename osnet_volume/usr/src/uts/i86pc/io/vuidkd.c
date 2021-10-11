/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vuidkd.c	1.10	99/05/20 SMI"

/*
 * VUIDKD module:  put keyboard scan codes into vuid format
 */

#ifndef i86
#define	i86			/* get around broken vuid_event.h */
#endif

#include <sys/param.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/kd.h>
#include <sys/vuid_event.h>
#include <sys/vuidkd.h>
#include <sys/cred.h>

#ifdef CANONICAL_MODE /* [ */
/*
 *   ldterm sends an MC_CANONQUERY down the write queue; if it
 * doesn't get a response, it assumes we're running in some sort
 * of canonical mode.  This doesn't seem to hurt anything, and
 * nobody wants to experiment with a reply, so I've left this
 * ifdefed out.
 */
#include <sys/tty.h>
#include <sys/strtty.h>
#include <sys/ptyvar.h>
#endif /* ] */

#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>


int _init(void);
int _fini(void);
int _info(struct modinfo *);
static int vuidkd_open(queue_t *const, const dev_t *const,
    const int, const int, const cred_t *const);
static int vuidkd_close(queue_t *const, const int, const cred_t *const);
static int vuidkd_rput(queue_t *const, mblk_t *);
static int vuidkd_wput(queue_t *const, mblk_t *);
static void vuidkd_iocack(queue_t *const, mblk_t *const,
    struct iocblk *const, const int);
static void vuidkd_iocnack(queue_t *const, mblk_t *const,
    struct iocblk *const, const int, const int);

#define	M_TYPE(mp)	((mp)->b_datap->db_type)

/*
 * vuidkd_state is THE per module state, which should not change
 * regardless of number of calls to vuidkd_open() or vuidkd_close().
 */
static vuidkd_state_t vuidkd_state = {
	(char)VS_OFF
};

static kmutex_t vuidkd_lock;

static struct module_info vuidkd_iinfo = {
	0,
	"vuidkd",
	0,
	VUIDKDPSZ,
	1000,
	100
};

static struct qinit vuidkd_rinit = {
	vuidkd_rput,
	NULL,
	vuidkd_open,
	vuidkd_close,
	NULL,
	&vuidkd_iinfo,
	NULL
};

static struct module_info vuidkd_oinfo = {
	0,
	"vuidkd",
	0,
	VUIDKDPSZ,
	1000,
	100
};

static struct qinit vuidkd_winit = {
	vuidkd_wput,
	NULL,
	NULL,
	NULL,
	NULL,
	&vuidkd_oinfo,
	NULL
};

struct streamtab vuidkd_info = {
	&vuidkd_rinit,
	&vuidkd_winit,
	NULL,
	NULL
};

/*
 * This is the loadable module wrapper.
 */

/*
 * D_MTQPAIR effectively makes the module single threaded.
 * There can be only one thread active in the module at any time.
 * It may be a read or write thread.
 */
#define	VUIDKD_CONF_FLAG	(D_MP | D_NEW | D_MTQPAIR)

static struct fmodsw fsw = {
	"vuidkd",
	&vuidkd_info,
	VUIDKD_CONF_FLAG
};

static struct modlstrmod modlstrmod = {
	&mod_strmodops,
	"scancode to vuid event",
	&fsw
};

/*
 * Module linkage information for the kernel.
 */
static struct modlinkage modlinkage = {
	MODREV_1,
	&modlstrmod,
	NULL
};

int
_init(void)
{
	int rc;

	mutex_init(&vuidkd_lock, NULL, MUTEX_DEFAULT, NULL);
	if ((rc = mod_install(&modlinkage)) != 0) {
		mutex_destroy(&vuidkd_lock);
	}
	return (rc);
}

int
_fini(void)
{
	int rc;

	if ((rc = mod_remove(&modlinkage)) == 0)
		mutex_destroy(&vuidkd_lock);
	return (rc);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*ARGSUSED1*/
static int
vuidkd_open(queue_t *const qp, const dev_t *const devp,
    const int oflag, const int sflag, const cred_t *const crp)
{
	mutex_enter(&vuidkd_lock);

	/*
	 * If q_ptr already set, we've allocated a struct already
	 */
	if (qp->q_ptr != NULL) {
		mutex_exit(&vuidkd_lock);
		return (0);	 /* not failure -- just simultaneous open */
	}

	/*
	 * Both the read and write queues share the same state structures.
	 */
	qp->q_ptr = &vuidkd_state;
	WR(qp)->q_ptr = &vuidkd_state;
	mutex_exit(&vuidkd_lock);

	qprocson(qp);
	return (0);
}

/*ARGSUSED1*/
static int
vuidkd_close(queue_t *const qp, const int flag, const cred_t *const crp)
{
	ASSERT(qp != NULL);

	qprocsoff(qp);
	flushq(qp, FLUSHALL);
	flushq(OTHERQ(qp), FLUSHALL);
	mutex_enter(&vuidkd_lock);

	/* Dump the associated state structure */
	qp->q_ptr = NULL;

	mutex_exit(&vuidkd_lock);
	return (0);
}

/*
 * Put procedure for input from driver end of stream (read queue).
 */
static int
vuidkd_rput(queue_t *const qp, mblk_t *mp)
{
	extern int atKeyboardConvertScan();
	ASSERT(qp != NULL);
	ASSERT(mp != NULL);

	/*
	 * Handle all the related high priority messages here, hence
	 * should spend the least amount of time here.
	 */
	switch (M_TYPE(mp)) {

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR)
			flushq(qp, FLUSHALL);
		putnext(qp, mp);		/* pass it on */
		break;

	case M_DATA: {
	    mblk_t *bp;
	    vuidkd_state_t *sp = qp->q_ptr;


	    if (VUIDKD_STATE(sp) & VS_ON) {
		    while (mp->b_rptr < mp->b_wptr) {
			Firm_event	*fep;
			int result, keynum, upflag;


			do {
				result = atKeyboardConvertScan(*mp->b_rptr++,
				    &keynum, &upflag);
			} while ((mp->b_rptr < mp->b_wptr) && !result);

			if (!result)
				/* somehow got an incomplete scancode */
				break;
			if ((bp = allocb((int)sizeof (Firm_event),
			    BPRI_HI)) == NULL)
				printf("vuidkd_rput: can't allocate block "
				    "for event\n");
			else {
				fep = (Firm_event *)bp->b_wptr;
				fep->id =
				    vuid_id_addr(vuid_first(VUID_WORKSTATION)) |
				    vuid_id_offset(keynum);
				fep->pair_type = FE_PAIR_NONE;
				fep->value = upflag;
				uniqtime32(&fep->time);
				bp->b_wptr += sizeof (Firm_event);
				putnext(qp, bp);
			}
		    }
		    freemsg(mp);
	    }
	    else
		    putnext(qp, mp);
	    break;
	}

	default:
		putnext(qp, mp);		/* pass it on */
		break;
	}
	return (0);
}

/*
 * Put procedure for write from user end of stream (write queue).
 */
static int
vuidkd_wput(queue_t *const qp, mblk_t *mp)
{
	vuidkd_state_t *sp;

	ASSERT(qp != NULL);
	ASSERT(mp != NULL);

	sp = qp->q_ptr;

	/*
	 * Handle all the related high priority messages here, hence
	 * should spend the least amount of time here.
	 */
	switch (M_TYPE(mp)) {	/* handle hi pri messages here */

#ifdef CANONICAL_MODE /* [ */
	case M_CTL:
		switch (*mp->b_rptr) {

		case MC_CANONQUERY:
			/*
			 * We're being asked whether we do canonicalization
			 * or not.  Send a reply back up indicating whether
			 * we do or not.
			 */
			(void) putctl1(RD(qp)->q_next, M_CTL,
			    (VUIDKD_STATE(sp) & VS_ON) ?
			    MC_NOCANON : MC_DOCANON);
			freemsg(mp);
			break;
		default:
			putnext(qp, mp);
			break;
		}
		break;

#endif /* ] */

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq(qp, FLUSHALL);
		putnext(qp, mp);			/* pass it on */
		break;

	case M_IOCTL: {
		struct iocblk *iocp = (struct iocblk *)mp->b_rptr;

		switch (iocp->ioc_cmd) {
		case TSON:
			if (VUIDKD_STATE(sp) & VS_ON) {
				/* already on */
				vuidkd_iocnack(qp, mp, iocp, ENOTTY, -1);
				return (0);
			} else {
			    VUIDKD_STATE(sp) = VS_ON;
#ifdef CANONICAL_MODE
			    (void) putctl1(RD(qp)->q_next, M_CTL,
				(VUIDKD_STATE(sp) & VS_ON) ?
				MC_NOCANON : MC_DOCANON);
#endif
			    vuidkd_iocack(qp, mp, iocp, 0);
			}
			break;
		case TSOFF:
			if (VUIDKD_STATE(sp) == VS_OFF) {
				/* already off */
				vuidkd_iocnack(qp, mp, iocp, ENOTTY, -1);
				return (0);
			} else {
			    VUIDKD_STATE(sp) = VS_OFF;
#ifdef CANONICAL_MODE
			    (void) putctl1(RD(qp)->q_next, M_CTL,
				(VUIDKD_STATE(sp) & VS_ON) ?
				MC_NOCANON : MC_DOCANON);
#endif
			    vuidkd_iocack(qp, mp, iocp, 0);
			}
			break;
		case VUIDSFORMAT:

			/*
			 * VUIDSFORMAT is known to streamio.c and this
			 * is assume to be non-I_STR & non-TRANSPARENT ioctl
			 */

			if (iocp->ioc_count == TRANSPARENT) {
				mp->b_datap->db_type = M_IOCNAK;
				iocp->ioc_rval = -1;
				iocp->ioc_error = EINVAL;
			} else {

				int format_type;

				format_type = *(int *)(int)mp->b_cont->b_rptr;
				VUIDKD_STATE(sp) = (unchar) format_type;
				mp->b_datap->db_type = M_IOCACK;
				vuidkd_iocack(qp, mp, iocp, 0);
			}

			/* return buffer to pool ASAP */
			if (mp->b_cont) {
				freemsg(mp->b_cont);
				mp->b_cont = NULL;
			}
			return (0);

		case VUIDGFORMAT:

			/* return buffer to pool ASAP */
			if (mp->b_cont) {
				freemsg(mp->b_cont); /* over written below */
				mp->b_cont = NULL;
			}

			/*
			 * VUIDGFORMAT is known to streamio.c and this
			 * is assume to be non-I_STR & non-TRANSPARENT ioctl
			 */

			if (iocp->ioc_count == TRANSPARENT) {
				vuidkd_iocnack(qp, mp, iocp, EINVAL, -1);
				return (0);
			}

			mp->b_cont = allocb(sizeof (int), BPRI_MED);
			if (mp->b_cont == NULL) {
				vuidkd_iocnack(qp, mp, iocp, EAGAIN, -1);
				return (0);
			}

			*(int *)(int)mp->b_cont->b_rptr = (int)VUIDKD_STATE(sp);
			mp->b_cont->b_wptr += sizeof (int);

			iocp->ioc_count = sizeof (int);
			mp->b_datap->db_type = M_IOCACK;
			qreply(qp, mp);
			return (0);

		case VUID_NATIVE:
		case VUIDSADDR:
		case VUIDGADDR:
			vuidkd_iocnack(qp, mp, iocp, ENOTTY, -1);
			return (0);
		default:
			putnext(qp, mp);	/* nothing to process here */
			return (0);
		}
	    break;
	}

	default:
		putnext(qp, mp);		/* pass it on */
		return (0);
	}
	/*NOTREACHED*/
}

static void
vuidkd_iocack(queue_t *const qp, mblk_t *const mp,
	    struct iocblk *const iocp, const int rval)
{
	mblk_t	*tmp;

	ASSERT(qp != NULL);
	ASSERT(mp != NULL);
	ASSERT(iocp != NULL);

	M_TYPE(mp) = M_IOCACK;
	iocp->ioc_rval = rval;
	iocp->ioc_count = iocp->ioc_error = 0;
	if ((tmp = unlinkb(mp)) != NULL)
		freemsg(tmp);
	qreply(qp, mp);
}

static void
vuidkd_iocnack(queue_t *const qp, mblk_t *const mp,
    struct iocblk *const iocp, const int error, const int rval)
{
	ASSERT(qp != NULL);
	ASSERT(mp != NULL);
	ASSERT(iocp != NULL);

	M_TYPE(mp) = M_IOCACK;
	iocp->ioc_rval = rval;
	iocp->ioc_error = error;
	qreply(qp, mp);
}

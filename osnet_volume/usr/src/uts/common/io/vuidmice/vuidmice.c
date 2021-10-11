/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vuidmice.c	1.13	99/05/20 SMI"

/*
 * VUIDMICE module:  put mouse events into vuid format
 */

#include <sys/param.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/sad.h>
#include <sys/vuid_event.h>
#include <sys/vuidmice.h>
#include <sys/msio.h>

#include <sys/conf.h>
#include <sys/modctl.h>

#include <sys/kmem.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

int _init(void);
int _fini(void);
int _info(struct modinfo *);

static int vuidmice_open(queue_t *const, const dev_t *const,
	const int, const int, const cred_t *const);
static int vuidmice_close(queue_t *const, const int, const cred_t *const);
static int vuidmice_rput(queue_t *const, mblk_t *);
static int vuidmice_rsrv(queue_t *const);
static int vuidmice_wput(queue_t *const, mblk_t *);
static void vuidmice_iocnack(queue_t *const, mblk_t *const,
	struct iocblk *const, const int, const int);

void VUID_QUEUE(queue_t *const, mblk_t *);
void VUID_OPEN(queue_t *const);

#define	M_TYPE(mp)	((mp)->b_datap->db_type)

static kmutex_t vuidmice_lock;

static struct module_info vuidmice_iinfo = {
	0,
	VUID_NAME,
	0,
	INFPSZ,
	1000,
	100
};

static struct qinit vuidmice_rinit = {
	vuidmice_rput,
	vuidmice_rsrv,
	vuidmice_open,
	vuidmice_close,
	NULL,
	&vuidmice_iinfo,
	NULL
};

static struct module_info vuidmice_oinfo = {
	0,
	VUID_NAME,
	0,
	INFPSZ,
	1000,
	100
};

static struct qinit vuidmice_winit = {
	vuidmice_wput,
	NULL,
	NULL,
	NULL,
	NULL,
	&vuidmice_oinfo,
	NULL
};

struct streamtab vuidmice_info = {
	&vuidmice_rinit,
	&vuidmice_winit,
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
#define	VUIDMICE_CONF_FLAG	(D_MP | D_NEW | D_MTQPAIR)

static struct fmodsw fsw = {
	VUID_NAME,
	&vuidmice_info,
	VUIDMICE_CONF_FLAG
};

static struct modlstrmod modlstrmod = {
	&mod_strmodops,
	"mouse events to vuid events",
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

static int module_open = 0;	/* allow only one open of this module */

int
_init(void)
{
	register int rc;

	mutex_init(&vuidmice_lock, NULL, MUTEX_DEFAULT, NULL);
	if ((rc = mod_install(&modlinkage)) != 0) {
		mutex_destroy(&vuidmice_lock);
	}
	return (rc);
}

int
_fini(void)
{
	register int rc;

	if ((rc = mod_remove(&modlinkage)) == 0)
		mutex_destroy(&vuidmice_lock);
	return (rc);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/* ARGSUSED1 */
static int
vuidmice_open(queue_t *const qp, const dev_t *const devp,
	const int oflag, const int sflag, const cred_t *const crp)
{
	mutex_enter(&vuidmice_lock);

	/* Allow only 1 open of this module */

	if (module_open) {
		mutex_exit(&vuidmice_lock);
		return (EBUSY);
	}

	module_open++;

	/*
	 * If q_ptr already set, we've allocated a struct already
	 */
	if (qp->q_ptr != NULL) {
		module_open--;
		mutex_exit(&vuidmice_lock);
		return (0);	 /* not failure -- just simultaneous open */
	}

	/*
	 * Both the read and write queues share the same state structures.
	 */

	qp->q_ptr = kmem_zalloc(sizeof (struct MouseStateInfo), KM_SLEEP);

	WR(qp)->q_ptr = qp->q_ptr;

	/* initialize state */
	STATEP->format = VUID_NATIVE;

	mutex_exit(&vuidmice_lock);

	qprocson(qp);

#ifdef	VUID_OPEN
	VUID_OPEN(qp);
#endif

	return (0);
}

/* ARGSUSED1 */
static int
vuidmice_close(queue_t *const qp, const int flag, const cred_t *const crp)
{
	ASSERT(qp != NULL);

	qprocsoff(qp);
	flushq(qp, FLUSHALL);
	flushq(OTHERQ(qp), FLUSHALL);

	mutex_enter(&vuidmice_lock);
	module_open--;
	kmem_free(qp->q_ptr, sizeof (struct MouseStateInfo));
	qp->q_ptr = NULL;	/* Dump the associated state structure */
	mutex_exit(&vuidmice_lock);

	return (0);
}

/*
 * Put procedure for input from driver end of stream (read queue).
 */
static int
vuidmice_rput(queue_t *const qp, mblk_t *mp)
{
	ASSERT(qp != NULL);
	ASSERT(mp != NULL);

	/*
	 * Handle all the related high priority messages here, hence
	 * should spend the least amount of time here.
	 */

	if (M_TYPE(mp) == M_DATA) {
		if ((int)STATEP->format ==  VUID_FIRM_EVENT)
			return (putq(qp, mp));   /* queue message & return */
	} else if (M_TYPE(mp) == M_FLUSH) {
			if (*mp->b_rptr & FLUSHR)
				flushq(qp, FLUSHALL);
	}

	putnext(qp, mp);	/* pass it on */
	return (0);
}

static int
vuidmice_rsrv(queue_t *const qp)
{
	register mblk_t *mp;

	ASSERT(qp != NULL);

	while ((mp = getq(qp)) != NULL) {
		ASSERT(M_TYPE(mp) == M_DATA);

		if (!canputnext(qp))
			return (putbq(qp, mp)); /* read side is blocked */

		switch (M_TYPE(mp)) {
			case M_DATA: {
				if ((int)STATEP->format == VUID_FIRM_EVENT)
					(void) VUID_QUEUE(qp, mp);
				else
					(void) putnext(qp, mp);
				break;
			}

			default:
				cmn_err(CE_WARN,
				"vuidmice_rsrv: bad message type (%#x)\n",
					M_TYPE(mp));

				(void) putnext(qp, mp);
				break;
		}
	}
	return (0);
}

/*
 * Put procedure for write from user end of stream (write queue).
 */
static int
vuidmice_wput(queue_t *const qp, mblk_t *mp)
{

	ASSERT(qp != NULL);
	ASSERT(mp != NULL);


	/*
	 * Handle all the related high priority messages here, hence
	 * should spend the least amount of time here.
	 */
	switch (M_TYPE(mp)) {	/* handle hi pri messages here */
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq(qp, FLUSHALL);
		putnext(qp, mp);			/* pass it on */
		return (0);

	case M_IOCTL: {
		struct iocblk *iocbp = (struct iocblk *)mp->b_rptr;

		switch (iocbp->ioc_cmd) {
		case VUIDSFORMAT:

			/*
			 * VUIDSFORMAT is known to streamio.c and this
			 * is assume to be non-I_STR & non-TRANSPARENT ioctl
			 */

			if (iocbp->ioc_count == TRANSPARENT) {
				mp->b_datap->db_type = M_IOCNAK;
				iocbp->ioc_rval = -1;
				iocbp->ioc_error = EINVAL;
			} else {
				int format_type;

				format_type = *(int *)mp->b_cont->b_rptr;
				STATEP->format = (unchar) format_type;
				iocbp->ioc_rval = 0;
				iocbp->ioc_count = 0;
				iocbp->ioc_error = 0;
				mp->b_datap->db_type = M_IOCACK;
			}

			/* return buffer to pool ASAP */
			if (mp->b_cont) {
				freemsg(mp->b_cont);
				mp->b_cont = NULL;
			}

			qreply(qp, mp);
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

			if (iocbp->ioc_count == TRANSPARENT) {
				vuidmice_iocnack(qp, mp, iocbp, EINVAL, -1);
				return (0);
			}

			mp->b_cont = allocb(sizeof (int), BPRI_MED);
			if (mp->b_cont == NULL) {
				mp->b_datap->db_type = M_IOCNAK;
				iocbp->ioc_error = EAGAIN;
				qreply(qp, mp);
				return (0);
			}

			*(int *)mp->b_cont->b_rptr = (int)STATEP->format;
			mp->b_cont->b_wptr += sizeof (int);

			iocbp->ioc_count = sizeof (int);
			mp->b_datap->db_type = M_IOCACK;
			qreply(qp, mp);
			return (0);

		case VUID_NATIVE:
		case VUIDSADDR:
		case VUIDGADDR:
			vuidmice_iocnack(qp, mp, iocbp, ENOTTY, -1);
			return (0);

		case MSIOBUTTONS:
			/* return buffer to pool ASAP */
			if (mp->b_cont) {
				freemsg(mp->b_cont); /* over written below */
				mp->b_cont = NULL;
			}

			/*
			 * MSIOBUTTONS is known to streamio.c and this
			 * is assume to be non-I_STR & non-TRANSPARENT ioctl
			 */

			if (iocbp->ioc_count == TRANSPARENT) {
				vuidmice_iocnack(qp, mp, iocbp, EINVAL, -1);
				return (0);
			}

			if (STATEP->nbuttons == 0) {
				vuidmice_iocnack(qp, mp, iocbp, EINVAL, -1);
				return (0);
			}

			mp->b_cont = allocb(sizeof (int), BPRI_MED);
			if (mp->b_cont == NULL) {
				mp->b_datap->db_type = M_IOCNAK;
				iocbp->ioc_error = EAGAIN;
				qreply(qp, mp);
				return (0);
			}

			*(int *)mp->b_cont->b_rptr = (int)STATEP->nbuttons;
			mp->b_cont->b_wptr += sizeof (int);

			iocbp->ioc_count = sizeof (int);
			mp->b_datap->db_type = M_IOCACK;
			qreply(qp, mp);
			return (0);

		default:
			putnext(qp, mp);	/* nothing to process here */
			return (0);
		}
	}

	default:
		putnext(qp, mp);		/* pass it on */
		return (0);
	}
	/*NOTREACHED*/
}

static void
vuidmice_iocnack(queue_t *const qp, mblk_t *const mp,
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

void
VUID_PUTNEXT(queue_t *const qp, unchar event_id, unchar event_pair_type,
	unchar event_pair, int event_value)
{
	int strikes = 1;
	mblk_t *bp;
	Firm_event *fep;

	/*
	 * Give this event 3 chances to allocate blocks,
	 * otherwise discard this mouse event.  3 Strikes and you're out.
	 */
	while ((bp = allocb((int)sizeof (Firm_event), BPRI_HI)) == NULL) {
		if (++strikes > 3)
			return;
		drv_usecwait(10);
	}

	fep = (Firm_event *)bp->b_wptr;
	fep->id = vuid_id_addr(VKEY_FIRST) | vuid_id_offset(event_id);

	fep->pair_type	= event_pair_type;
	fep->pair	= event_pair;
	fep->value	= event_value;
	uniqtime32(&fep->time);
	bp->b_wptr += sizeof (Firm_event);

	if (canput(qp->q_next))
		putnext(qp, bp);
	else
		(void) putbq(qp, bp); /* read side is blocked */
}

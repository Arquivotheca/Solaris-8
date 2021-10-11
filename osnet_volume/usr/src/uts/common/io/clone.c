/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)clone.c	1.47	99/07/30 SMI" /* from S5R4 1.10 */

/*
 * Clone Driver.
 */

#include "sys/types.h"
#include "sys/param.h"
#include "sys/errno.h"
#include "sys/signal.h"
#include "sys/vfs.h"
#include "sys/vnode.h"
#include "sys/pcb.h"
#include "sys/user.h"
#include "sys/stropts.h"
#include "sys/stream.h"
#include "sys/errno.h"
#include "sys/sysinfo.h"
#include "sys/systm.h"
#include "sys/conf.h"
#include "sys/debug.h"
#include "sys/cred.h"
#include "sys/mkdev.h"
#include "sys/open.h"
#include "sys/strsubr.h"
#include "sys/ddi.h"

#include "sys/sunddi.h"

extern struct vnode *makespecvp(register dev_t dev, register vtype_t type);

int clnopen(queue_t *rq, dev_t *devp, int flag, int sflag, cred_t *crp);

static struct module_info clnm_info = { 0, "CLONE", 0, 0, 0, 0 };
static struct qinit clnrinit = { NULL, NULL, clnopen, NULL, NULL,
					&clnm_info, NULL };
static struct qinit clnwinit = { NULL, NULL, NULL, NULL, NULL,
					&clnm_info, NULL };

struct streamtab clninfo = { &clnrinit, &clnwinit };

/*
 * XXX: old conf.c entry had d_open filled plus a d_str filled in?
 * XXX: can't see why, thus it's not here.
 */

static int cln_info(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int cln_identify(dev_info_t *);
static int cln_attach(dev_info_t *, ddi_attach_cmd_t);
static dev_info_t *cln_dip;		/* private copy of devinfo pointer */

#define	CLONE_CONF_FLAG		(D_NEW|D_MP)
	DDI_DEFINE_STREAM_OPS(clone_ops, cln_identify, nulldev,	\
			cln_attach, nodev, nodev,		\
			cln_info, CLONE_CONF_FLAG, &clninfo);

#define	CBFLAG(maj)	(devopsp[(maj)]->devo_cb_ops->cb_flag)

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/modctl.h>


extern int nodev(void);
extern int nulldev(void);
extern int dseekneg_flag;
extern struct mod_ops mod_driverops;
extern struct dev_ops clone_ops;

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a pseudo driver */
	"Clone Pseudodriver 'clone'",
	&clone_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};


int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	/*
	 * Since the clone driver's reference count is unreliable,
	 * make sure we are never unloaded.
	 */
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
cln_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "clone") == 0)
		return (DDI_IDENTIFIED);
	return (DDI_NOT_IDENTIFIED);
}

/* ARGSUSED */
static int
cln_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	cln_dip = devi;
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
cln_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
	void **result)
{
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (cln_dip == NULL) {
			error = DDI_FAILURE;
		} else {
			*result = (void *)cln_dip;
			error = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*
 * Clone open.  Maj is the major device number of the streams
 * device to open.  Look up the device in the cdevsw[].  Attach
 * its qinit structures to the read and write queues and call its
 * open with the sflag set to CLONEOPEN.  Swap in a new vnode with
 * the real device number constructed from either
 *	a) for old-style drivers:
 *		maj and the minor returned by the device open, or
 *	b) for new-style drivers:
 *		the whole dev passed back as a reference parameter
 *		from the device open.
 */
int
clnopen(queue_t *rq, dev_t *devp, int flag, int sflag, cred_t *crp)
{
	struct streamtab *stp;
	dev_t newdev;
	int error = 0;
	major_t maj;
	minor_t emaj;
	int	safety;
	struct qinit *rinit, *winit;
	uint32_t qflag, sqtype;

	if (sflag)
		return (ENXIO);

	/*
	 * Get the device to open.
	 */
	emaj = getminor(*devp); /* minor is major for a cloned drivers */
	maj = etoimajor(emaj);	/* get internal major of cloned driver */

	if (maj >= devcnt)
		return (ENXIO);

	/*
	 * XXX: there is no corresponding release for clone driver opens.
	 */
	if (ddi_hold_installed_driver(maj) == NULL)
		return (ENXIO);

	if ((stp = STREAMSTAB(maj)) == NULL) {
		ddi_rele_driver(maj);
		return (ENXIO);
	}

	newdev = makedevice(emaj, 0);	/* create new style device number  */

	safety =  CBFLAG(maj);

	/*
	 * Save so that we can restore the q on failure.
	 */
	rinit = rq->q_qinfo;
	winit = WR(rq)->q_qinfo;
	ASSERT(rq->q_syncq->sq_type == (SQ_CI|SQ_CO));
	ASSERT((rq->q_flag & QMT_TYPEMASK) == QMTSAFE);

	error = devflg_to_qflag(stp, safety, &qflag, &sqtype);
	if (error)
		return (error);
	/*
	 * Set the syncq state what qattach started off with. This is safe
	 * since no other thread can access this queue at this point
	 * (stream open, close, push, and pop are single threaded
	 * by the framework.)
	 */
	leavesq(rq->q_syncq, SQ_OPENCLOSE);

	/*
	 * Substitute the real qinit values for the current ones.
	 */
	/* setq might sleep in kmem_alloc - avoid holding locks. */
	setq(rq, stp->st_rdinit, stp->st_wrinit, stp, &perdev_syncq[maj],
	    qflag, sqtype, 0);

	/*
	 * Open the attached module or driver.
	 *
	 * If there is an outer perimeter get exclusive access during
	 * the open procedure.
	 * Bump up the reference count on the queue.
	 */
	entersq(rq->q_syncq, SQ_OPENCLOSE);

	/*
	 * Call the device open with the stream flag CLONEOPEN.  The device
	 * will either fail this or return the device number.
	 */
	if (!(error = (*rq->q_qinfo->qi_qopen)(rq, &newdev, flag,
	    CLONEOPEN, crp))) {
		if ((getmajor(newdev) >= devcnt) ||
		    !(stp = STREAMSTAB(getmajor(newdev)))) {
			(*rq->q_qinfo->qi_qclose)(rq, flag, crp);
			error = ENXIO;
		} else {
			major_t m = getmajor(newdev);
			if (m != maj)  {
				(void) ddi_hold_installed_driver(m);
			}
			*devp = newdev;
		}
	}
	if (error) {
		/*
		 * open failed; pretty up to look like original
		 * queue.
		 */
		if (backq(WR(rq)) && backq(WR(rq))->q_next == WR(rq))
			qprocsoff(rq);
		leavesq(rq->q_syncq, SQ_OPENCLOSE);
		rq->q_next = WR(rq)->q_next = NULL;
		ASSERT(flush_syncq(rq->q_syncq, rq) == 0);
		ASSERT(flush_syncq(WR(rq)->q_syncq, WR(rq)) == 0);
		rq->q_ptr = WR(rq)->q_ptr = NULL;
		/* setq might sleep in kmem_alloc - avoid holding locks. */
		setq(rq, rinit, winit, NULL, NULL, QMTSAFE, SQ_CI|SQ_CO, 0);

		/* Restore back to what qattach will expect */
		entersq(rq->q_syncq, SQ_OPENCLOSE);

		ddi_rele_driver(maj);
	}
	return (error);
}

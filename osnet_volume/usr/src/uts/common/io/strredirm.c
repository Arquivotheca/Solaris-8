/*
 * Copyright (c) 1990,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strredirm.c	1.16	97/10/22 SMI"

/*
 * Redirection STREAMS module.
 *
 * This module is intended for use in conjunction with instantiations of the
 * redirection driver.  Its purpose in life is to detect when the stream that
 * it's pushed on is closed, thereupon calling back into the redirection
 * driver so that the driver can cancel redirection to the stream.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/kmem.h>

#include <sys/stream.h>
#include <sys/stropts.h>

#include <sys/debug.h>

#include <sys/strredir.h>
#include <sys/thread.h>


#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

/*
 * This is the loadable module wrapper.
 */
#include <sys/conf.h>
#include <sys/modctl.h>

extern struct streamtab	redirminfo;

static struct fmodsw fsw = {
	"redirmod",
	&redirminfo,
	D_NEW | D_MP
};

static struct modlstrmod modlstrmod = {
	&mod_strmodops,
	"redirection module",
	&fsw
};

/*
 * Module linkage information for the kernel.
 */
static struct modlinkage modlinkage = {
	MODREV_1, &modlstrmod, NULL
};

_init()
{
	return (mod_install(&modlinkage));
}
_fini()
{
	return (EBUSY);
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Forward declarations for private routines.
 */
static int	wcmopen(queue_t	*, dev_t *, int, int, cred_t *);
static int	wcmclose(queue_t *, int, cred_t *);
static int	wcmput(queue_t *, mblk_t *);

static struct module_info	wcminfo = {
	_STRREDIR_MODID,
	"redirmod",
	0,
	INFPSZ,
	5120,
	1024
};

static struct qinit	wcmrinit = {
	wcmput,		/* put */
	NULL,		/* service */
	wcmopen,	/* open */
	wcmclose,	/* close */
	NULL,		/* qadmin */
	&wcminfo,
	NULL		/* mstat */
};

static struct qinit	wcmwinit = {
	wcmput,		/* put */
	NULL,		/* service */
	wcmopen,	/* open */
	wcmclose,	/* close */
	NULL,		/* qadmin */
	&wcminfo,
	NULL		/* mstat */
};

struct streamtab	redirminfo = {
	&wcmrinit,
	&wcmwinit,
	NULL,
	NULL
};

/* ARGSUSED */
static int
wcmopen(queue_t *q, dev_t *dev, int flag, int sflag, cred_t *cred)
{
	extern kthread_t	*iwscn_thread;
	extern wcm_data_t	*iwscn_wcm_data;

	if (sflag != MODOPEN)
		return (EINVAL);

	/*
	 * There's nothing to do if we're already open.
	 */
	if (q->q_ptr == NULL) {
		/*
		 * Attach the per open instance state structure.
		 * Its fields were * initialized elsewhere (from the
		 * SRIOCSREDIR case of of the redirection driver's ioctl
		 * routine).
		 * To prevent other threads from getting this, check thread_id.
		 */
		if (curthread != iwscn_thread)
			return (EINVAL);
		q->q_ptr = WR(q)->q_ptr = iwscn_wcm_data;
	}
	qprocson(q);
	return (0);
}

/* ARGSUSED */
static int
wcmclose(queue_t *q, int flag, cred_t *cred)
{
	wcm_data_t	*mdp = (wcm_data_t *)q->q_ptr;

	qprocsoff(q);
	srpop(mdp, flag, cred);
	WR(q)->q_ptr = q->q_ptr = NULL;
	kmem_free(mdp, sizeof (*mdp));

	return (0);
}

/*
 * This module's only purpose in life is to intercept closes on the stream
 * it's pushed on.  It passes all messages on unchanged, in both directions.
 */
static int
wcmput(queue_t *q, mblk_t *mp)
{
	putnext(q, mp);
	return (0);
}

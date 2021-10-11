/*
 * Copyright (c) 1990-1992,1996,1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)gentty.c	1.31	99/12/17 SMI"
					/* from S5R4 1.22 */

/*
 * Indirect driver for controlling tty.
 */
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/session.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/stat.h>
#include <sys/sunddi.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/modctl.h>


#define	IS_STREAM(dev) (devopsp[getmajor(dev)]->devo_cb_ops->cb_str != NULL)

int syopen(dev_t *, int, int, cred_t *);
int syread(dev_t, struct uio *, cred_t *);
int sywrite(dev_t, struct uio *, cred_t *);
int sypoll(dev_t, short, int, short *, struct pollhead **);
int syioctl(dev_t, int, intptr_t, int, cred_t *, int *);

static int sy_info(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int sy_identify(dev_info_t *);
static int sy_attach(dev_info_t *, ddi_attach_cmd_t);
static dev_info_t *sy_dip;		/* private copy of devinfo pointer */

struct cb_ops	sy_cb_ops = {

	syopen,			/* open */
	nulldev,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	syread,			/* read */
	sywrite,		/* write */
	syioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev, 			/* segmap */
	sypoll,			/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */

};

struct dev_ops	sy_ops = {

	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	sy_info,		/* info */
	sy_identify,		/* identify */
	nulldev,		/* probe */
	sy_attach,		/* attach */
	nodev,			/* detach */
	nodev,			/* reset */
	&sy_cb_ops,		/* driver operations */
	(struct bus_ops *)0	/* bus operations */

};


extern int nodev(void);
extern int nulldev(void);
extern int dseekneg_flag;
extern struct mod_ops mod_driverops;
extern struct dev_ops sy_ops;

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a pseudo driver */
	"Indirect driver for tty 'sy'",
	&sy_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
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
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
sy_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "sy") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

/* ARGSUSED */
static int
sy_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (ddi_create_minor_node(devi, "tty", S_IFCHR,
	    0, NULL, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (-1);
	}
	sy_dip = devi;
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
sy_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	dev_t dev = (dev_t)arg;
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (sy_dip == NULL) {
			*result = (void *)NULL;
			error = DDI_FAILURE;
		} else {
			*result = (void *) sy_dip;
			error = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		if (getminor(dev) != 0) {
			*result = (void *)-1;
			error = DDI_FAILURE;
		} else {
			*result = (void *)0;
			error = DDI_SUCCESS;
		}
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}


/* ARGSUSED */
int
syopen(dev_t *devp, int flag, int otyp, struct cred *cr)
{
	dev_t ttyd;
	vnode_t *ttyvp;
	sess_t	*sp = curproc->p_sessp;
	int	error;

	if ((ttyd = sp->s_dev) == NODEV)
		return (ENXIO);
	TTY_HOLD(sp);
	if ((ttyvp = sp->s_vp) == NULL) {
		TTY_RELE(sp);
		return (EIO);
	}
	/*
	 * XXX: Clone opens are broken...
	 * We don't have to hold the underlying driver, because
	 * the underlying driver will make itself the controlling terminal
	 * and the hold is being done in makectty().
	 */
	(void) ddi_install_driver(ddi_major_to_name(getmajor(ttyd)));
	/*
	 * By setting sflag to CONSOPEN, we allow opens of this device
	 * to succeed even if it points to the console device.
	 * See the comment in stropen().
	 */
	if (IS_STREAM(ttyd))
		error = stropen(ttyvp, &ttyd, flag, CONSOPEN, cr);
	else
		error = dev_open(&ttyd, flag, otyp, cr);
	TTY_RELE(sp);
	return (error);
}

/* ARGSUSED */
int
syclose(dev_t dev, int flag, int otyp, struct cred *cr)
{
	return (0);
}

/* ARGSUSED */
int
syread(dev_t dev, struct uio *uiop, struct cred *cr)
{
	dev_t ttyd;
	vnode_t *ttyvp;
	sess_t	*sp = curproc->p_sessp;
	int	error;

	if ((ttyd = sp->s_dev) == NODEV)
		return (ENXIO);
	TTY_HOLD(sp);
	if ((ttyvp = sp->s_vp) == NULL) {
		TTY_RELE(sp);
		return (EIO);
	}
	if (IS_STREAM(ttyd))
		error = strread(ttyvp, uiop, cr);
	else
		error = cdev_read(ttyd, uiop, cr);
	TTY_RELE(sp);
	return (error);

}

/* ARGSUSED */
int
sywrite(dev_t dev, struct uio *uiop, struct cred *cr)
{
	dev_t ttyd;
	vnode_t *ttyvp;
	sess_t	*sp = curproc->p_sessp;
	int	error;

	if ((ttyd = sp->s_dev) == NODEV)
		return (ENXIO);
	TTY_HOLD(sp);
	if ((ttyvp = sp->s_vp) == NULL) {
		TTY_RELE(sp);
		return (EIO);
	}
	if (IS_STREAM(ttyd))
		error = strwrite(ttyvp, uiop, cr);
	else
		error = cdev_write(ttyd, uiop, cr);
	TTY_RELE(sp);
	return (error);
}


/* ARGSUSED */
int
syioctl(dev_t dev, int cmd, intptr_t arg, int mode, struct cred *cr,
	int *rvalp)
{
	dev_t ttyd;
	vnode_t *ttyvp;
	sess_t	*sp = curproc->p_sessp;
	int	error;

	if ((ttyd = sp->s_dev) == NODEV)
		return (ENXIO);
	TTY_HOLD(sp);
	if ((ttyvp = sp->s_vp) == NULL) {
		TTY_RELE(sp);
		return (EIO);
	}
	if (IS_STREAM(ttyd))
		error = strioctl(ttyvp, cmd, arg, mode, U_TO_K, cr, rvalp);
	else
		error = cdev_ioctl(ttyd, cmd, arg, mode, cr, rvalp);
	TTY_RELE(sp);
	return (error);
}



/* ARGSUSED */
int
sypoll(dev_t dev, short events, int anyyet, short *reventsp,
	struct pollhead **phpp)
{
	dev_t ttyd;
	vnode_t *ttyvp;
	sess_t  *sp = curproc->p_sessp;
	int	error;

	if ((ttyd = sp->s_dev) == NODEV)
		return (ENXIO);
	TTY_HOLD(sp);
	if ((ttyvp = sp->s_vp) == NULL) {
		TTY_RELE(sp);
		return (EIO);
	}
	if (IS_STREAM(ttyd))
		error = strpoll(ttyvp->v_stream, events, anyyet, reventsp,
				phpp);
	else
		error = cdev_poll(ttyd, events, anyyet, reventsp, phpp);
	TTY_RELE(sp);
	return (error);
}

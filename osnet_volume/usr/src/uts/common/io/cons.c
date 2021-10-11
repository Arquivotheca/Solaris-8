/*
 * Copyright (c) 1987-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cons.c	5.54	99/05/04 SMI"

/*
 * Indirect console driver for Sun.
 *
 * Redirects all I/O to the device designated as the underlying "hardware"
 * console, as given by the value of rconsvp.  The implementation assumes that
 * rconsvp denotes a STREAMS device; the assumption is justified since
 * consoles must be capable of effecting tty semantics.
 *
 * rconsvp is set in autoconf.c:consconfig(), based on information obtained
 * from the EEPROM.
 *
 * XXX:	The driver still needs to be converted to use ANSI C consistently
 *	throughout.
 */

#include <sys/types.h>
#include <sys/open.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/file.h>
#include <sys/taskq.h>
#include <sys/log.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/stat.h>

#include <sys/console.h>
#include <sys/consdev.h>

#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/poll.h>

#include <sys/debug.h>

#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

static int cnopen(dev_t *, int, int, struct cred *);
static int cnclose(dev_t, int, int, struct cred *);
static int cnread(dev_t, struct uio *, struct cred *);
static int cnwrite(dev_t, struct uio *, struct cred *);
static int cnioctl(dev_t, int, intptr_t, int, struct cred *, int *);
static int cnpoll(dev_t, short, int, short *, struct pollhead **);
static int cn_info(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int cn_identify(dev_info_t *);
static int cn_attach(dev_info_t *, ddi_attach_cmd_t);
static int cn_detach(dev_info_t *, ddi_detach_cmd_t);

static dev_info_t *cn_dip;		/* private copy of devinfo pointer */

static struct cb_ops cn_cb_ops = {

	cnopen,			/* open */
	cnclose,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	cnread,			/* read */
	cnwrite,		/* write */
	cnioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev, 			/* segmap */
	cnpoll,			/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */

};

static struct dev_ops cn_ops = {

	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	cn_info,		/* info */
	cn_identify,		/* identify */
	nulldev,		/* probe */
	cn_attach,		/* attach */
	cn_detach,		/* detach */
	nodev,			/* reset */
	&cn_cb_ops,		/* driver operations */
	(struct bus_ops *)0	/* bus operations */

};

/*
 * Global variables associated with the console device:
 *
 * XXX:	There are too many of these!
 * moved to space.c to becone resident in the kernel so that cons
 * can be loadable.
 */

extern dev_t	rconsdev;	/* "hardware" console */
extern vnode_t	*rconsvp;	/* pointer to vnode for that device */

/*
 * XXX: consulted in prsubr.c, for /proc entry point for obtaining ps info.
 */
extern dev_t	uconsdev;	/* What the user thinks is the console device */

/*
 * Private driver state:
 */

/*
 * The underlying console device potentially can be opened through (at least)
 * two paths: through this driver and through the underlying device's driver.
 * To ensure that reference counts are meaningful and therefore that close
 * routines are called at the right time, it's important to make sure that
 * rconsvp's s_count field (i.e., the count on the underlying device) never
 * has a contribution of more than one through this driver, regardless of how
 * many times this driver's been opened.  rconsopen keeps track of the
 * necessary information to ensure this property.
 */
static uint_t	rconsopen;


#include <sys/types.h>
#include <sys/conf.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/modctl.h>


extern int nodev(), nulldev();
extern int dseekneg_flag;
extern struct mod_ops mod_driverops;
extern struct dev_ops cn_ops;

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a pseudo driver */
	"Console redirection driver",
	&cn_ops,	/* driver ops */
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
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * DDI glue routines
 */
static int
cn_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "cn") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

static int
cn_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	if (ddi_create_minor_node(devi, "syscon", S_IFCHR,
	    0, NULL, NULL) == DDI_FAILURE) {
		return (DDI_FAILURE);
	}
	if (ddi_create_minor_node(devi, "systty", S_IFCHR,
	    0, NULL, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	if (ddi_create_minor_node(devi, "console", S_IFCHR,
	    0, NULL, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	cn_dip = devi;
	return (DDI_SUCCESS);
}

static int
cn_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);
	ddi_remove_minor_node(devi, NULL);
	uconsdev = NODEV;
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
cn_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error = DDI_FAILURE;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (getminor((dev_t)arg) == 0 && cn_dip != NULL) {
			*result = (void *) cn_dip;
			error = DDI_SUCCESS;
		}
		break;

	case DDI_INFO_DEVT2INSTANCE:
		if (getminor((dev_t)arg) == 0) {
			*result = (void *)0;
			error = DDI_SUCCESS;
		}
		break;

	default:
		break;
	}

	return (error);
}

/*
 * XXX	Caution: before allowing more than 256 minor devices on the
 *	console, make sure you understand the 'compatibility' hack
 *	in ufs_iget() that translates old dev_t's to new dev_t's.
 *	See bugid 1098104 for the sordid details.
 */

/* ARGSUSED */
static int
cnopen(dev_t *dev, int flag, int state, struct cred *cred)
{
	int	err = 0;
	static int	been_here;
	vnode_t	*vp = rconsvp;

	ASSERT(cred != NULL);

	if (rconsvp == NULL)
		return (0);


	/*
	 * XXX: Clean up inactive PIDs from previous opens if any.
	 * These would have been created as a result of an I_SETSIG
	 * issued against console.  This is a workaround, and
	 * console driver must be correctly redesigned not to need
	 * this hook.
	 */
	if (vp->v_stream) {
		str_cn_clean(vp);
	}

	/*
	 * XXX:	Set hook to tell /proc about underlying console.  (There's
	 *	gotta be a better way...)
	 */
	if (state != OTYP_CHR || getminor(*dev) != 0)
		return (ENXIO);
	if (been_here == 0) {
		uconsdev = *dev;
		been_here = 1;
		if (vn_open("/dev/console", UIO_SYSSPACE, FWRITE | FNOCTTY,
		    0, &console_vnode, 0, 0) == 0)
			console_taskq = taskq_create("console_taskq",
			    1, maxclsyspri - 1, LOG_LOWAT / LOG_MSGSIZE,
			    LOG_HIWAT / LOG_MSGSIZE, TASKQ_PREPOPULATE);
	}

	err = VOP_OPEN(&vp, flag, cred);
	/*
	 * The underlying driver is not allowed to have cloned itself
	 * for this open.
	 */
	if (vp != rconsvp)
		cmn_err(CE_PANIC, "cnopen: cloned open");

	if (!err)
		rconsopen++;

	return (err);
}

/* ARGSUSED */
static int
cnclose(dev_t dev, int flag, int state, struct cred *cred)
{
	int	err = 0;
	vnode_t	*vp;

	/*
	 * Since this is the _last_ close, it's our last chance to close the
	 * underlying device.  (Note that if someone else has the underlying
	 * hardware console device open, we won't get here, since spec_close
	 * will see s_count > 1.)
	 */
	if (state != OTYP_CHR)
		return (ENXIO);

	if (rconsvp == NULL)
		return (0);

	while ((rconsopen != 0) && ((vp = rconsvp) != NULL)) {
		err = VOP_CLOSE(vp, flag, 1, (offset_t)0, cred);
		if (!err) {
			vp->v_stream = NULL;
			rconsopen--;
		}
	}
	return (err);
}

/* ARGSUSED */
static int
cnread(dev_t dev, struct uio *uio, struct cred *cred)
{
	kcondvar_t	sleep_forever;
	kmutex_t	sleep_forever_mutex;

	if (rconsvp == NULL) {
		/*
		 * Go to sleep forever.  This seems like the least
		 * harmful thing to do if there's no console.
		 * EOF might be better if we're ending up single-user
		 * mode.
		 */
		cv_init(&sleep_forever, NULL, CV_DRIVER, NULL);
		mutex_init(&sleep_forever_mutex, NULL, MUTEX_DRIVER, NULL);
		mutex_enter(&sleep_forever_mutex);
		(void) cv_wait_sig(&sleep_forever, &sleep_forever_mutex);
		mutex_exit(&sleep_forever_mutex);
		return (EIO);
	}

	if (rconsvp->v_stream != NULL)
		return (strread(rconsvp, uio, cred));
	else
		return (cdev_read(rconsdev, uio, cred));
}

/* ARGSUSED */
static int
cnwrite(dev_t dev, struct uio *uio, struct cred *cred)
{
	if (rconsvp == NULL) {
		uio->uio_resid = 0;
		return (0);
	}

	if (rconsvp->v_stream != NULL)
		return (strwrite(rconsvp, uio, cred));
	else
		return (cdev_write(rconsdev, uio, cred));
}

/* ARGSUSED */
static int
cnioctl(dev_t dev, int cmd, intptr_t arg, int flag, struct cred *cred,
	int *rvalp)
{
	if (rconsvp == NULL)
		return (0);

	if (rconsvp->v_stream != NULL)
		return (strioctl(rconsvp, cmd, arg, flag, U_TO_K, cred,
		    rvalp));
	else
		return (cdev_ioctl(rconsdev, cmd, arg, flag, cred, rvalp));
}

/* ARGSUSED */
static int
cnpoll(dev_t dev, short events, int anyyet, short *reventsp,
	struct pollhead **phpp)
{
	if (rconsvp == NULL)
		return (nochpoll(dev, events, anyyet, reventsp, phpp));

	if (rconsvp->v_stream != NULL)
		return (strpoll(rconsvp->v_stream, events, anyyet, reventsp,
		    phpp));
	else
		return (cdev_poll(rconsdev, events, anyyet, reventsp, phpp));
}

/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)driver.c	1.46	99/09/23 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/fs/snode.h>
#include <sys/open.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/tnf_probe.h>

/* Don't #include <sys/ddi.h> - it #undef's getmajor() */

#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/sunpm.h>
#include <sys/ddi_impldefs.h>
#include <sys/ndi_impldefs.h>
#include <sys/esunddi.h>
#include <sys/autoconf.h>
#include <sys/modctl.h>
#include <sys/epm.h>
#include <sys/dacf.h>

static void i_devi_postattach(dev_info_t *, int);
static int i_devi_predetach(dev_info_t *, ddi_detach_cmd_t);
static void i_devi_postdetach(dev_info_t *, int);
static void i_attach_ctlop(dev_info_t *, ddi_attach_cmd_t, ddi_pre_post_t, int);
static void i_detach_ctlop(dev_info_t *, ddi_detach_cmd_t, ddi_pre_post_t, int);

/*
 * Configuration-related entry points for nexus and leaf drivers
 */
int
devi_identify(dev_info_t *devi)
{
	struct dev_ops *ops;
	int (*fn)(dev_info_t *);

	if ((ops = ddi_get_driver(devi)) == NULL ||
	    (fn = ops->devo_identify) == NULL)
		return (DDI_NOT_IDENTIFIED);

	return ((*fn)(devi));
}

int
devi_probe(dev_info_t *devi)
{
	struct dev_ops *ops;
	int (*fn)(dev_info_t *);
	dev_info_t *pdevi;
	pm_info_t *pinfo;

	ops = ddi_get_driver(devi);
	ASSERT(ops);

	/*
	 * probe(9E) in 2.0 implies that you can get
	 * away with not writing one of these .. so we
	 * pretend we're 'nulldev' if we don't find one (sigh).
	 */
	if ((fn = ops->devo_probe) == NULL)
		return (DDI_PROBE_DONTCARE);

	/*
	 * If parent is power manageable, make sure it is powered up for the
	 * probe, and allow it to be powered down afterwards.
	 */
	if ((pdevi = ddi_get_parent(devi)) != NULL &&
	    (pinfo = PM_GET_PM_INFO(pdevi)) != NULL) {
		void pm_pre_probe(dev_info_t *pdip, pm_info_t *pinfo);
		void pm_post_probe(dev_info_t *pdip, pm_info_t *pinfo);
		int retval;

		pm_pre_probe(pdevi, pinfo);
		retval = (*fn)(devi);
		pm_post_probe(pdevi, pinfo);
		return (retval);
	} else {
		return ((*fn)(devi));
	}
}

/*
 * i_devi_postattach()
 * 	do post-attach operations for the given device instance
 */
static void
i_devi_postattach(dev_info_t *devi, int error)
{

	ASSERT(MUTEX_HELD(&dacf_lock));

	if (error == DDI_FAILURE) {
		/*
		 * The attach failed, so clean up dacf reservations
		 */
		dacf_clr_rsrvs(devi, DACF_OPID_POSTATTACH);
		dacf_clr_rsrvs(devi, DACF_OPID_PREDETACH);
	} else {
		/*
		 * Attach succeeded, so proceed to doing post-attach tasks
		 */
		e_pm_props(devi);		/* Get power mgmt props */
		if (PM_GET_PM_INFO(devi) == NULL)
			(void) pm_start(devi);
		(void) dacfc_postattach(devi);	/* dacf postattach operations */
	}
}

/*
 * devi_attach()
 * 	attach a device instance to the system if the driver supplies an
 * 	attach(9E) entrypoint.  Check to see if	cmd is a power management
 * 	subcommand, and if so, simply call the driver's attach routine with
 * 	that command.
 *
 * 	If we're actually attaching, proceed in two stages:
 * 		1. attach	- call the driver's attach(9E) entrypoint
 * 		2. post-attach	- call routines which setup power management,
 *				  and/or configure the device into the kernel.
 */
int
devi_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	struct dev_ops *ops;
	int error;
	int (*fn)(dev_info_t *, ddi_attach_cmd_t);

	if ((cmd == DDI_RESUME || cmd == DDI_PM_RESUME) &&
	    e_ddi_parental_suspend_resume(devi)) {
		return (e_ddi_resume(devi, cmd));
	}
	if ((ops = ddi_get_driver(devi)) == NULL ||
	    (fn = ops->devo_attach) == NULL)
		return (DDI_FAILURE);

	/*
	 * The easy case: this is doing power mgmt stuff and can just call
	 * through fn and return.
	 */
	if ((cmd == DDI_RESUME) || (cmd == DDI_PM_RESUME)) {
		i_attach_ctlop(devi, cmd, DDI_PRE, 0);
		error = (*fn)(devi, cmd);
		i_attach_ctlop(devi, cmd, DDI_POST, error);
		return (error);
	}

	/*
	 * 1. Call the driver's attach(9e) entrypoint
	 */
	NDI_CONFIG_DEBUG((CE_CONT, "devi_attach: %s%d (%p)\n",
	    ddi_driver_name(devi), ddi_get_instance(devi), (void *)devi));

	DEVI_SET_ATTACHING(devi);

	i_attach_ctlop(devi, cmd, DDI_PRE, 0);
	error = (*fn)(devi, cmd);
	i_attach_ctlop(devi, cmd, DDI_POST, error);

	DEVI_CLR_ATTACHING(devi);

	/*
	 * 2. Do various post-attach tasks, depending on the success or
	 * failure of the driver's attach processing.
	 */
	mutex_enter(&dacf_lock);
	i_devi_postattach(devi, error);

	mutex_exit(&dacf_lock);

	return (error);
}

/*
 * i_devi_predetach()
 * 	do operations in preparation for detach.
 */
static int
i_devi_predetach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int r = DDI_SUCCESS;
	ASSERT(cmd == DDI_DETACH || cmd == DDI_HOTPLUG_DETACH);

	/*
	 * If the command was DDI_HOTPLUG_DETACH, we've already tried to do
	 * the predetach auto-unconfigure.  So don't try again.
	 */
	if (cmd == DDI_DETACH) {
		mutex_enter(&dacf_lock);
		r = dacfc_predetach(devi);
		mutex_exit(&dacf_lock);
	}

	return (r);
}

/*
 * i_devi_postdetach()
 * 	cleanup after a detach has completed, either successfully or
 * 	unsuccessfully
 */
static void
i_devi_postdetach(dev_info_t *devi, int error)
{
	if (error == DDI_SUCCESS) {
		/*
		 * Success.  clean up autoconfiguration baggage.
		 */
		mutex_enter(&dacf_lock);
		dacf_clr_rsrvs(devi, DACF_OPID_POSTATTACH);
		dacf_clr_rsrvs(devi, DACF_OPID_PREDETACH);
		mutex_exit(&dacf_lock);
	} else {
		char *pathp, *path;
		path = kmem_alloc(MAXPATHLEN, KM_SLEEP);

		/*
		 * Try to re-autoconfigure the instance, by re-running its
		 * postattach dacf operations.  If all goes well, it returns
		 * as a first-class member of the system.  If this fails,
		 * the situation is dangerous: emit a warning.
		 */
		mutex_enter(&dacf_lock);
		if (dacfc_postattach(devi) != DDI_SUCCESS) {
			if ((pathp = ddi_pathname(devi, path)) == NULL)
				pathp = "<unknown>";
			cmn_err(CE_WARN, "%s failed to detach, and could "
			    "not be re-autoconfigured.", pathp);
		}
		mutex_exit(&dacf_lock);
		kmem_free(path, MAXPATHLEN);
	}
}

/*
 * devi_detach()
 * 	detach a device instance from the system if the driver supplies a
 * 	detach(9E) entrypoint.  Check to see if	cmd is a power management
 * 	subcommand, and if so, simply call the driver's detach routine with
 * 	that command.
 *
 * 	If we're actually detaching, proceed in three steps:
 *		1. pre-detach	- unconfigure the device from the system
 *		2. detach	- call the driver's detach(9E) entrypoint
 *		3. post-detach	- any additional cleanup, or error handling
 *				  if step 2 fails.
 */
int
devi_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	struct dev_ops *ops;
	int error;
	int (*fn)(dev_info_t *, ddi_detach_cmd_t);

	ASSERT(cmd == DDI_SUSPEND || cmd == DDI_PM_SUSPEND ||
	    cmd == DDI_DETACH || cmd == DDI_HOTPLUG_DETACH);

	if ((cmd == DDI_SUSPEND || cmd == DDI_PM_SUSPEND) &&
	    e_ddi_parental_suspend_resume(devi)) {
		return (e_ddi_suspend(devi, cmd));
	}
	if ((ops = ddi_get_driver(devi)) == NULL ||
	    (fn = ops->devo_detach) == NULL)
		return (DDI_FAILURE);

	/*
	 * This is a power mgmt command.  Just call through fn and return.
	 */
	if (cmd == DDI_SUSPEND || cmd == DDI_PM_SUSPEND) {
		i_detach_ctlop(devi, cmd, DDI_PRE, 0);
		error = (*fn)(devi, cmd);
		i_detach_ctlop(devi, cmd, DDI_POST, error);
		return (error);
	}

	NDI_CONFIG_DEBUG((CE_CONT, "devi_detach: %s%d (%p)\n",
	    ddi_driver_name(devi), ddi_get_instance(devi), (void *)devi));

	/*
	 * 1. Do pre-detach operations
	 */

	if (i_devi_predetach(devi, cmd) != 0) {
		return (DDI_FAILURE);
	}

	/*
	 * 2. Call the driver's detach routine
	 */
	DEVI_SET_DETACHING(devi);
	pm_detaching(devi);			/* suspend pm while detaching */

	i_detach_ctlop(devi, cmd, DDI_PRE, 0);

	error = (*fn)(devi, DDI_DETACH);

	i_detach_ctlop(devi, cmd, DDI_POST, error);
	if (error == DDI_SUCCESS)
		(void) pm_stop(devi);		/* make it permanent */
	else
		pm_detach_failed(devi);		/* resume power management */

	DEVI_CLR_DETACHING(devi);

	/*
	 * 3. Do post-detach cleanup, or re-autoconfiguration, if the detach
	 * was unsuccessful.
	 */
	i_devi_postdetach(devi, error);

	return (error);
}

static void
i_attach_ctlop(dev_info_t *devi, ddi_attach_cmd_t cmd, ddi_pre_post_t w,
    int ret)
{
	int error;
	struct attachspec as;
	dev_info_t *pdip = ddi_get_parent(devi);

	as.cmd = cmd;
	as.when = w;
	as.pdip = pdip;
	as.result = ret;
	(void) ddi_ctlops(devi, devi, DDI_CTLOPS_ATTACH, &as, &error);
}

static void
i_detach_ctlop(dev_info_t *devi, ddi_detach_cmd_t cmd, ddi_pre_post_t w,
    int ret)
{
	int error;
	struct detachspec ds;
	dev_info_t *pdip = ddi_get_parent(devi);

	ds.cmd = cmd;
	ds.when = w;
	ds.pdip = pdip;
	ds.result = ret;
	(void) ddi_ctlops(devi, devi, DDI_CTLOPS_DETACH, &ds, &error);
}

/*
 * This entry point not defined by Solaris 2.0 DDI/DKI, so
 * its inclusion here is somewhat moot.
 */
int
devi_reset(dev_info_t *devi, ddi_reset_cmd_t cmd)
{
	struct dev_ops *ops;
	int (*fn)(dev_info_t *, ddi_reset_cmd_t);

	if ((ops = ddi_get_driver(devi)) == NULL ||
	    (fn = ops->devo_reset) == NULL)
		return (DDI_FAILURE);

	return ((*fn)(devi, cmd));
}

/*
 * Leaf driver entry points
 */
int
dev_open(dev_t *devp, int flag, int type, struct cred *cred)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(*devp)]->devo_cb_ops;
	return ((*cb->cb_open)(devp, flag, type, cred));
}

/*
 * The target driver is held (referenced) until we are
 * done. (See spec_close).
 */
int
dev_close(dev_t dev, int flag, int type, struct cred *cred)
{
	struct cb_ops	*cb;

	cb = (devopsp[getmajor(dev)])->devo_cb_ops;
	return ((*cb->cb_close)(dev, flag, type, cred));
}

/*
 * We only attempt to find the devinfo node if the driver is *already*
 * in memory and has attached.  If it isn't in memory, we don't load it,
 * - that's left to open(2).  If it is in memory, but not attached,
 * we don't attach it, we just fail.
 *
 * If it is attached, we return the dev_info node and increment the
 * devops so that it won't disappear while we're looking at it..
 */
/*ARGSUSED1*/
dev_info_t *
dev_get_dev_info(dev_t dev, int otyp)
{
	struct dev_ops	*ops;
	dev_info_t	*dip;
	int error;
	major_t major = getmajor(dev);
	struct devnames *dnp;

	if (major >= devcnt)
		return (NULL);

	dnp = &(devnamesp[major]);
	LOCK_DEV_OPS(&(dnp->dn_lock));
	ops = devopsp[major];
	if (ops == NULL || ops->devo_getinfo == NULL) {
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		return (NULL);
	}

	if ((!CB_DRV_INSTALLED(ops)) || (ops->devo_getinfo == NULL) ||
	    ((dnp->dn_flags & DN_DEVS_ATTACHED) == 0))  {
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		return (NULL);
	}

	INCR_DEV_OPS_REF(ops);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	error = (*ops->devo_getinfo)(NULL, DDI_INFO_DEVT2DEVINFO,
	    (void *)dev, (void **)&dip);

	if (error != DDI_SUCCESS || dip == NULL) {
		LOCK_DEV_OPS(&(dnp->dn_lock));
		DECR_DEV_OPS_REF(ops);
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		return (NULL);
	}

	return (dip);	/* with one hold outstanding .. */
}

/*
 * The following function does not load the driver if it's not loaded.
 * Returns DDI_FAILURE or the instance number of the given dev_t as
 * interpreted by the device driver.
 *
 * instance is supposed to be a int but drivers have assumed that
 * the pointer was a pointer to "void *" instead of a pointer to
 * "int *" so we now explicitly pass a pointer to "void *" and then
 * cast the result to an int when returning the value.
 */
int
dev_to_instance(dev_t dev)
{
	struct dev_ops	*ops;
	void *instance;
	int error;
	major_t major = getmajor(dev);
	struct devnames *dnp = &(devnamesp[major]);

	if (major >= devcnt)
		return (DDI_FAILURE);

	LOCK_DEV_OPS(&(dnp->dn_lock));
	ops = devopsp[major];
	if (ops == NULL || ops->devo_getinfo == NULL) {
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		return (DDI_FAILURE);
	}

	if ((!CB_DRV_INSTALLED(ops)) || (ops->devo_getinfo == NULL) ||
	    ((dnp->dn_flags & DN_DEVS_ATTACHED) == 0))  {
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		return (DDI_FAILURE);
	}

	INCR_DEV_OPS_REF(ops);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	error = (*ops->devo_getinfo)(NULL, DDI_INFO_DEVT2INSTANCE,
	    (void *)dev, &instance);

	LOCK_DEV_OPS(&(dnp->dn_lock));
	DECR_DEV_OPS_REF(ops);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	if (error != DDI_SUCCESS)
		return (DDI_FAILURE);
	return ((int)instance);
}

/*
 * bdev_strategy should really be a 'void' function, since
 * the driver's strategy doesn't return anything meaningful
 * (it returns errors and such through the buf(9S) structure).
 * However, this breaks some unbundled products that look at
 * the return value (which they shouldn't).
 */
int
bdev_strategy(struct buf *bp)
{
	struct cb_ops	*cb;

	/* Kernel probe */
	TNF_PROBE_5(strategy, "io blockio", /* CSTYLED */,
		tnf_device,	device,		bp->b_edev,
		tnf_diskaddr,	block,		bp->b_lblkno,
		tnf_size,	size,		bp->b_bcount,
		tnf_opaque,	buf,		bp,
		tnf_bioflags,	flags,		bp->b_flags);

	cb = devopsp[getmajor(bp->b_edev)]->devo_cb_ops;
	(void) (*cb->cb_strategy)(bp);
	return (0);
}

int
bdev_print(dev_t dev, caddr_t str)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_print)(dev, str));
}

int
bdev_size(dev_t dev)
{
	return (e_ddi_getprop(dev, VBLK, "nblocks",
	    DDI_PROP_NOTPROM | DDI_PROP_DONTPASS, -1));
}

/*
 * Same for 64-bit Nblocks property
 */
uint64_t
bdev_Size(dev_t dev)
{
	return (e_ddi_getprop_int64(dev, VBLK, "Nblocks",
	    DDI_PROP_NOTPROM | DDI_PROP_DONTPASS, -1));
}

int
bdev_dump(dev_t dev, caddr_t addr, daddr_t blkno, int blkcnt)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_dump)(dev, addr, blkno, blkcnt));
}

int
cdev_read(dev_t dev, struct uio *uiop, struct cred *cred)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_read)(dev, uiop, cred));
}

int
cdev_write(dev_t dev, struct uio *uiop, struct cred *cred)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_write)(dev, uiop, cred));
}

int
cdev_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, struct cred *cred,
    int *rvalp)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_ioctl)(dev, cmd, arg, mode, cred, rvalp));
}

int
cdev_devmap(dev_t dev, devmap_cookie_t dhp, offset_t off, size_t len,
	size_t *maplen, uint_t mode)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_devmap)(dev, dhp, off, len, maplen, mode));
}

int
cdev_mmap(int (*mapfunc)(dev_t, off_t, int), dev_t dev, off_t off, int prot)
{
	return ((*mapfunc)(dev, off, prot));
}

int
cdev_segmap(dev_t dev, off_t off, struct as *as, caddr_t *addrp, off_t len,
	    uint_t prot, uint_t maxprot, uint_t flags, cred_t *credp)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_segmap)(dev, off, as, addrp,
	    len, prot, maxprot, flags, credp));
}

int
cdev_poll(dev_t dev, short events, int anyyet, short *reventsp,
	struct pollhead **pollhdrp)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_chpoll)(dev, events, anyyet, reventsp, pollhdrp));
}

/*
 * A 'size' property can be provided by a VCHR device.
 *
 * Since it's defined as zero for STREAMS devices, so we avoid the
 * overhead of looking it up.  Note also that we don't force an
 * unused driver into memory simply to ask about it's size.  We also
 * don't bother to ask it its size unless it's already been attached
 * (the attach routine is the earliest place the property'll be created)
 *
 * XXX	In an ideal world, we'd call this at VOP_GETATTR() time.
 */
int
cdev_size(dev_t dev)
{
	major_t maj;
	struct devnames *dnp;

	if ((maj = getmajor(dev)) >= devcnt)
		return (0);

	dnp = &(devnamesp[maj]);
	if ((dnp->dn_flags & DN_DEVS_ATTACHED) && devopsp[maj]) {
		LOCK_DEV_OPS(&dnp->dn_lock);
		if (devopsp[maj] && devopsp[maj]->devo_cb_ops &&
		    !devopsp[maj]->devo_cb_ops->cb_str) {
			UNLOCK_DEV_OPS(&dnp->dn_lock);
			return (e_ddi_getprop(dev, VCHR, "size",
			    DDI_PROP_NOTPROM | DDI_PROP_DONTPASS, 0));
		} else
			UNLOCK_DEV_OPS(&dnp->dn_lock);
	}
	return (0);
}

/*
 * same for 64-bit Size property
 */
uint64_t
cdev_Size(dev_t dev)
{
	major_t maj;
	struct devnames *dnp;

	if ((maj = getmajor(dev)) >= devcnt)
		return (0);

	dnp = &(devnamesp[maj]);
	if ((dnp->dn_flags & DN_DEVS_ATTACHED) && devopsp[maj]) {
		LOCK_DEV_OPS(&dnp->dn_lock);
		if (devopsp[maj] && devopsp[maj]->devo_cb_ops &&
		    !devopsp[maj]->devo_cb_ops->cb_str) {
			UNLOCK_DEV_OPS(&dnp->dn_lock);
			return (e_ddi_getprop_int64(dev, VCHR, "Size",
			    DDI_PROP_NOTPROM | DDI_PROP_DONTPASS, 0));
		} else
			UNLOCK_DEV_OPS(&dnp->dn_lock);
	}
	return (0);
}

/*
 * XXX	This routine is poorly named, because block devices can and do
 *	have properties (see bdev_size() above).
 *
 * XXX	fix the comment in devops.h that claims that cb_prop_op
 *	is character-only.
 */
int
cdev_prop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op, int mod_flags,
    char *name, caddr_t valuep, int *lengthp)
{
	struct cb_ops	*cb;

	if ((cb = devopsp[getmajor(dev)]->devo_cb_ops) == NULL)
		return (DDI_PROP_NOT_FOUND);

	return ((*cb->cb_prop_op)(dev, dip, prop_op, mod_flags,
	    name, valuep, lengthp));
}

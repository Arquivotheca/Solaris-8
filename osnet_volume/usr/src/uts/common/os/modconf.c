/*
 * Copyright (c) 1988-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)modconf.c	1.54	99/07/07 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/vm.h>
#include <sys/conf.h>
#include <sys/class.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <sys/systm.h>
#include <sys/modctl.h>
#include <sys/exec.h>
#include <sys/exechdr.h>
#include <sys/devops.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cmn_err.h>
#include <sys/hwconf.h>
#include <sys/ddi_impldefs.h>
#include <sys/autoconf.h>
#include <sys/disp.h>
#include <sys/kmem.h>
#include <sys/instance.h>
#include <sys/modhash.h>
#include <sys/dacf.h>
#include <sys/debug.h>

extern int moddebug;

extern struct cb_ops no_cb_ops;
extern struct dev_ops nodev_ops;
extern struct dev_ops mod_nodev_ops;

extern struct modctl *mod_getctl(struct modlinkage *);
extern int errsys(), nodev(), nulldev();

extern struct vfssw *vfs_getvfsswbyname(char *);
extern int vfs_opsinuse(struct vfsops *);
extern struct vfssw *allocate_vfssw(char *);

extern int findmodbyname(char *);
extern int mod_getsysnum(char *);

extern int impl_probe_attach_devi(dev_info_t *);
extern void impl_unattach_devs(major_t);
extern void ddi_orphan_devs(dev_info_t *);
extern void ddi_unorphan_devs(major_t);
extern void impl_unattach_driver(major_t);
extern int impl_initdev(dev_info_t *);

extern struct execsw execsw[];
extern struct vfssw vfssw[];

/*
 * Define dev_ops for unused devopsp entry.
 */
struct dev_ops mod_nodev_ops = {
	DEVO_REV,		/* devo_rev	*/
	0,			/* refcnt	*/
	ddi_no_info,		/* info */
	nulldev,		/* identify	*/
	nulldev,		/* probe	*/
	ddifail,		/* attach	*/
	nodev,			/* detach	*/
	nulldev,		/* reset	*/
	&no_cb_ops,		/* character/block driver operations */
	(struct bus_ops *)0	/* bus operations for nexus drivers */
};

/*
 * Define mod_ops for each supported module type
 */

/*
 * Null operations; used for uninitialized and "misc" modules.
 */
static int mod_null(struct modldrv *, struct modlinkage *);
static int mod_infonull(void *, struct modlinkage *, int *);

struct mod_ops mod_miscops = {
	mod_null, mod_null, mod_infonull
};

/*
 * Device drivers
 */
static int mod_infodrv(struct modldrv *, struct modlinkage *, int *);
static int mod_installdrv(struct modldrv *, struct modlinkage *);
static int mod_removedrv(struct modldrv *, struct modlinkage *);

struct mod_ops mod_driverops = {
	mod_installdrv, mod_removedrv, mod_infodrv
};

/*
 * System calls (new interface)
 */
static int mod_infosys(struct modlsys *, struct modlinkage *, int *);
static int mod_installsys(struct modlsys *, struct modlinkage *);
static int mod_removesys(struct modlsys *, struct modlinkage *);

struct mod_ops mod_syscallops = {
	mod_installsys, mod_removesys, mod_infosys
};

#ifdef _SYSCALL32_IMPL
/*
 * 32-bit system calls in 64-bit kernel
 */
static int mod_infosys32(struct modlsys *, struct modlinkage *, int *);
static int mod_installsys32(struct modlsys *, struct modlinkage *);
static int mod_removesys32(struct modlsys *, struct modlinkage *);

struct mod_ops mod_syscallops32 = {
	mod_installsys32, mod_removesys32, mod_infosys32
};
#endif	/* _SYSCALL32_IMPL */

/*
 * Filesystems
 */
static int mod_infofs(struct modlfs *, struct modlinkage *, int *);
static int mod_installfs(struct modlfs *, struct modlinkage *);
static int mod_removefs(struct modlfs *, struct modlinkage *);

struct mod_ops mod_fsops = {
	mod_installfs, mod_removefs, mod_infofs
};

/*
 * Streams modules.
 */
static int mod_infostrmod(struct modlstrmod *, struct modlinkage *, int *);
static int mod_installstrmod(struct modlstrmod *, struct modlinkage *);
static int mod_removestrmod(struct modlstrmod *, struct modlinkage *);

struct mod_ops mod_strmodops = {
	mod_installstrmod, mod_removestrmod, mod_infostrmod
};

/*
 * Scheduling classes.
 */
static int mod_infosched(struct modlsched *, struct modlinkage *, int *);
static int mod_installsched(struct modlsched *, struct modlinkage *);
static int mod_removesched(struct modlsched *, struct modlinkage *);

struct mod_ops mod_schedops = {
	mod_installsched, mod_removesched, mod_infosched
};

/*
 * Exec file type (like COFF, ...).
 */
static int mod_infoexec(struct modlexec *, struct modlinkage *, int *);
static int mod_installexec(struct modlexec *, struct modlinkage *);
static int mod_removeexec(struct modlexec *, struct modlinkage *);

struct mod_ops mod_execops = {
	mod_installexec, mod_removeexec, mod_infoexec
};

/*
 * Dacf (Dynamic Autoconfiguration) modules.
 */
static int mod_infodacf(struct modldacf *, struct modlinkage *, int *);
static int mod_installdacf(struct modldacf *, struct modlinkage *);
static int mod_removedacf(struct modldacf *, struct modlinkage *);

struct mod_ops mod_dacfops = {
	mod_installdacf, mod_removedacf, mod_infodacf
};

static struct sysent *mod_getsysent(struct modlinkage *, struct sysent *);

static char uninstall_err[] = "Cannot uninstall %s; not installed";

/*
 * Debugging support
 */
#define	DRV_DBG		MODDEBUG_LOADMSG2

/*PRINTFLIKE2*/
static void
mod_dprintf(int flag, char *format, ...)
{
	va_list alist;
	if ((moddebug & flag) != 0) {
		va_start(alist, format);
		(void) vprintf(format, alist);
		va_end(alist);
	}
}

/*
 * Install a module.
 * (This routine is in the Solaris SPARC DDI/DKI)
 */
int
mod_install(struct modlinkage *modlp)
{
	int retval = -1;	/* No linkage structures */
	struct modlmisc **linkpp;
	struct modlmisc **linkpp1;

	if (modlp->ml_rev != MODREV_1) {
		printf("mod_install:  modlinkage structure is not MODREV_1\n");
		return (EINVAL);
	}
	linkpp = (struct modlmisc **)&modlp->ml_linkage[0];

	while (*linkpp != NULL) {
		if ((retval = MODL_INSTALL(*linkpp, modlp)) != 0) {
			linkpp1 = (struct modlmisc **)&modlp->ml_linkage[0];

			while (linkpp1 != linkpp) {
				MODL_REMOVE(*linkpp1, modlp); /* clean up */
				linkpp1++;
			}
			break;
		}
		linkpp++;
	}
	return (retval);
}

static char *reins_err =
	"Could not reinstall %s\nReboot to correct the problem";

/*
 * Remove a module.  This is called by the module wrapper routine.
 * (This routine is in the Solaris SPARC DDI/DKI)
 */
int
mod_remove(struct modlinkage *modlp)
{
	int retval = 0;
	struct modlmisc **linkpp, *last_linkp;

	linkpp = (struct modlmisc **)&modlp->ml_linkage[0];

	while (*linkpp != NULL) {
		if ((retval = MODL_REMOVE(*linkpp, modlp)) != 0) {
			last_linkp = *linkpp;
			linkpp = (struct modlmisc **)&modlp->ml_linkage[0];
			while (*linkpp != last_linkp) {
				if (MODL_INSTALL(*linkpp, modlp) != 0) {
					cmn_err(CE_WARN, reins_err,
						(*linkpp)->misc_linkinfo);
					break;
				}
				linkpp++;
			}
			break;
		}
		linkpp++;
	}
	return (retval);
}

/*
 * Get module status.
 * (This routine is in the Solaris SPARC DDI/DKI)
 */
int
mod_info(struct modlinkage *modlp, struct modinfo *modinfop)
{
	int i;
	int retval = 0;
	struct modspecific_info *msip;
	struct modlmisc **linkpp;

	modinfop->mi_rev = modlp->ml_rev;

	linkpp = (struct modlmisc **)modlp->ml_linkage;
	msip = &modinfop->mi_msinfo[0];

	for (i = 0; i < MODMAXLINK; i++) {
		if (*linkpp == NULL) {
			msip->msi_linkinfo[0] = '\0';
		} else {
			(void) strncpy(msip->msi_linkinfo,
			    (*linkpp)->misc_linkinfo, MODMAXLINKINFOLEN);
			retval = MODL_INFO(*linkpp, modlp, &msip->msi_p0);
			if (retval != 0)
				break;
			linkpp++;
		}
		msip++;
	}

	if (modinfop->mi_info == MI_INFO_LINKAGE) {
		/*
		 * Slight kludge used to extract the address of the
		 * modlinkage structure from the module (just after
		 * loading a module for the very first time)
		 */
		modinfop->mi_base = (void *)modlp;
	}

	if (retval == 0)
		return (1);
	return (0);
}

/*
 * Null operation; return 0.
 */
/*ARGSUSED*/
static int
mod_null(struct modldrv *modl, struct modlinkage *modlp)
{
	return (0);
}

/*
 * Status for User modules.
 */
/*ARGSUSED*/
static int
mod_infonull(void *modl, struct modlinkage *modlp, int *p0)
{
	*p0 = -1;		/* for modinfo display */
	return (0);
}

/*
 * Driver status info
 */
/*ARGSUSED*/
static int
mod_infodrv(struct modldrv *modl, struct modlinkage *modlp, int *p0)
{
	struct modctl *mcp;
	char *mod_name;

	if ((mcp = mod_getctl(modlp)) == NULL) {
		*p0 = -1;
		return (0);	/* driver is not yet installed */
	}

	mod_name = mcp->mod_modname;

	*p0 = ddi_name_to_major(mod_name);
	return (0);
}

static int
ddi_installdrv(struct dev_ops *ops, char *modname)
{
	int major;
	struct dev_ops **dp;
	kmutex_t *lp;

	if ((major = ddi_name_to_major(modname)) == -1) {
		cmn_err(CE_WARN, "ddi_installdrv: no major number for %s",
			modname);
		return (DDI_FAILURE);
	}
	lp = &(devnamesp[major].dn_lock);

	LOCK_DEV_OPS(lp);
	dp = &devopsp[major];
	if (*dp != &nodev_ops && *dp != &mod_nodev_ops) {
		UNLOCK_DEV_OPS(lp);
		return (DDI_FAILURE);
	}
	*dp = ops; /* setup devopsp */
	UNLOCK_DEV_OPS(lp);
	ddi_unorphan_devs(major);
	e_ddi_unorphan_instance_nos();
	return (DDI_SUCCESS);
}

/*
 * Manage dacf (device autoconfiguration) modules
 */

/*ARGSUSED*/
static int
mod_infodacf(struct modldacf *modl, struct modlinkage *modlp, int *p0)
{
	if (mod_getctl(modlp) == NULL) {
		*p0 = -1;
		return (0);	/* module is not yet installed */
	}

	*p0 = 0;
	return (0);
}

static int
mod_installdacf(struct modldacf *modl, struct modlinkage *modlp)
{
	char *modname = (mod_getctl(modlp))->mod_modname;

	return (dacf_module_register(modname, modl->dacf_dacfsw));
}

/*ARGSUSED*/
static int
mod_removedacf(struct modldacf *modl, struct modlinkage *modlp)
{
	char *modname = (mod_getctl(modlp))->mod_modname;

	return (dacf_module_unregister(modname));
}

/*
 * Install a new driver
 */

static int
mod_installdrv(struct modldrv *modl, struct modlinkage *modlp)
{
	int status;
	struct modctl *mcp;
	struct dev_ops *ops;
	char *modname;

	ops = modl->drv_dev_ops;
	mcp = mod_getctl(modlp);
	modname = mcp->mod_modname;

	if (ops->devo_bus_ops == NULL && ops->devo_cb_ops != NULL &&
	    !(ops->devo_cb_ops->cb_flag & D_MP)) {
		cmn_err(CE_WARN, "mod_install: MT-unsafe driver '%s' rejected",
		    modname);
		return (ENXIO);
	}

	status = ddi_installdrv(ops, modname);
	if (status != DDI_SUCCESS) {
		cmn_err(CE_WARN, "mod_installdrv: Cannot install %s", modname);
		status = ENOSPC;
	}

	return (status);
}

static int
mod_detachdrv(major_t major)
{
	struct dev_ops *ops = devopsp[major];
	struct devnames *dnp = &(devnamesp[major]);
	dev_info_t *dip, *sdip;
	int error = 0;
	char *modname = ddi_major_to_name(major);

	ASSERT(mutex_owned(&(dnp->dn_lock)));

	/*
	 * Lock for driver `major' is held and it's reference
	 * count is zero and it has been attached.
	 *
	 * For each driver instance, call the driver's detach function
	 * If one of them fails, reattach all the instances
	 * because we don't have any notion of a partially attached driver.
	 * If it succeeds, remove pseudo devinfo nodes and put
	 * prom devinfo nodes back to canonical form 1.
	 */

	if ((ops == NULL) || (ops->devo_detach == NULL) ||
	    (ops->devo_detach == nodev))  {
		mod_dprintf(DRV_DBG, "No detach function for device "
		    "driver <%s>\n", modname);
		return (EBUSY);
	}

	if (dnp->dn_head)
		mod_dprintf(DRV_DBG, "Detaching device driver <%s>\n",
		    dnp->dn_name);

	/*
	 * Bump the refcnt on the ops - we're still 'here'
	 */
	INCR_DEV_OPS_REF(ops);

	/*
	 * Drop the per-driver mutex; nobody else can get in to change things
	 * because the busy/unloading flags are set. We need to drop it
	 * because we are calling into the driver and it is allowed to sleep.
	 */
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	for (dip = dnp->dn_head; dip != NULL; dip = ddi_get_next(dip))  {
		if (!DDI_CF2(dip))
			continue;	/* Skip if not attached */
		if ((error = devi_detach(dip, DDI_DETACH)) != DDI_SUCCESS)
			break;
	}

	/*
	 * If error is non-zero, we have to clean up the damage we might
	 * have done and reattach the driver to anything we already
	 * detached it from. (We could allow "lazy" re-attach, though.)
	 */
	if (error != 0)  {
		dev_info_t *ndip = NULL;	/* 1094364 */
		sdip = dip;
		mod_dprintf(DRV_DBG, "Detach failed for %s%d\n", modname,
		    ddi_get_instance(sdip));
		for (dip = dnp->dn_head; dip != sdip; dip = ndip)  {
			ndip = ddi_get_next(dip);
			/* Skip unnamed prototypes */
			if (DDI_CF1(dip) && DDI_CF2(dip)) {
				(void) impl_initdev(dip);
			}
		}
		LOCK_DEV_OPS(&(dnp->dn_lock));
		DECR_DEV_OPS_REF(ops);
		return (EBUSY);
	}
	LOCK_DEV_OPS(&(dnp->dn_lock));
	DECR_DEV_OPS_REF(ops);
	return (0);
}

static int
mod_removedrv(struct modldrv *modl, struct modlinkage *modlp)
{
	struct modctl *mcp;
	struct dev_ops *ops;
	struct devnames *dnp;
	int major, e;
	int no_broadcast;
	struct dev_ops **dp;
	char *modname;
	extern kthread_id_t mod_aul_thread;

	if ((moddebug & MODDEBUG_NOAUL_DRV) && (mod_aul_thread == curthread))
		return (EBUSY);

	ops = modl->drv_dev_ops;
	mcp = mod_getctl(modlp);
	ASSERT(mcp != NULL);
	modname = mcp->mod_modname;
	if ((major = ddi_name_to_major(modname)) == -1) {
		cmn_err(CE_WARN, uninstall_err, modname);
		return (EINVAL);
	}

	dnp = &(devnamesp[major]);
	LOCK_DEV_OPS(&(dnp->dn_lock));

	dp = &devopsp[major];
	if (*dp != ops)  {
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		cmn_err(CE_NOTE, "?Mismatched device driver ops for <%s>",
			modname);
		return (EBUSY);
	}

	/*
	 * A driver is not unloadable if its dev_ops are held or
	 * it is an attached nexus driver.
	 */
	if ((!DRV_UNLOADABLE(ops)) ||
	    (NEXUS_DRV(ops) && (dnp->dn_flags & DN_DEVS_ATTACHED)))  {
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		mod_dprintf(DRV_DBG, "Cannot unload device driver <%s>,"
		    " refcnt %d\n", modname, (*dp)->devo_refcnt);
		return (EBUSY);
	}

	/*
	 * If the driver is loading, or if it is unloading
	 * and this is not the thread doing the unloading,
	 * then get out of the way: the driver is busy. No need
	 * to wait here for this to change -- we might end up
	 * waiting and find that the calling driver has been removed
	 * from underneath us and that would not be desirable.
	 *
	 * If it is busy being unloaded and we are the thread
	 * doing it, that means we are being called on a failed hold
	 * via ddi_hold_installed_driver.
	 */
	if ((dnp->dn_flags & DN_BUSY_LOADING) ||
	    (DN_BUSY_CHANGING(dnp->dn_flags) &&
	    (dnp->dn_busy_thread != curthread)))  {
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		return (EBUSY);
	}

	/*
	 * If we are called with busy/changing already held by another
	 * caller within this thread, the caller will take care of the
	 * broadcast.  The caller is ddi_hold_installed_driver.  This
	 * allows mod_detachdrv to drop the mutex before calling the driver.
	 */
	no_broadcast = (int)dnp->dn_busy_thread;
	dnp->dn_flags |= DN_BUSY_UNLOADING;

	if (dnp->dn_flags & DN_DEVS_ATTACHED)  {
		if ((e = mod_detachdrv(major)) != 0)  {
			/*
			 * This means a detach failed, cannot be called
			 * via ddi_hold_installed_driver in this case.
			 */
			ASSERT(no_broadcast == 0);
			dnp->dn_flags &= ~(DN_BUSY_CHANGING_BITS);
			cv_broadcast(&(dnp->dn_wait));
			UNLOCK_DEV_OPS(&(dnp->dn_lock));
			return (e);
		}
	}

	/*
	 * OK to unload.
	 * Unattach this driver from any remaining proto/cf1 instances.
	 * and place any remaining devinfos on the orphan list.
	 */
	impl_unattach_devs(major);
	impl_unattach_driver(major);
	*dp = &mod_nodev_ops;
	dnp->dn_flags &= ~(DN_DEVS_ATTACHED | DN_WALKED_TREE);
	if (dnp->dn_head != NULL)
		ddi_orphan_devs(dnp->dn_head);
	dnp->dn_head = NULL;
	if (dnp->dn_inlist != NULL) {
		e_ddi_orphan_instance_nos(dnp->dn_inlist);
		dnp->dn_instance = IN_SEARCHME;
	}
	dnp->dn_inlist = NULL;
	if (no_broadcast == 0) {
		dnp->dn_flags &= ~DN_BUSY_CHANGING_BITS;
		cv_broadcast(&(dnp->dn_wait));
	}
	UNLOCK_DEV_OPS(&(dnp->dn_lock));
	return (0);
}

/*
 * System call subroutines
 */

/*
 * Compute system call number for given sysent and sysent table
 */
static int
mod_infosysnum(struct modlinkage *modlp, struct sysent table[])
{
	struct sysent *sysp;

	if ((sysp = mod_getsysent(modlp, table)) == NULL)
		return (-1);
	return ((int)(sysp - table));
}

/*
 * Put a loadable system call entry into a sysent table.
 */
static int
mod_installsys_sysent(
	struct modlsys		*modl,
	struct modlinkage	*modlp,
	struct sysent		table[])
{
	struct sysent *sysp;
	struct sysent *mp;

#ifdef DEBUG
	/*
	 * Before we even play with the sysent table, sanity check the
	 * incoming flags to make sure the entry is valid
	 */
	switch (modl->sys_sysent->sy_flags & SE_RVAL_MASK) {
	case SE_32RVAL1:
		/* only r_val1 returned */
	case SE_32RVAL1 | SE_32RVAL2:
		/* r_val1 and r_val2 returned */
	case SE_64RVAL:
		/* 64-bit rval returned */
		break;
	default:
		cmn_err(CE_WARN, "loadable syscall: %p: bad rval flags %x",
		    (void *)modl, modl->sys_sysent->sy_flags);
		return (ENOSYS);
	}
#endif
	if ((sysp = mod_getsysent(modlp, table)) == NULL)
		return (ENOSPC);

	/*
	 * We should only block here until the reader in syscall gives
	 * up the lock.  Multiple writers are prevented in the mod layer.
	 */
	rw_enter(sysp->sy_lock, RW_WRITER);
	mp = modl->sys_sysent;
	sysp->sy_narg = mp->sy_narg;
	sysp->sy_call = mp->sy_call;

	/*
	 * clear the old call method flag, and get the new one from the module.
	 */
	sysp->sy_flags &= ~SE_ARGC;
	sysp->sy_flags |= SE_LOADED |
	    (mp->sy_flags & (SE_ARGC | SE_NOUNLOAD | SE_RVAL_MASK));

	/*
	 * If the syscall doesn't need or want unloading, it can avoid
	 * the locking overhead on each entry.  Convert the sysent to a
	 * normal non-loadable entry in that case.
	 */
	if (mp->sy_flags & SE_NOUNLOAD) {
		if (mp->sy_flags & SE_ARGC) {
			sysp->sy_callc = (int64_t (*)())mp->sy_call;
		} else {
			sysp->sy_callc = syscall_ap;
		}
		sysp->sy_flags &= ~SE_LOADABLE;
	}
	rw_exit(sysp->sy_lock);
	return (0);
}

/*
 * Remove a loadable system call entry from a sysent table.
 */
static int
mod_removesys_sysent(
	struct modlsys		*modl,
	struct modlinkage	*modlp,
	struct sysent		table[])
{
	struct sysent	*sysp;

	if ((sysp = mod_getsysent(modlp, table)) == NULL ||
	    (sysp->sy_flags & (SE_LOADABLE | SE_NOUNLOAD)) == 0 ||
	    sysp->sy_call != modl->sys_sysent->sy_call) {

		struct modctl *mcp = mod_getctl(modlp);
		char *modname = mcp->mod_modname;

		cmn_err(CE_WARN, uninstall_err, modname);
		return (EINVAL);
	}

	/* If we can't get the write lock, we can't unlink from the system */

	if (!(moddebug & MODDEBUG_NOAUL_SYS) &&
	    rw_tryenter(sysp->sy_lock, RW_WRITER)) {
		/*
		 * Check the flags to be sure the syscall is still
		 * (un)loadable.
		 * If SE_NOUNLOAD is set, SE_LOADABLE will not be.
		 */
		if ((sysp->sy_flags & (SE_LOADED | SE_LOADABLE)) ==
		    (SE_LOADED | SE_LOADABLE)) {
			sysp->sy_flags &= ~SE_LOADED;
			sysp->sy_callc = loadable_syscall;
			sysp->sy_call = (int (*)())nosys;
			rw_exit(sysp->sy_lock);
			return (0);
		}
		rw_exit(sysp->sy_lock);
	}
	return (EBUSY);
}

/*
 * System call status info
 */
/*ARGSUSED*/
static int
mod_infosys(struct modlsys *modl, struct modlinkage *modlp, int *p0)
{
	*p0 = mod_infosysnum(modlp, sysent);
	return (0);
}

/*
 * Link a system call into the system by setting the proper sysent entry.
 * Called from the module's _init routine.
 */
static int
mod_installsys(struct modlsys *modl, struct modlinkage *modlp)
{
	return (mod_installsys_sysent(modl, modlp, sysent));
}

/*
 * Unlink a system call from the system.
 * Called from a modules _fini routine.
 */
static int
mod_removesys(struct modlsys *modl, struct modlinkage *modlp)
{
	return (mod_removesys_sysent(modl, modlp, sysent));
}

#ifdef _SYSCALL32_IMPL

/*
 * 32-bit system call status info
 */
/*ARGSUSED*/
static int
mod_infosys32(struct modlsys *modl, struct modlinkage *modlp, int *p0)
{
	*p0 = mod_infosysnum(modlp, sysent32);
	return (0);
}

/*
 * Link the 32-bit syscall into the system by setting the proper sysent entry.
 * Also called from the module's _init routine.
 */
static int
mod_installsys32(struct modlsys *modl, struct modlinkage *modlp)
{
	return (mod_installsys_sysent(modl, modlp, sysent32));
}

/*
 * Unlink the 32-bit flavour of a system call from the system.
 * Also called from a module's _fini routine.
 */
static int
mod_removesys32(struct modlsys *modl, struct modlinkage *modlp)
{
	return (mod_removesys_sysent(modl, modlp, sysent32));
}

#endif	/* _SYSCALL32_IMPL */

/*
 * Filesystem status info
 */
/*ARGSUSED*/
static int
mod_infofs(struct modlfs *modl, struct modlinkage *modlp, int *p0)
{
	struct vfssw *vswp;

	RLOCK_VFSSW();
	if ((vswp = vfs_getvfsswbyname(modl->fs_vfssw->vsw_name)) == NULL)
		*p0 = -1;
	else
		*p0 = vswp - vfssw;
	RUNLOCK_VFSSW();
	return (0);
}

/*
 * Install a filesystem
 * Return with vfssw locked.
 */
/*ARGSUSED1*/
static int
mod_installfs(struct modlfs *modl, struct modlinkage *modlp)
{
	struct vfssw *vswp;
	char *fsname = modl->fs_vfssw->vsw_name;

	WLOCK_VFSSW();
	if ((vswp = vfs_getvfsswbyname(fsname)) == NULL) {
		if ((vswp = allocate_vfssw(fsname)) == NULL) {
			WUNLOCK_VFSSW();
			/*
			 * See 1095689.  If this message appears, then
			 * we either need to make the vfssw table bigger
			 * statically, or make it grow dynamically.
			 */
			cmn_err(CE_WARN, "no room for '%s' in vfssw!", fsname);
			return (ENXIO);
		}
	}
	ASSERT(vswp != NULL);

	vswp->vsw_init = modl->fs_vfssw->vsw_init;
	vswp->vsw_flag = modl->fs_vfssw->vsw_flag;
	/* XXX - The vsw_init entry should do this */
	vswp->vsw_vfsops = modl->fs_vfssw->vsw_vfsops;
	if (modl->fs_vfssw->vsw_flag & VSW_HASPROTO)
		vswp->vsw_optproto = modl->fs_vfssw->vsw_optproto;

	(*(vswp->vsw_init))(vswp, vswp - vfssw);
	WUNLOCK_VFSSW();

	return (0);
}

/*
 * Remove a filesystem
 */
static int
mod_removefs(struct modlfs *modl, struct modlinkage *modlp)
{
	struct vfssw *vswp;
	struct modctl *mcp;
	char *modname;

	WLOCK_VFSSW();
	if ((moddebug & MODDEBUG_NOAUL_FS) ||
	    vfs_opsinuse(modl->fs_vfssw->vsw_vfsops)) {
		WUNLOCK_VFSSW();
		return (EBUSY);
	}

	if ((vswp = vfs_getvfsswbyname(modl->fs_vfssw->vsw_name)) == NULL) {
		mcp = mod_getctl(modlp);
		ASSERT(mcp != NULL);
		modname = mcp->mod_modname;
		WUNLOCK_VFSSW();
		cmn_err(CE_WARN, uninstall_err, modname);
		return (EINVAL);
	}
	vswp->vsw_flag = 0;
	vswp->vsw_init = NULL;
	vswp->vsw_vfsops = NULL;
	WUNLOCK_VFSSW();
	return (0);
}

/*
 * Get status of a streams module.
 */
/*ARGSUSED1*/
static int
mod_infostrmod(struct modlstrmod *modl, struct modlinkage *modlp, int *p0)
{
	extern kmutex_t fmodsw_lock;

	mutex_enter(&fmodsw_lock);
	*p0 = findmodbyname(modl->strmod_fmodsw->f_name);
	mutex_exit(&fmodsw_lock);
	return (0);
}


/*
 * Install a streams module.
 */
/* ARGSUSED */
static int
mod_installstrmod(struct modlstrmod *modl, struct modlinkage *modlp)
{
	int mid;
	fmodsw_impl_t *fmp;

	static char *no_fmodsw = "No available slots in fmodsw table for %s";

	if (!(modl->strmod_fmodsw->f_flag & D_MP)) {
		cmn_err(CE_WARN, "mod_install: MT-unsafe strmod '%s' rejected",
		    modl->strmod_fmodsw->f_name);
		return (ENXIO);
	}

	/*
	 * See if module is already installed.
	 */
	mid = allocate_fmodsw(modl->strmod_fmodsw->f_name);
	if (mid == -1) {
		cmn_err(CE_WARN, no_fmodsw, modl->strmod_fmodsw->f_name);
		return (ENOMEM);
	}
	fmp = &fmodsw[mid];

	mutex_enter(fmp->f_lock);
	fmp->f_str = modl->strmod_fmodsw->f_str;
	fmp->f_flag = modl->strmod_fmodsw->f_flag;
	mutex_exit(fmp->f_lock);
	return (0);
}

/*
 * Remove a streams module.
 */
static int
mod_removestrmod(struct modlstrmod *modl, struct modlinkage *modlp)
{
	int mid;
	fmodsw_impl_t *fmp;
	struct modctl *mcp;
	char *modname;

	/*
	 * Hold the fmodsw lock while searching the fmodsw table.  This
	 * interlocks searching and allocation.
	 */
	mutex_enter(&fmodsw_lock);
	mid = findmodbyname(modl->strmod_fmodsw->f_name);
	/*
	 * Done searching, give up the lock.
	 *
	 * Note: the fmodsw entry is never deallocated, so, since we found
	 * it, it won't change. That's why we don't need to hold this
	 * lock while writing the allocated entry.
	 */
	mutex_exit(&fmodsw_lock);
	if (mid == -1) {
		mcp = mod_getctl(modlp);
		ASSERT(mcp != NULL);
		modname = mcp->mod_modname;
		cmn_err(CE_WARN, uninstall_err, modname);
		return (EINVAL);
	}
	fmp = &fmodsw[mid];

	if (moddebug & MODDEBUG_NOAUL_STR)
		return (EBUSY);
	/*
	 * A non-zero count indicates that this module is
	 * in use.
	 */
	mutex_enter(fmp->f_lock);
	if (fmp->f_count != 0) {
		mutex_exit(fmp->f_lock);
		return (EBUSY);
	}
	fmp->f_str = NULL;
	fmp->f_flag = 0;
	mutex_exit(fmp->f_lock);
	return (0);
}

/*
 * Get status of a scheduling class module.
 */
/*ARGSUSED1*/
static int
mod_infosched(struct modlsched *modl, struct modlinkage *modlp, int *p0)
{
	int	status;
	auto id_t	cid;
	extern kmutex_t class_lock;

	mutex_enter(&class_lock);
	status = getcidbyname(modl->sched_class->cl_name, &cid);
	mutex_exit(&class_lock);

	if (status != 0)
		*p0 = -1;
	else
		*p0 = cid;

	return (0);
}

/*
 * Install a scheduling class module.
 */
/*ARGSUSED1*/
static int
mod_installsched(struct modlsched *modl, struct modlinkage *modlp)
{
	sclass_t *clp;
	int status;
	id_t cid;

	/*
	 * See if module is already installed.
	 */
	mutex_enter(&class_lock);
	status = alloc_cid(modl->sched_class->cl_name, &cid);
	mutex_exit(&class_lock);
	ASSERT(status == 0);
	clp = &sclass[cid];
	rw_enter(clp->cl_lock, RW_WRITER);
	if (SCHED_INSTALLED(clp)) {
		printf("scheduling class %s is already installed\n",
			modl->sched_class->cl_name);
		rw_exit(clp->cl_lock);
		return (EBUSY);		/* it's already there */
	}

	clp->cl_init = modl->sched_class->cl_init;
	clp->cl_funcs = modl->sched_class->cl_funcs;
	modl->sched_class = clp;
	disp_add(clp);
	loaded_classes++;		/* for priocntl system call */
	rw_exit(clp->cl_lock);
	return (0);
}

/*
 * Remove a scheduling class module.
 *
 * we only null out the init func and the class functions because
 * once a class has been loaded it has that slot in the class
 * array until the next reboot. We don't decrement loaded_classes
 * because this keeps count of the number of classes that have
 * been loaded for this session. It will have to be this way until
 * we implement the class array as a linked list and do true
 * dynamic allocation.
 */
static int
mod_removesched(struct modlsched *modl, struct modlinkage *modlp)
{
	int status;
	sclass_t *clp;
	struct modctl *mcp;
	char *modname;
	id_t cid;

	extern kmutex_t class_lock;

	mutex_enter(&class_lock);
	status = getcidbyname(modl->sched_class->cl_name, &cid);
	mutex_exit(&class_lock);
	if (status != 0) {
		mcp = mod_getctl(modlp);
		ASSERT(mcp != NULL);
		modname = mcp->mod_modname;
		cmn_err(CE_WARN, uninstall_err, modname);
		return (EINVAL);
	}
	clp = &sclass[cid];
	if (moddebug & MODDEBUG_NOAUL_SCHED ||
	    !rw_tryenter(clp->cl_lock, RW_WRITER))
		return (EBUSY);

	clp->cl_init = NULL;
	clp->cl_funcs = NULL;
	rw_exit(clp->cl_lock);
	return (0);
}

/*
 * Get status of an exec module.
 */
/*ARGSUSED1*/
static int
mod_infoexec(struct modlexec *modl, struct modlinkage *modlp, int *p0)
{
	struct execsw *eswp;

	if ((eswp = findexecsw(modl->exec_execsw->exec_magic)) == NULL)
		*p0 = -1;
	else
		*p0 = eswp - execsw;

	return (0);
}

/*
 * Install an exec module.
 */
static int
mod_installexec(struct modlexec *modl, struct modlinkage *modlp)
{
	struct execsw *eswp;
	struct modctl *mcp;
	char *modname;
	char *magic;
	size_t magic_size;

	/*
	 * See if execsw entry is already allocated.  Can't use findexectype()
	 * because we may get a recursive call to here.
	 */

	if ((eswp = findexecsw(modl->exec_execsw->exec_magic)) == NULL) {
		mcp = mod_getctl(modlp);
		ASSERT(mcp != NULL);
		modname = mcp->mod_modname;
		magic = modl->exec_execsw->exec_magic;
		magic_size = modl->exec_execsw->exec_maglen;
		if ((eswp = allocate_execsw(modname, magic, magic_size)) ==
		    NULL) {
			printf("no unused entries in 'execsw'\n");
			return (ENOSPC);
		}
	}
	if (eswp->exec_func != NULL) {
		printf("exec type %x is already installed\n",
			*eswp->exec_magic);
			return (EBUSY);		 /* it's already there! */
	}

	rw_enter(eswp->exec_lock, RW_WRITER);
	eswp->exec_func = modl->exec_execsw->exec_func;
	eswp->exec_core = modl->exec_execsw->exec_core;
	rw_exit(eswp->exec_lock);

	return (0);
}

/*
 * Remove an exec module.
 */
static int
mod_removeexec(struct modlexec *modl, struct modlinkage *modlp)
{
	struct execsw *eswp;
	struct modctl *mcp;
	char *modname;

	eswp = findexecsw(modl->exec_execsw->exec_magic);
	if (eswp == NULL) {
		mcp = mod_getctl(modlp);
		ASSERT(mcp != NULL);
		modname = mcp->mod_modname;
		cmn_err(CE_WARN, uninstall_err, modname);
		return (EINVAL);
	}
	if (moddebug & MODDEBUG_NOAUL_EXEC ||
	    !rw_tryenter(eswp->exec_lock, RW_WRITER))
		return (EBUSY);
	eswp->exec_func = NULL;
	eswp->exec_core = NULL;
	rw_exit(eswp->exec_lock);
	return (0);
}

/*
 * Find a free sysent entry or check if the specified one is free.
 */
static struct sysent *
mod_getsysent(struct modlinkage *modlp, struct sysent *se)
{
	int sysnum;
	struct modctl *mcp;
	char *mod_name;

	if ((mcp = mod_getctl(modlp)) == NULL) {
		/*
		 * This happens when we're looking up the module
		 * pointer as part of a stub installation.  So
		 * there's no need to whine at this point.
		 */
		return (NULL);
	}

	mod_name = mcp->mod_modname;

	if ((sysnum = mod_getsysnum(mod_name)) == -1) {
		cmn_err(CE_WARN, "system call missing from bind file");
		return (NULL);
	}

	if (sysnum > 0 && sysnum < NSYSCALL &&
	    (se[sysnum].sy_flags & (SE_LOADABLE | SE_NOUNLOAD)))
		return (se + sysnum);

	cmn_err(CE_WARN, "system call entry %d is already in use", sysnum);
	return (NULL);
}

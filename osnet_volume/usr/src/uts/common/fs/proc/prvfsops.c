/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)prvfsops.c	1.41	99/04/15 SMI"	/* SVr4.0 1.25  */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mode.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/mount.h>
#include <sys/bitmap.h>
#include <sys/kmem.h>
#include <fs/fs_subr.h>
#include <fs/proc/prdata.h>

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

static struct vfsops prvfsops;
static int prinit();

static struct vfssw vfw = {
	"proc",
	prinit,
	&prvfsops,
	0
};

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_fsops;

static struct modlfs modlfs = {
	&mod_fsops, "filesystem for proc", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlfs, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * N.B.
 * No _fini routine. The module cannot be unloaded once loaded.
 * The NO_UNLOAD_STUB in modstubs.s must change if this module
 * is ever modified to become unloadable.
 */

int		nproc_highbit;		/* highbit(v.v_nproc) */

static int	procfstype;
static dev_t	procdev;
static kmutex_t	pr_mount_lock;

/*
 * /proc VFS operations vector.
 */
static int	prmount(), prunmount(), prroot(), prstatvfs();

static struct vfsops prvfsops = {
	prmount,
	prunmount,
	prroot,
	prstatvfs,
	fs_sync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_nosys,	/* swapvp */
	fs_freevfs
};

static void
prinitrootnode(prnode_t *pnp)
{
	struct vnode *vp = PTOV(pnp);

	bzero((caddr_t)pnp, sizeof (*pnp));
	mutex_init(&pnp->pr_mutex, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&vp->v_lock, NULL, MUTEX_DEFAULT, NULL);
	vp->v_flag = VROOT|VNOCACHE|VNOMAP|VNOSWAP|VNOMOUNT;
	vp->v_count = 1;
	vp->v_op = &prvnodeops;
	vp->v_type = VDIR;
	vp->v_data = (caddr_t)pnp;
	cv_init(&vp->v_cv, NULL, CV_DEFAULT, NULL);
	pnp->pr_type = PR_PROCDIR;
	pnp->pr_mode = 0555;	/* read-search by everyone */
}

static int
prinit(struct vfssw *vswp, int fstype)
{
	major_t dev;

	nproc_highbit = highbit(v.v_proc);
	procfstype = fstype;
	ASSERT(procfstype != 0);
	/*
	 * Associate VFS ops vector with this fstype.
	 */
	vswp->vsw_vfsops = &prvfsops;

	/*
	 * Assign a unique "device" number (used by stat(2)).
	 */
	if ((dev = getudev()) == (major_t)-1) {
		cmn_err(CE_WARN, "prinit: can't get unique device number");
		dev = 0;
	}
	procdev = makedevice(dev, 0);
	mutex_init(&pr_mount_lock, NULL, MUTEX_DEFAULT, NULL);

	return (0);
}

/* ARGSUSED */
static int
prmount(struct vfs *vfsp, struct vnode *mvp,
	struct mounta *uap, struct cred *cr)
{
	prnode_t *pnp;

	if (!suser(cr))
		return (EPERM);
	if (mvp->v_type != VDIR)
		return (ENOTDIR);

	pnp = kmem_alloc(sizeof (*pnp), KM_SLEEP);
	mutex_enter(&pr_mount_lock);

	mutex_enter(&mvp->v_lock);
	if ((uap->flags & MS_OVERLAY) == 0 &&
	    (mvp->v_count > 1 || (mvp->v_flag & VROOT))) {
		mutex_exit(&mvp->v_lock);
		mutex_exit(&pr_mount_lock);
		kmem_free(pnp, sizeof (*pnp));
		return (EBUSY);
	}
	mutex_exit(&mvp->v_lock);

	prinitrootnode(pnp);
	PTOV(pnp)->v_vfsp = vfsp;
	vfsp->vfs_fstype = procfstype;
	vfsp->vfs_data = (caddr_t)pnp;
	vfsp->vfs_dev = procdev;
	vfs_make_fsid(&vfsp->vfs_fsid, procdev, procfstype);
	vfsp->vfs_bsize = DEV_BSIZE;

	mutex_exit(&pr_mount_lock);
	return (0);
}

/* ARGSUSED */
static int
prunmount(struct vfs *vfsp, int flag, struct cred *cr)
{
	prnode_t *pnp = (prnode_t *)vfsp->vfs_data;
	vnode_t *vp = PTOV(pnp);

	mutex_enter(&pr_mount_lock);
	if (!suser(cr)) {
		mutex_exit(&pr_mount_lock);
		return (EPERM);
	}

	/*
	 * forced unmount is not supported by this file system
	 * and thus, ENOTSUP, is being returned.
	 */
	if (flag & MS_FORCE) {
		mutex_exit(&pr_mount_lock);
		return (ENOTSUP);
	}

	/*
	 * Ensure that no /proc vnodes are in use on this mount point.
	 */
	mutex_enter(&vp->v_lock);
	if (vp->v_count > 1) {
		mutex_exit(&vp->v_lock);
		mutex_exit(&pr_mount_lock);
		return (EBUSY);
	}

	mutex_exit(&vp->v_lock);
	mutex_exit(&pr_mount_lock);
	kmem_free(pnp, sizeof (*pnp));
	return (0);
}

/* ARGSUSED */
static int
prroot(struct vfs *vfsp, struct vnode **vpp)
{
	prnode_t *pnp = (prnode_t *)vfsp->vfs_data;
	struct vnode *vp = PTOV(pnp);

	VN_HOLD(vp);
	*vpp = vp;
	return (0);
}

static int
prstatvfs(struct vfs *vfsp, struct statvfs64 *sp)
{
	int n;
	dev32_t d32;
	extern u_int nproc;

	n = v.v_proc - nproc;

	bzero((caddr_t)sp, sizeof (*sp));
	sp->f_bsize	= DEV_BSIZE;
	sp->f_frsize	= DEV_BSIZE;
	sp->f_blocks	= (fsblkcnt64_t)0;
	sp->f_bfree	= (fsblkcnt64_t)0;
	sp->f_bavail	= (fsblkcnt64_t)0;
	sp->f_files	= (fsfilcnt64_t)v.v_proc + 2;
	sp->f_ffree	= (fsfilcnt64_t)n;
	sp->f_favail	= (fsfilcnt64_t)n;
	(void) cmpldev(&d32, vfsp->vfs_dev);
	sp->f_fsid	= d32;
	(void) strcpy(sp->f_basetype, vfssw[procfstype].vsw_name);
	sp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sp->f_namemax = 64;		/* quite arbitrary */
	bzero(sp->f_fstr, sizeof (sp->f_fstr));
	(void) strcpy(sp->f_fstr, "/proc");
	(void) strcpy(&sp->f_fstr[6], "/proc");
	return (0);
}

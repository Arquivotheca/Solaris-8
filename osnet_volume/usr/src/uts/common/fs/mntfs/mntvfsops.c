/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mntvfsops.c	1.9	99/11/16 SMI"

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
#include <sys/fs/mntdata.h>

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

static struct vfsops mntvfsops;
static int mntinit();

static struct vfssw vfw = {
	"mntfs",
	mntinit,
	&mntvfsops,
	0
};

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_fsops;

static struct modlfs modlfs = {
	&mod_fsops, "mount information file system", &vfw
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

static int	mntfstype;
static dev_t	mntdev;
static kmutex_t	mnt_mount_lock;
static int 	mnt_mounted;

/*
 * /mnttab VFS operations vector.
 */
static int	mntmount(), mntunmount(), mntroot(), mntstatvfs();

static struct vfsops mntvfsops = {
	mntmount,
	mntunmount,
	mntroot,
	mntstatvfs,
	fs_sync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_nosys,	/* swapvp */
	fs_freevfs
};

static void
mntinitrootnode(mntnode_t *mnp)
{
	struct vnode *vp = MTOV(mnp);

	bzero((caddr_t)mnp, sizeof (*mnp));
	mutex_init(&vp->v_lock, NULL, MUTEX_DEFAULT, NULL);
	vp->v_flag = VROOT|VNOCACHE|VNOMAP|VNOSWAP|VNOMOUNT;
	vp->v_count = 1;
	vp->v_op = &mntvnodeops;
	vp->v_type = VREG;
	vp->v_data = (caddr_t)mnp;
	cv_init(&vp->v_cv, NULL, CV_DEFAULT, NULL);
}

static int
mntinit(struct vfssw *vswp, int fstype)
{
	major_t dev;

	mntfstype = fstype;
	ASSERT(mntfstype != 0);
	/*
	 * Associate VFS ops vector with this fstype.
	 */
	vswp->vsw_vfsops = &mntvfsops;

	/*
	 * Assign a unique "device" number (used by stat(2)).
	 */
	if ((dev = getudev()) == (major_t)-1) {
		cmn_err(CE_WARN, "mntinit: can't get unique device number");
		dev = 0;
	}
	mntdev = makedevice(dev, 0);
	mutex_init(&mnt_mount_lock, NULL, MUTEX_DEFAULT, NULL);

	return (0);
}

/* ARGSUSED */
static int
mntmount(struct vfs *vfsp, struct vnode *mvp,
	struct mounta *uap, struct cred *cr)
{
	mntnode_t *mnp;

	if (!suser(cr))
		return (EPERM);

	/*
	 * XXX - use kmem_cache_alloc here
	 */
	mutex_enter(&mnt_mount_lock);
	if (mnt_mounted) {
		mutex_exit(&mnt_mount_lock);
		return (EBUSY);
	}
	mnt_mounted = 1;
	mutex_exit(&mnt_mount_lock);
	mnp = kmem_alloc(sizeof (*mnp), KM_SLEEP);
	mutex_enter(&mnt_mount_lock);

	mutex_enter(&mvp->v_lock);
	if ((uap->flags & MS_OVERLAY) == 0 &&
	    (mvp->v_count > 1 || (mvp->v_flag & VROOT))) {
		mutex_exit(&mvp->v_lock);
		mutex_exit(&mnt_mount_lock);
		kmem_free(mnp, sizeof (*mnp));
		return (EBUSY);
	}
	mutex_exit(&mvp->v_lock);

	mntinitrootnode(mnp);
	MTOV(mnp)->v_vfsp = vfsp;
	vfsp->vfs_fstype = mntfstype;
	vfsp->vfs_data = (caddr_t)mnp;
	vfsp->vfs_dev = mntdev;
	vfs_make_fsid(&vfsp->vfs_fsid, mntdev, mntfstype);
	vfsp->vfs_bsize = DEV_BSIZE;
	mnp->mnt_mountvp = mvp;

	mutex_exit(&mnt_mount_lock);
	return (0);
}

/* ARGSUSED */
static int
mntunmount(struct vfs *vfsp, int flag, struct cred *cr)
{
	mntnode_t *mnp = (mntnode_t *)vfsp->vfs_data;
	vnode_t *vp = MTOV(mnp);

	mutex_enter(&mnt_mount_lock);
	if (!suser(cr)) {
		mutex_exit(&mnt_mount_lock);
		return (EPERM);
	}

	/*
	 * Ensure that no /mnttab vnodes are in use on this mount point.
	 */
	mutex_enter(&vp->v_lock);
	if (vp->v_count > 1 || mnt_nopen > 0) {
		mutex_exit(&vp->v_lock);
		mutex_exit(&mnt_mount_lock);
		return (EBUSY);
	}

	mutex_exit(&vp->v_lock);
	mnt_mounted = 0;
	mutex_exit(&mnt_mount_lock);
	kmem_free(mnp, sizeof (*mnp));
	return (0);
}

/* ARGSUSED */
static int
mntroot(struct vfs *vfsp, struct vnode **vpp)
{
	mntnode_t *mnp = (mntnode_t *)vfsp->vfs_data;
	struct vnode *vp = MTOV(mnp);

	VN_HOLD(vp);
	*vpp = vp;
	return (0);
}

static int
mntstatvfs(struct vfs *vfsp, struct statvfs64 *sp)
{
	dev32_t d32;

	bzero((caddr_t)sp, sizeof (*sp));
	sp->f_bsize	= DEV_BSIZE;
	sp->f_frsize	= DEV_BSIZE;
	sp->f_blocks	= (fsblkcnt64_t)0;
	sp->f_bfree	= (fsblkcnt64_t)0;
	sp->f_bavail	= (fsblkcnt64_t)0;
	sp->f_files	= (fsfilcnt64_t)1;
	sp->f_ffree	= (fsfilcnt64_t)0;
	sp->f_favail	= (fsfilcnt64_t)0;
	(void) cmpldev(&d32, vfsp->vfs_dev);
	sp->f_fsid	= d32;
	(void) strcpy(sp->f_basetype, vfssw[mntfstype].vsw_name);
	sp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sp->f_namemax = 64;		/* quite arbitrary */
	bzero(sp->f_fstr, sizeof (sp->f_fstr));
	(void) strcpy(sp->f_fstr, "/mnttab");
	(void) strcpy(&sp->f_fstr[8], "/mnttab");
	return (0);
}

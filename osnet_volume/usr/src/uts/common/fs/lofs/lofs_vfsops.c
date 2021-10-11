/*
 * Copyright (c) 1988-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lofs_vfsops.c	1.36	99/08/07 SMI"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/pathname.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/statvfs.h>
#include <sys/fs/lofs_info.h>
#include <sys/fs/lofs_node.h>
#include <sys/mount.h>
#include <sys/mntent.h>
#include <sys/mkdev.h>
#include <sys/sysmacros.h>
#include "fs/fs_subr.h"

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

static mntopts_t lofs_mntopts;

static struct vfssw vfw = {
	"lofs",
	lofsinit,
	&lo_vfsops,
	VSW_HASPROTO,
	&lofs_mntopts
};

/*
 * LOFS mount options table
 */
static char *rw_cancel[] = { MNTOPT_RO, NULL };
static char *ro_cancel[] = { MNTOPT_RW, NULL };
static char *suid_cancel[] = { MNTOPT_NOSUID, NULL };
static char *nosuid_cancel[] = { MNTOPT_SUID, NULL };

static mntopt_t mntopts[] = {
/*
 *	option name		cancel option	default arg	flags
 *		private data
 */
	{ MNTOPT_RW,		rw_cancel,	NULL,		MO_DEFAULT,
		(void *)0 },
	{ MNTOPT_RO,		ro_cancel,	NULL,		0,
		(void *)0 },
	{ MNTOPT_SUID,		suid_cancel,	NULL,		MO_DEFAULT,
		(void *)0 },
	{ MNTOPT_NOSUID,	nosuid_cancel,	NULL,		0,
		(void *)0 }
};

static mntopts_t lofs_mntopts = {
	sizeof (mntopts) / sizeof (mntopt_t),
	mntopts
};

/*
 * Module linkage information for the kernel.
 */

static struct modlfs modlfs = {
	&mod_fsops, "filesystem for lofs", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlfs, NULL
};

/*
 * This is the module initialization routine.
 */

_init()
{
	int status;

	lofs_subrinit();
	status = mod_install(&modlinkage);
	if (status != 0) {
		/*
		 * Cleanup previously initialized work.
		 */
		lofs_subrfini();
	}

	return (status);
}

/*
 * Don't allow the lofs module to be unloaded for now.
 * There is a memory leak if it gets unloaded.
 */

_fini()
{
	return (EBUSY);
}

_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


static int lofsfstype;

int
lofsinit(struct vfssw *vswp, int fstyp)
{
	vswp->vsw_vfsops = &lo_vfsops;
	lofsfstype = fstyp;

	return (0);
}

/*
 * lo mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
/*ARGSUSED*/
static int
lo_mount(struct vfs *vfsp,
	struct vnode *vp,
	struct mounta *uap,
	struct cred *cr)
{
	int error;
	struct vnode *srootvp = NULL;	/* the server's root */
	struct vnode *realrootvp;
	struct loinfo *li;
	dev_t lofs_rdev;

	if (!suser(cr))
		return (EPERM);

	mutex_enter(&vp->v_lock);
	if (!(uap->flags & MS_OVERLAY) &&
		(vp->v_count != 1 || (vp->v_flag & VROOT))) {
		mutex_exit(&vp->v_lock);
		return (EBUSY);
	}
	mutex_exit(&vp->v_lock);

	/*
	 * Find real root, and make vfs point to real vfs
	 */
	if (error = lookupname(uap->spec, UIO_USERSPACE, FOLLOW, NULLVPP,
	    &realrootvp))
		return (error);

	/*
	 * realrootvp may be an AUTOFS node, in which case we
	 * perform a VOP_ACCESS() to trigger the mount of the
	 * intended filesystem, so we loopback mount the intended
	 * filesystem instead of the AUTOFS filesystem.
	 */
	(void) VOP_ACCESS(realrootvp, 0, 0, cr);

	/*
	 * We're interested in the top most filesystem.
	 * This is specially important when uap->spec is a trigger
	 * AUTOFS node, since we're really interested in mounting the
	 * filesystem AUTOFS mounted as result of the VOP_ACCESS()
	 * call not the AUTOFS node itself.
	 */
	if (realrootvp->v_vfsmountedhere != NULL) {
		if (error = traverse(&realrootvp)) {
			VN_RELE(realrootvp);
			return (error);
		}
	}

	/*
	 * allocate a vfs info struct and attach it
	 */
	li = (struct loinfo *)kmem_zalloc(sizeof (*li), KM_SLEEP);
	li->li_realvfs = realrootvp->v_vfsp;
	li->li_mountvfs = vfsp;

	/*
	 * Set mount flags to be inherited by loopback vfs's
	 */
	if (uap->flags & MS_RDONLY) {
		li->li_mflag |= VFS_RDONLY;
		vfs_setmntopt(&vfsp->vfs_mntopts, MNTOPT_RO, NULL, 0);
	}
	if (uap->flags & MS_NOSUID) {
		li->li_mflag |= VFS_NOSUID;
		vfs_setmntopt(&vfsp->vfs_mntopts, MNTOPT_NOSUID, NULL, 0);
	}

	/*
	 * Propagate inheritable mount flags from the real vfs.
	 */
	if (li->li_realvfs->vfs_flag & VFS_RDONLY)
		uap->flags |= MS_RDONLY;
	if (li->li_realvfs->vfs_flag & VFS_NOSUID)
		uap->flags |= MS_NOSUID;
	li->li_refct = 0;
	mutex_enter(&lofs_minor_lock);
	do {
		lofs_minor = (lofs_minor + 1) & MAXMIN32;
		lofs_rdev = makedevice(lofs_major, lofs_minor);
	} while (vfs_devismounted(lofs_rdev));
	mutex_exit(&lofs_minor_lock);
	li->li_rdev = lofs_rdev;
	vfsp->vfs_data = (caddr_t)li;
	vfsp->vfs_bcount = 0;
	vfsp->vfs_fstype = lofsfstype;
	vfsp->vfs_bsize = li->li_realvfs->vfs_bsize;
	vfsp->vfs_dev = li->li_realvfs->vfs_dev;
	vfsp->vfs_fsid.val[0] = li->li_realvfs->vfs_fsid.val[0];
	vfsp->vfs_fsid.val[1] = li->li_realvfs->vfs_fsid.val[1];

	/*
	 * Make the root vnode
	 */
	srootvp = makelonode(realrootvp, li);
	srootvp->v_flag |= VROOT;
	li->li_rootvp = srootvp;

#ifdef LODEBUG
	lo_dprint(4, "lo_mount: vfs %p realvfs %p root %p realroot %p li %p\n",
	    vfsp, li->li_realvfs, srootvp, realrootvp, li);
#endif
	return (0);
}

/*
 * Undo loopback mount
 */
static int
lo_unmount(struct vfs *vfsp, int flag, struct cred *cr)
{
	struct loinfo *li;

	if (!suser(cr))
		return (EPERM);

	/*
	 * forced unmount is not supported by this file system
	 * and thus, ENOTSUP, is being returned.
	 */
	if (flag & MS_FORCE)
		return (ENOTSUP);

	li = vtoli(vfsp);
#ifdef LODEBUG
	lo_dprint(4, "lo_unmount(%p) li %p\n", vfsp, li);
#endif
	if (li->li_refct != 1 || li->li_rootvp->v_count != 1) {
#ifdef LODEBUG
		lo_dprint(4, "refct %d v_ct %d\n", li->li_refct,
		    li->li_rootvp->v_count);
#endif
		return (EBUSY);
	}
	VN_RELE(li->li_rootvp);
	kmem_free(li, sizeof (*li));
	return (0);
}

/*
 * find root of lo
 */
static int
lo_root(struct vfs *vfsp, struct vnode **vpp)
{
	*vpp = (struct vnode *)vtoli(vfsp)->li_rootvp;
#ifdef LODEBUG
	lo_dprint(4, "lo_root(0x%p) = %p\n", vfsp, *vpp);
#endif
	VN_HOLD(*vpp);
	return (0);
}

/*
 * Get file system statistics.
 */
static int
lo_statvfs(register struct vfs *vfsp, struct statvfs64 *sbp)
{
	vnode_t *realrootvp;

#ifdef LODEBUG
	lo_dprint(4, "lostatvfs %p\n", vfsp);
#endif
	/*
	 * Using realrootvp->v_vfsp (instead of the realvfsp that was
	 * cached) is necessary to make lofs work woth forced UFS unmounts.
	 * In the case of a forced unmount, UFS stores a set of dummy vfsops
	 * in all the (i)vnodes in the filesystem. The dummy ops simply
	 * returns back EIO.
	 */
	(void) lo_realvfs(vfsp, &realrootvp);
	if (realrootvp != NULL)
		return (VFS_STATVFS(realrootvp->v_vfsp, sbp));
	else
		return (EIO);
}

/*
 * LOFS doesn't have any data or metadata to flush, pending I/O on the
 * underlying filesystem will be flushed when such filesystem is synched.
 */
/* ARGSUSED */
static int
lo_sync(struct vfs *vfsp,
	short flag,
	struct cred *cr)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_sync: %p\n", vfsp);
#endif
	return (0);
}

/*
 * Obtain the vnode from the underlying filesystem.
 */
static int
lo_vget(struct vfs *vfsp, struct vnode **vpp, struct fid *fidp)
{
	vnode_t *realrootvp;

#ifdef LODEBUG
	lo_dprint(4, "lo_vget: %p\n", vfsp);
#endif
	(void) lo_realvfs(vfsp, &realrootvp);
	if (realrootvp != NULL)
		return (VFS_VGET(realrootvp->v_vfsp, vpp, fidp));
	else
		return (EIO);
}

/*
 * lo vfs operations vector.
 */
struct vfsops lo_vfsops = {
	lo_mount,
	lo_unmount,
	lo_root,
	lo_statvfs,
	lo_sync,
	lo_vget,	/* vget */
	fs_nosys,	/* mountroot */
	fs_nosys,	/* swapvp */
	fs_freevfs
};

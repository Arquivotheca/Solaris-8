/*
 * Copyright (c) 1989-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tmp_vfsops.c	1.58	99/08/07 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/time.h>
#include <sys/pathname.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/mntent.h>
#include <fs/fs_subr.h>
#include <vm/page.h>
#include <vm/anon.h>
#include <sys/model.h>

#include <sys/fs/swapnode.h>
#include <sys/fs/tmp.h>
#include <sys/fs/tmpnode.h>

/*
 * tmpfs vfs operations.
 */
static int tmpfsinit(struct vfssw *, int);
static int tmp_mount(struct vfs *, struct vnode *,
	struct mounta *, struct cred *);
static int tmp_unmount(struct vfs *, int, struct cred *);
static int tmp_root(struct vfs *, struct vnode **);
static int tmp_statvfs(struct vfs *, struct statvfs64 *);
static int tmp_vget(struct vfs *, struct vnode **, struct fid *);

static struct vfsops tmp_vfsops = {
	tmp_mount,
	tmp_unmount,
	tmp_root,
	tmp_statvfs,
	fs_sync,
	tmp_vget,
	fs_nosys,	/* mountroot */
	fs_nosys,	/* swapvp */
	fs_freevfs
};

/*
 * Loadable module wrapper
 */
#include <sys/modctl.h>

static mntopts_t tmpfs_proto_opttbl;

static struct vfssw vfw = {
	"tmpfs",
	tmpfsinit,
	&tmp_vfsops,
	VSW_HASPROTO,
	&tmpfs_proto_opttbl
};

/*
 * in-kernel mnttab options
 */
static char *suid_cancel[] = { MNTOPT_NOSUID, NULL };
static char *nosuid_cancel[] = { MNTOPT_SUID, NULL };

static mntopt_t tmpfs_options[] = {
	/* Option name		Cancel Opt	Arg	Flags		Data */
	{ MNTOPT_RW,		NULL,		NULL,	NULL,		NULL},
	{ MNTOPT_RO,		NULL,		NULL,	NULL,		NULL},
	{ MNTOPT_SUID,		suid_cancel,	NULL,	NULL,		NULL},
	{ MNTOPT_NOSUID,	nosuid_cancel,	NULL,	NULL,		NULL},
	{ "size",		NULL,		"0",	MO_HASVALUE,	NULL}
};


static mntopts_t tmpfs_proto_opttbl = {
	sizeof (tmpfs_options) / sizeof (mntopt_t),
	tmpfs_options
};

/*
 * Module linkage information
 */
static struct modlfs modlfs = {
	&mod_fsops, "filesystem for tmpfs", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, &modlfs, NULL
};

int
_init()
{
	return (mod_install(&modlinkage));
}

int
_fini()
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * The following are patchable variables limiting the amount of system
 * resources tmpfs can use.
 *
 * tmpfs_maxkmem limits the amount of kernel kmem_alloc memory
 * tmpfs can use for it's data structures (e.g. tmpnodes, directory entries)
 * It is not determined by setting a hard limit but rather as a percentage of
 * physical memory which is determined when tmpfs is first used in the system.
 *
 * tmpfs_minfree is the minimum amount of swap space that tmpfs leaves for
 * the rest of the system.  In other words, if the amount of free swap space
 * in the system (i.e. anoninfo.ani_free) drops below tmpfs_minfree, tmpfs
 * anon allocations will fail.
 *
 * There is also a per mount limit on the amount of swap space
 * (tmount.tm_anonmax) settable via a mount option.
 */
size_t tmpfs_maxkmem = 0;
long tmpfs_minfree = 0;
size_t tmp_kmemspace;		/* bytes of kernel heap used by all tmpfs */

static int tmpfsfstype;
static dev_t tmpdev;
static uint32_t tmpfs_minor;

/*
 * initialize global tmpfs locks and such
 * called when loading tmpfs module
 */
static int
tmpfsinit(struct vfssw *vswp, int fstype)
{
	long dev;
	extern  void    tmpfs_hash_init();

	tmpfs_hash_init();
	tmpfsfstype = fstype;
	ASSERT(tmpfsfstype != 0);

	/*
	 * tmpfs_minfree doesn't need to be some function of configured
	 * swap space since it really is an absolute limit of swap space
	 * which still allows other processes to execute.
	 */
	if (tmpfs_minfree == 0) {
		/*
		 * Set if not patched
		 */
		tmpfs_minfree = btopr(TMPMINFREE);
	}

	/*
	 * The maximum amount of space tmpfs can allocate is
	 * TMPMAXPROCKMEM percent of kernel memory
	 */
	if (tmpfs_maxkmem == 0)
		tmpfs_maxkmem = MAX(PAGESIZE, kmem_maxavail() / TMPMAXFRACKMEM);

	vswp->vsw_vfsops = &tmp_vfsops;
	if ((dev = getudev()) == (major_t)-1) {
		cmn_err(CE_WARN, "tmpfsinit: Can't get unique device number.");
		dev = 0;
	}
	tmpdev = makedevice(dev, 0);
	return (0);
}

static int
tmp_mount(
	struct vfs *vfsp,
	struct vnode *mvp,
	struct mounta *uap,
	struct cred *cr)
{
	struct tmount *tm = NULL;
	struct tmpnode *tp;
	struct pathname dpn;
	int error;
	size_t anonmax;
	struct vattr rattr;
	int got_attrs;

	char *sizestr;


	if (!suser(cr))
		return (EPERM);

	if (mvp->v_type != VDIR)
		return (ENOTDIR);

	mutex_enter(&mvp->v_lock);
	if ((uap->flags & MS_OVERLAY) == 0 &&
	    (mvp->v_count != 1 || (mvp->v_flag & VROOT))) {
		mutex_exit(&mvp->v_lock);
		return (EBUSY);
	}
	mutex_exit(&mvp->v_lock);

	/*
	 * now look for options we understand...
	 */

	/* tmpfs doesn't support read-only mounts */
	if (vfs_optionisset(vfs_opttblptr(vfsp), MNTOPT_RO, NULL)) {
		error = EINVAL;
		goto out;
	}

	/*
	 * tm_anonmax is set according to the mount arguments
	 * if any.  Otherwise, it is set to a maximum value.
	 */
	if (vfs_optionisset(vfs_opttblptr(vfsp), "size", &sizestr)) {
		if ((anonmax = tmp_convnum(sizestr)) == -1) {
			error = EINVAL;
			goto out;
		}
		anonmax = btopr(anonmax);
	} else {
		anonmax = LONG_MAX;
	}


	if ((tm = tmp_memalloc(sizeof (struct tmount), 0)) == NULL) {
		error = ENOMEM;
		goto out;
	}

	if (error = pn_get(uap->dir, UIO_USERSPACE, &dpn))
		goto out;

	/*
	 * find an available minor device number for this mount
	 */
	do {
		tm->tm_dev = makedevice(tmpdev,
		    atomic_add_32_nv(&tmpfs_minor, 1));
	} while (vfs_devismounted(tm->tm_dev));

	/*
	 * Set but don't bother entering the mutex
	 * (tmount not on mount list yet)
	 */
	mutex_init(&tm->tm_contents, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&tm->tm_renamelck, NULL, MUTEX_DEFAULT, NULL);

	tm->tm_vfsp = vfsp;
	tm->tm_anonmax = anonmax;

	vfsp->vfs_data = (caddr_t)tm;
	vfsp->vfs_fstype = tmpfsfstype;
	vfsp->vfs_dev = tm->tm_dev;
	vfsp->vfs_bsize = PAGESIZE;
	vfsp->vfs_flag |= VFS_NOTRUNC;
	vfs_make_fsid(&vfsp->vfs_fsid, tm->tm_dev, tmpfsfstype);
	tm->tm_mntpath = tmp_memalloc(dpn.pn_pathlen + 1, TMP_MUSTHAVE);
	(void) strcpy(tm->tm_mntpath, dpn.pn_path);

	/*
	 * allocate and initialize root tmpnode structure
	 */
	bzero(&rattr, sizeof (struct vattr));
	rattr.va_mode = (mode_t)(S_IFDIR | 0777);	/* XXX modes */
	rattr.va_type = VDIR;
	rattr.va_rdev = 0;
	tp = tmp_memalloc(sizeof (struct tmpnode), TMP_MUSTHAVE);
	tmpnode_init(tm, tp, &rattr, cr);

	/*
	 * Get the mode, uid, and gid from the underlying mount point.
	 */
	rattr.va_mask = AT_MODE|AT_UID|AT_GID;	/* Hint to getattr */
	got_attrs = VOP_GETATTR(mvp, &rattr, 0, cr);

	rw_enter(&tp->tn_rwlock, RW_WRITER);
	TNTOV(tp)->v_flag |= VROOT;

	/*
	 * If the getattr succeeded, use its results.  Otherwise allow
	 * the previously set hardwired defaults to prevail.
	 */
	if (got_attrs == 0) {
		tp->tn_mode = rattr.va_mode;
		tp->tn_uid = rattr.va_uid;
		tp->tn_gid = rattr.va_gid;
	}

	/*
	 * initialize linked list of tmpnodes so that the back pointer of
	 * the root tmpnode always points to the last one on the list
	 * and the forward pointer of the last node is null
	 */
	tp->tn_back = tp;
	tp->tn_forw = NULL;
	tp->tn_nlink = 0;
	tm->tm_rootnode = tp;

	tdirinit(tp, tp);

	rw_exit(&tp->tn_rwlock);

	pn_free(&dpn);
	error = 0;

out:
	return (error);
}

static int
tmp_unmount(struct vfs *vfsp, int flag, struct cred *cr)
{
	struct tmount *tm = (struct tmount *)VFSTOTM(vfsp);
	struct tmpnode *tnp;

	if (!suser(cr))
		return (EPERM);

	/*
	 * forced unmount is not supported by this file system
	 * and thus, ENOTSUP, is being returned.
	 */
	if (flag & MS_FORCE)
		return (ENOTSUP);

	mutex_enter(&tm->tm_contents);

	/*
	 * Don't close down the tmpfs if there are open files.
	 * There should be only one file referenced (the rootnode)
	 * and only one reference to the vnode for that file.
	 */
	tnp = tm->tm_rootnode;
	if (TNTOV(tnp)->v_count > 1) {
		mutex_exit(&tm->tm_contents);
		return (EBUSY);
	}

	for (tnp = tnp->tn_forw; tnp; tnp = tnp->tn_forw) {
		if (TNTOV(tnp)->v_count > 0) {
			mutex_exit(&tm->tm_contents);
			return (EBUSY);
		}
	}

	/*
	 * We can drop the mutex now because no one can find this mount
	 */
	mutex_exit(&tm->tm_contents);

	/*
	 * Free all kmemalloc'd and anonalloc'd memory associated with
	 * this filesystem.  To do this, we go through the file list twice,
	 * once to remove all the directory entries, and then to remove
	 * all the files.  We do this because there is useful code in
	 * tmpnode_free which assumes that the directory entry has been
	 * removed before the file.
	 */
	/*
	 * Remove all directory entries
	 */
	for (tnp = tm->tm_rootnode; tnp; tnp = tnp->tn_forw) {
		rw_enter(&tnp->tn_rwlock, RW_WRITER);
		if (tnp->tn_type == VDIR)
			tdirtrunc(tnp);
		rw_exit(&tnp->tn_rwlock);
	}

	ASSERT(tm->tm_rootnode);

	/*
	 * We re-acquire the lock to prevent others who have a HOLD on
	 * a tmpnode via its pages or anon slots from blowing it away
	 * (in tmp_inactive) while we're trying to get to it here. Once
	 * we have a HOLD on it we know it'll stick around.
	 */
	mutex_enter(&tm->tm_contents);
	/*
	 * Remove all the files (except the rootnode) backwards.
	 */
	while ((tnp = tm->tm_rootnode->tn_back) != tm->tm_rootnode) {
		/*
		 * Blow the tmpnode away by HOLDing it and RELE'ing it.
		 * The RELE calls inactive and blows it away because there
		 * we have the last HOLD.
		 */
		VN_HOLD(TNTOV(tnp));
		mutex_exit(&tm->tm_contents);
		VN_RELE(TNTOV(tnp));
		mutex_enter(&tm->tm_contents);
		/*
		 * It's still there after the RELE. Someone else like pageout
		 * has a hold on it so wait a bit and then try again - we know
		 * they'll give it up soon.
		 */
		if (tnp == tm->tm_rootnode->tn_back) {
			mutex_exit(&tm->tm_contents);
			delay(hz / 4);
			mutex_enter(&tm->tm_contents);
		}
	}
	mutex_exit(&tm->tm_contents);

	VN_RELE(TNTOV(tm->tm_rootnode));

	ASSERT(tm->tm_mntpath);

	tmp_memfree(tm->tm_mntpath, strlen(tm->tm_mntpath) + 1);

	ASSERT(tm->tm_anonmem == 0);

	mutex_destroy(&tm->tm_contents);
	mutex_destroy(&tm->tm_renamelck);
	tmp_memfree(tm, sizeof (struct tmount));

	return (0);
}

/*
 * return root tmpnode for given vnode
 */
static int
tmp_root(struct vfs *vfsp, struct vnode **vpp)
{
	struct tmount *tm = (struct tmount *)VFSTOTM(vfsp);
	struct tmpnode *tp = tm->tm_rootnode;
	struct vnode *vp;

	ASSERT(tp);

	vp = TNTOV(tp);
	VN_HOLD(vp);
	*vpp = vp;
	return (0);
}

static int
tmp_statvfs(struct vfs *vfsp, struct statvfs64 *sbp)
{
	struct tmount	*tm = (struct tmount *)VFSTOTM(vfsp);
	long	blocks;
	dev32_t d32;

	sbp->f_bsize = PAGESIZE;
	sbp->f_frsize = PAGESIZE;

	/*
	 * Find the amount of available physical and memory swap
	 */
	mutex_enter(&anoninfo_lock);
	ASSERT(k_anoninfo.ani_max >= k_anoninfo.ani_phys_resv);
	blocks = CURRENT_TOTAL_AVAILABLE_SWAP;
	mutex_exit(&anoninfo_lock);

	/*
	 * If tm_anonmax for this mount is less than the available swap space
	 * (minus the amount tmpfs can't use), use that instead
	 */
	sbp->f_bfree = (fsblkcnt64_t)(MIN(MAX(blocks - (long)tmpfs_minfree, 0),
	    (long)(tm->tm_anonmax - tm->tm_anonmem)));
	sbp->f_bavail = (fsblkcnt64_t)(sbp->f_bfree);

	/*
	 * Total number of blocks is what's available plus what's been used
	 */
	sbp->f_blocks = (fsblkcnt64_t)(sbp->f_bfree + tm->tm_anonmem);

	/*
	 * The maximum number of files available is approximately the number
	 * of tmpnodes we can allocate from the remaining kernel memory
	 * available to tmpfs.  This is fairly inaccurate since it doesn't
	 * take into account the names stored in the directory entries.
	 */
	sbp->f_ffree = (fsfilcnt64_t)
	    (MAX(((long)tmpfs_maxkmem - (long)tmp_kmemspace) /
	    (long)(sizeof (struct tmpnode) + sizeof (struct tdirent)), 0));
	sbp->f_files = tmpfs_maxkmem /
	    (sizeof (struct tmpnode) + sizeof (struct tdirent));
	sbp->f_favail = (fsfilcnt64_t)(sbp->f_ffree);
	(void) cmpldev(&d32, vfsp->vfs_dev);
	sbp->f_fsid = d32;
	(void) strcpy(sbp->f_basetype, vfssw[tmpfsfstype].vsw_name);
	(void) strcpy(sbp->f_fstr, tm->tm_mntpath);
	sbp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sbp->f_namemax = MAXNAMELEN - 1;
	return (0);
}

static int
tmp_vget(struct vfs *vfsp, struct vnode **vpp, struct fid *fidp)
{
	register struct tfid *tfid;
	register struct tmount *tm = (struct tmount *)VFSTOTM(vfsp);
	register struct tmpnode *tp = NULL;

	tfid = (struct tfid *)fidp;
	*vpp = NULL;

	mutex_enter(&tm->tm_contents);
	for (tp = tm->tm_rootnode; tp; tp = tp->tn_forw) {
		mutex_enter(&tp->tn_tlock);
		if (tp->tn_nodeid == tfid->tfid_ino) {
			/*
			 * If the gen numbers don't match we know the
			 * file won't be found since only one tmpnode
			 * can have this number at a time.
			 */
			if (tp->tn_gen != tfid->tfid_gen || tp->tn_nlink == 0) {
				mutex_exit(&tp->tn_tlock);
				mutex_exit(&tm->tm_contents);
				return (0);
			}
			*vpp = (struct vnode *)TNTOV(tp);

			VN_HOLD(*vpp);

			if ((tp->tn_mode & S_ISVTX) &&
			    !(tp->tn_mode & (S_IXUSR | S_IFDIR))) {
				mutex_enter(&(*vpp)->v_lock);
				(*vpp)->v_flag |= VISSWAP;
				mutex_exit(&(*vpp)->v_lock);
			}
			mutex_exit(&tp->tn_tlock);
			mutex_exit(&tm->tm_contents);
			return (0);
		}
		mutex_exit(&tp->tn_tlock);
	}
	mutex_exit(&tm->tm_contents);
	return (0);
}

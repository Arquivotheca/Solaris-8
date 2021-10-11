/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986-1989,1993,1997-1998 by Sun Microsystems, Inc.
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)s5_vfsops.c	1.17	99/04/15 SMI"
/* from s5_vfsops.c	2.90	92/12/02 SMI */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/buf.h>
#include <sys/pathname.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/kstat.h>
#include <sys/fs/s5_fsdir.h>
#include <sys/fs/s5_fs.h>
#include <sys/fs/s5_inode.h>
#include <sys/fs/s5_mount.h>
#undef NFS
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/swap.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include "fs/fs_subr.h"
#include <sys/cmn_err.h>
#include <sys/modctl.h>
#include <sys/dnlc.h>

extern struct vfsops s5_vfsops;
extern int s5init();

static int mountfs(struct vfs *vfsp, enum whymountroot why, dev_t dev,
	char *path, struct cred *cr, int isroot, struct s5_args *argsp);

static struct vfssw vfw = {
	"s5fs",
	s5init,
	&s5_vfsops,
	0
};

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_fsops;

static struct modlfs modlfs = {
	&mod_fsops,
	"filesystem for s5",
	&vfw
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modlfs,
	NULL
};

char _depends_on[] = "fs/specfs";

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

extern int s5fstype;

/*
 * s5 vfs operations.
 */
static int s5_mount(struct vfs *vfsp, struct vnode *mvp,
    struct mounta *uap, struct cred *cr);
static int s5_unmount(struct vfs *vfsp, int, struct cred *cr);
static int s5_root(struct vfs *vfsp, struct vnode **vpp);
static int s5_statvfs(struct vfs *vfsp, struct statvfs64 *sp);
static int s5_sync(struct vfs *vfsp, short flag, struct cred *cr);
static int s5_mountroot(struct vfs *vfsp, enum whymountroot why);
void s5_sbupdate(struct vfs *vfsp);
static void s5vfs_init(struct s5vfs *s5vfsp, int bsize);

struct vfsops s5_vfsops = {
	s5_mount,
	s5_unmount,
	s5_root,
	s5_statvfs,
	s5_sync,
	fs_nosys,	/* vget */
	s5_mountroot,
	fs_nosys,	/* swapvp */
	fs_freevfs
};

kmutex_t	s5_syncbusy;	/* initialized in s5_init() */

extern int s5fstype;

/*
 * XXX - this appears only to be used by the VM code to handle the case where
 * UNIX is running off the mini-root.  That probably wants to be done
 * differently.
 */
struct vnode *rootvp;

static int
s5_mount(struct vfs *vfsp, struct vnode *mvp,
    struct mounta *uap, struct cred *cr)
{
	char *data = uap->dataptr;
	int datalen = uap->datalen;
	dev_t dev;
	struct vnode *bvp;
	struct pathname dpn;
	int error;
	enum whymountroot why;
	struct s5_args args;
	struct s5_args *argsp;
	int fromspace = (uap->flags & MS_SYSSPACE) ?
	    UIO_SYSSPACE : UIO_USERSPACE;

	if (!suser(cr))
		return (EPERM);

	if (mvp->v_type != VDIR)
		return (ENOTDIR);

	mutex_enter(&mvp->v_lock);
	if ((uap->flags & MS_REMOUNT) == 0 &&
		(mvp->v_count != 1 || (mvp->v_flag & VROOT))) {
			mutex_exit(&mvp->v_lock);
			return (EBUSY);
	}
	mutex_exit(&mvp->v_lock);

	/*
	 * Get arguments
	 */
	if (data && datalen) {
		int copy_result = 0;
		if (datalen != sizeof (args))
			return (EINVAL);
		if (uap->flags & MS_SYSSPACE) {
			bcopy(data, &args, sizeof (args));
		} else {
			copy_result = copyin(data, &args, sizeof (args));
		}
		if (copy_result)
			return (EFAULT);
		if (args.flags & ~S5MNT_NOINTR)
			return (EINVAL);
		argsp = &args;
	} else
		argsp = NULL;

	/* XXX why don't we just use mvp? */
	if (error = pn_get(uap->dir, fromspace, &dpn))
		return (error);

	/*
	 * Resolve path name of special file being mounted.
	 */
	if (error = lookupname(uap->spec, fromspace, FOLLOW, NULLVPP,
	    &bvp)) {
		pn_free(&dpn);
		return (error);
	}
	if (bvp->v_type != VBLK) {
		VN_RELE(bvp);
		pn_free(&dpn);
		return (ENOTBLK);
	}
	dev = bvp->v_rdev;
	VN_RELE(bvp);
	/*
	 * Ensure that this device isn't already mounted,
	 * unless this is a REMOUNT request
	 */
	if (vfs_devismounted(dev)) {
		if (uap->flags & MS_REMOUNT)
			why = ROOT_REMOUNT;
		else {
			pn_free(&dpn);
			return (EBUSY);
		}
	} else {
		why = ROOT_INIT;
	}
	if (getmajor(dev) >= devcnt) {
		pn_free(&dpn);
		return (ENXIO);
	}

	/*
	 * If the device is a tape, mount it read only
	 */
	if (devopsp[getmajor(dev)]->devo_cb_ops->cb_flag & D_TAPE)
		vfsp->vfs_flag |= VFS_RDONLY;

	if (uap->flags & MS_RDONLY)
		vfsp->vfs_flag |= VFS_RDONLY;

	/*
	 * Mount the filesystem.
	 */
	error = mountfs(vfsp, why, dev, dpn.pn_path, cr, 0, argsp);
	pn_free(&dpn);
	return (error);
}

/*
 * Mount root file system.
 * "why" is ROOT_INIT on initial call, ROOT_REMOUNT if called to
 * remount the root file system, and ROOT_UNMOUNT if called to
 * unmount the root (e.g., as part of a system shutdown).
 *
 * XXX - this may be partially machine-dependent; it, along with the VFS_SWAPVP
 * operation, goes along with auto-configuration.  A mechanism should be
 * provided by which machine-INdependent code in the kernel can say "get me the
 * right root file system" and "get me the right initial swap area", and have
 * that done in what may well be a machine-dependent fashion.
 * Unfortunately, it is also file-system-type dependent (NFS gets it via
 * bootparams calls, S5 gets it from various and sundry machine-dependent
 * mechanisms, as SPECFS does for swap).
 */
static int
s5_mountroot(struct vfs *vfsp, enum whymountroot why)
{
	struct filsys *fsp;
	int error;
	static int s5rootdone = 0;
	dev_t rootdev;
	extern dev_t getrootdev();
	struct buf *bp;
	struct vnode *vp;
	int ovflags;
	struct s5vfs *s5vfsp = (struct s5vfs *)vfsp->vfs_data;

	if (why == ROOT_INIT) {
		if (s5rootdone++)
			return (EBUSY);
		rootdev = getrootdev();
		if (rootdev == (dev_t)NODEV)
			return (ENODEV);
		vfsp->vfs_dev = rootdev;
		vfsp->vfs_flag |= VFS_RDONLY;
	} else if (why == ROOT_REMOUNT) {
		vp = s5vfsp->vfs_devvp;
		(void) dnlc_purge_vfsp(vfsp, 0);
		(void) VOP_PUTPAGE(vp, (offset_t)0, (u_int)0, B_INVAL, CRED());
		(void) s5_iflush(vfsp);
		binval(vfsp->vfs_dev);
		ovflags = vfsp->vfs_flag;
		vfsp->vfs_flag &= ~VFS_RDONLY;
		vfsp->vfs_flag |= VFS_REMOUNT;
		rootdev = vfsp->vfs_dev;
	} else if (why == ROOT_UNMOUNT) {
		s5_update(0);
		fsp = getfs(vfsp);
		bp = s5vfsp->vfs_bufp;
		if (fsp->s_state == FsACTIVE) {
			vp = ((struct s5vfs *)vfsp->vfs_data)->vfs_devvp;
			s5_sbupdate(vfsp);
			(void) VOP_CLOSE(vp, FREAD|FWRITE, 1,
			    (offset_t)0, CRED());
			VN_RELE(vp);
		}
		bp->b_flags |= B_AGE;
		brelse(bp);	/* free the superblock buf */
		return (0);
	}
	error = vfs_lock(vfsp);
	if (error)
		return (error);
	error = mountfs(vfsp, why, rootdev, "/", CRED(), 1, NULL);
	/*
	 * XXX - assumes root device is not indirect, because we don't set
	 * rootvp.  Is rootvp used for anything?  If so, make another arg
	 * to mountfs (in S5 case too?)
	 */
	if (error) {
		vfs_unlock(vfsp);
		if (why == ROOT_REMOUNT)
			vfsp->vfs_flag = ovflags;
		if (rootvp) {
			VN_RELE(rootvp);
			rootvp = (struct vnode *)0;
		}
		return (error);
	}
	if (why == ROOT_INIT)
		vfs_add((struct vnode *)0, vfsp,
		    (vfsp->vfs_flag & VFS_RDONLY) ? MS_RDONLY : 0);
	vfs_unlock(vfsp);
	fsp = getfs(vfsp);
	clkset(fsp->s_time);
	return (0);
}

static int
mountfs(struct vfs *vfsp, enum whymountroot why, dev_t dev, char *path,
    struct cred *cr, int isroot, struct s5_args *argsp)
{
	struct vnode *devvp = 0;
	struct filsys *fsp, *fspt;
	struct s5vfs *s5vfsp = 0;
	struct buf *bp = 0;
	struct buf *tp = 0;
	struct buf *tpt = 0;
	int error = 0;
	int needclose = 0;
	struct inode *rip;
	struct vnode *rvp;
	struct ulockfs *ulp;

#ifdef lint
	path = path;
#endif

	if (why == ROOT_INIT) {
		/*
		 * Open the device.
		 */
		devvp = makespecvp(dev, VBLK);

		/*
		 * Open block device mounted on.
		 * When bio is fixed for vnodes this can all be vnode
		 * operations.
		 */
		error = VOP_OPEN(&devvp,
		    (vfsp->vfs_flag & VFS_RDONLY) ? FREAD : FREAD|FWRITE, cr);
		if (error)
			return (error);
		needclose = 1;

		/*
		 * Refuse to go any further if this
		 * device is being used for swapping.
		 */
		if (IS_SWAPVP(devvp)) {
			error = EBUSY;
			goto out;
		}
	}

	/*
	 * check for dev already mounted on
	 */
	if (vfsp->vfs_flag & VFS_REMOUNT) {
		/* cannot remount to RDONLY */
		if (vfsp->vfs_flag & VFS_RDONLY)
			return (EINVAL);

		if (vfsp->vfs_dev != dev)
			return (EINVAL);

		s5vfsp = (struct s5vfs *)vfsp->vfs_data;
		ulp = &s5vfsp->vfs_ulockfs;
		bp = s5vfsp->vfs_bufp;
		fsp = (struct filsys *)bp->b_un.b_addr;
		devvp = s5vfsp->vfs_devvp;

		/*
		 * fsck may have altered the file system; discard
		 * as much incore data as possible.  Don't flush
		 * if this is a rw to rw remount; it's just resetting
		 * the options.
		 */
		if (fsp->s_ronly) {
			(void) dnlc_purge_vfsp(vfsp, 0);
			(void) VOP_PUTPAGE(devvp, (offset_t)0, (u_int)0,
					B_INVAL, CRED());
			(void) s5_iflush(vfsp);
			bflush(dev);
			binval(dev);
		}

		/*
		 * synchronize w/s5 ioctls
		 */
		mutex_enter(&ulp->ul_lock);

		/*
		 * reset options
		 */
		s5vfsp->vfs_nointr = (argsp && argsp->flags & S5MNT_NOINTR);
		cv_broadcast(&ulp->ul_vnops_cnt_cv);

		/*
		 * read/write to read/write; all done
		 */
		if (fsp->s_ronly == 0)
			goto remountout;

		/*
		 * fsck may have updated the superblock so wait for I/O
		 * rundown and read in the superblock again
		 */
		tpt = bread(vfsp->vfs_dev, SBLOCK, SBSIZE);
		if (tpt->b_flags & B_ERROR) {
			error = EIO;
			goto remountout;
		}
		fspt = (struct filsys *)tpt->b_un.b_addr;

		if (fspt->s_magic != FsMAGIC ||
			FsBSIZE(fspt->s_bshift) > MAXBSIZE) {
			tpt->b_flags |= B_STALE | B_AGE;
			error = EINVAL;
			goto remountout;
		}

		if ((fspt->s_state + fspt->s_time) == FsOKAY) {
			/* switch in the new superblock */
			bcopy(tpt->b_un.b_addr,	bp->b_un.b_addr, SBSIZE);
			fsp->s_state = FsACTIVE;
		} /* superblock updated in memory */
		tpt->b_flags |= B_STALE | B_AGE;
		brelse(tpt);
		tpt = 0;

		if (fsp->s_state != FsACTIVE) {
			error = ENOSPC;
			goto remountout;
		}

		tp = ngeteblk(SBSIZE);
		tp->b_edev = dev;
		tp->b_dev = cmpdev(dev);
		tp->b_blkno = SBLOCK;
		tp->b_bcount = SBSIZE;

		/*
		 * superblock gets flushed immediately, no need to bother
		 * s5_update
		 */
		fsp->s_fmod = 0;
		fsp->s_ronly = 0;

		bcopy(fsp, tp->b_un.b_addr, SBSIZE);
		bwrite(tp);
remountout:
		if (tpt)
			brelse(tpt);
		mutex_exit(&ulp->ul_lock);
		return (error);
	}
	ASSERT(devvp != 0);

	/*
	 * Flush back any dirty pages on the block device to
	 * try and keep the buffer cache in sync with the page
	 * cache if someone is trying to use block devices when
	 * they really should be using the raw device.
	 */
	(void) VOP_PUTPAGE(devvp, (offset_t)0, (u_int)0, B_INVAL, cr);

	/*
	 * read in superblock
	 */

	tp = bread(dev, SBLOCK, SBSIZE);
	if (tp->b_flags & B_ERROR) {
		goto out;
	}
	fsp = (struct filsys *)tp->b_un.b_addr;
	if (fsp->s_magic != FsMAGIC ||
	    FsBSIZE(fsp->s_bshift) > MAXBSIZE) {
		error = EINVAL;	/* also needs translation */
		goto out;
	}
	/*
	 * Allocate VFS private data.
	 */
	s5vfsp = kmem_zalloc(sizeof (struct s5vfs), KM_SLEEP);

	vfsp->vfs_bcount = 0;
	vfsp->vfs_data = (caddr_t)s5vfsp;
	vfsp->vfs_fstype = s5fstype;
	vfsp->vfs_dev = dev;
	vfsp->vfs_flag |= VFS_NOTRUNC;
	vfs_make_fsid(&vfsp->vfs_fsid, dev, s5fstype);
	s5vfsp->vfs_devvp = devvp;

	/*
	 * Cross-link with vfs and add to instance list.
	 */
	s5vfsp->vfs_vfs = vfsp;
	s5_vfs_add(s5vfsp);

	/*
	 * Copy the super block into a buffer in its native size.
	 * Use ngeteblk to allocate the buffer
	 */
	bp = ngeteblk((int)SBSIZE);
	s5vfsp->vfs_bufp = bp;
	bp->b_edev = dev;
	bp->b_dev = cmpdev(dev);
	bp->b_blkno = SBLOCK;
	bp->b_bcount = SBSIZE;
	bcopy(tp->b_un.b_addr, bp->b_un.b_addr, SBSIZE);
	tp->b_flags |= B_STALE | B_AGE;
	brelse(tp);
	tp = 0;

	fsp = (struct filsys *)bp->b_un.b_addr;
	if (vfsp->vfs_flag & VFS_RDONLY) {
		fsp->s_ronly = 1;
		fsp->s_fmod = 0;
		if ((fsp->s_state + fsp->s_time) == FsOKAY)
			fsp->s_state = FsACTIVE;
		else
			fsp->s_state = FsBAD;
	} else {

		/*
		 * superblock gets flushed immediately, no need to bother
		 * s5_update
		 */
		fsp->s_fmod = 0;
		fsp->s_ronly = 0;

		if ((fsp->s_state + fsp->s_time) == FsOKAY)
			fsp->s_state = FsACTIVE;
		else {
			if (isroot)
				/*
				 * allow root partition to be mounted even
				 * when s_state is not ok
				 * will be fixed later by a remount root
				 */
				fsp->s_state = FsBAD;
			else {
				error = ENOSPC;
				goto out;
			}
		}

		tp = ngeteblk(SBSIZE);
		tp->b_edev = dev;
		tp->b_dev = cmpdev(dev);
		tp->b_blkno = SBLOCK;
		tp->b_bcount = SBSIZE;
		bcopy(fsp, tp->b_un.b_addr, SBSIZE);
		bwrite(tp);
		tp = 0;

	}

	vfsp->vfs_bsize = (u_int)FsBSIZE(fsp->s_bshift);
	s5vfs_init(s5vfsp, vfsp->vfs_bsize);

	mutex_init(&s5vfsp->vfs_lock, NULL, MUTEX_DEFAULT, NULL);

	/*
	 * Initialize lockfs structure to support file system locking
	 */
	s5vfsp->vfs_ulockfs.ul_vfsp = vfsp;
	mutex_init(&s5vfsp->vfs_ulockfs.ul_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&s5vfsp->vfs_ulockfs.ul_vnops_cnt_cv, NULL, CV_DEFAULT, NULL);

	if (why == ROOT_INIT) {
		if (isroot)
			rootvp = devvp;
	}
	if (error = s5_iget(vfsp, fsp, S5ROOTINO, &rip, cr))
		goto out;
	rvp = ITOV(rip);
	mutex_enter(&rvp->v_lock);
	rvp->v_flag |= VROOT;
	mutex_exit(&rvp->v_lock);
	s5vfsp->vfs_root = rvp;

	/* options */
	s5vfsp->vfs_nointr = (argsp && argsp->flags & S5MNT_NOINTR);

	return (0);

out:
	if (error == 0)
		error = EIO;
	if (bp) {
		bp->b_flags |= B_AGE;
		brelse(bp);
	}
	if (tp) {
		tp->b_flags |= B_AGE;
		brelse(tp);
	}
	if (s5vfsp) {
		s5_vfs_remove(s5vfsp);
		kmem_free(s5vfsp, sizeof (struct s5vfs));
	}

	if (needclose) {
		(void) VOP_CLOSE(devvp, (vfsp->vfs_flag & VFS_RDONLY) ?
			FREAD : FREAD|FWRITE, 1, (offset_t)0, cr);
		binval(dev);
	}
	VN_RELE(devvp);
	return (error);
}

/*
 * vfs operations
 */
static int
s5_unmount(struct vfs *vfsp, int fflag, struct cred *cr)
{
	dev_t dev = vfsp->vfs_dev;
	struct filsys *fs;
	struct s5vfs *s5vfsp = (struct s5vfs *)vfsp->vfs_data;
	struct vnode *bvp, *rvp;
	struct inode *rip;
	struct buf *bp;
	struct ulockfs *ulp;
	int flag;

	if (!suser(cr))
		return (EPERM);

	/*
	 * forced unmount is not supported by this file system
	 * and thus, ENOTSUP, is being returned.
	 */
	if (fflag & MS_FORCE)
		return (ENOTSUP);

	ulp = &s5vfsp->vfs_ulockfs;
	mutex_enter(&ulp->ul_lock);

	/* check if file system is busy */
	if (ulp->ul_vnops_cnt) {
		mutex_exit(&ulp->ul_lock);
		return (EBUSY);
	}

	mutex_exit(&ulp->ul_lock);

	rvp = s5vfsp->vfs_root;
	ASSERT(rvp != NULL);
	rip = VTOI(rvp);

	if (s5_iflush(vfsp) < 0)
		return (EBUSY);

	/* Flush root inode to disk */
	rw_enter(&rip->i_contents, RW_WRITER);
	(void) s5_syncip(rip, B_INVAL, I_SYNC);
	rw_exit(&rip->i_contents);

	fs = getfs(vfsp);
	bp = s5vfsp->vfs_bufp;
	bvp = s5vfsp->vfs_devvp;
	flag = !fs->s_ronly;
	mutex_enter(&s5_syncbusy);
	if (!fs->s_ronly) {
		bflush(dev);
		s5_sbupdate(vfsp);
	}
	bp->b_flags |= B_AGE;
	brelse(bp);			/* free the superblock buf */

	mutex_exit(&s5_syncbusy);
	(void) VOP_PUTPAGE(bvp, (offset_t)0, (u_int)0, B_INVAL, cr);
	(void) VOP_CLOSE(bvp, flag, 1, (offset_t)0, cr);
	binval(dev);
	VN_RELE(bvp);
	s5_delcache(rip);
	s5_iput(rip);

	/*
	 * Remove from instance list.
	 */
	s5_vfs_remove(s5vfsp);

	kmem_free(s5vfsp, sizeof (struct s5vfs));
	return (0);
}

static int
s5_root(struct vfs *vfsp, struct vnode **vpp)
{
	struct s5vfs *s5vfsp = (struct s5vfs *)vfsp->vfs_data;
	struct vnode *vp = s5vfsp->vfs_root;

	VN_HOLD(vp);
	*vpp = vp;
	return (0);
}

/*
 * Get file system statistics.
 */
static int
s5_statvfs(struct vfs *vfsp, struct statvfs64 *sp)
{
	struct filsys *fp;
	int i;
	char *cp;
	dev32_t d32;

	if ((fp = getfs(vfsp))->s_magic != FsMAGIC)
		return (EINVAL);

	bzero((caddr_t)sp, sizeof (*sp));
	sp->f_bsize = sp->f_frsize = vfsp->vfs_bsize;
	sp->f_blocks = (fsblkcnt64_t)fp->s_fsize;
	sp->f_bfree = sp->f_bavail = (fsblkcnt64_t)fp->s_tfree;
	sp->f_files = (fsfilcnt64_t)(fp->s_isize - 2) *
		((struct s5vfs *)vfsp->vfs_data)->vfs_inopb;
	sp->f_ffree = sp->f_favail = (fsfilcnt64_t)fp->s_tinode;
	(void) cmpldev(&d32, vfsp->vfs_dev);
	sp->f_fsid = d32;
	(void) strcpy(sp->f_basetype, vfssw[vfsp->vfs_fstype].vsw_name);
	sp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sp->f_namemax = DIRSIZ;
	cp = &sp->f_fstr[0];
	for (i = 0; i < sizeof (fp->s_fname) && fp->s_fname[i] != '\0';
	    i++, cp++)
		*cp = fp->s_fname[i];
	*cp++ = '\0';
	for (i = 0; i < sizeof (fp->s_fpack) && fp->s_fpack[i] != '\0';
	    i++, cp++)
		*cp = fp->s_fpack[i];
	*cp = '\0';

	return (0);
}

/*
 * Flush any pending I/O to file system vfsp.
 * The s5_update() routine will only flush *all* s5 files.
 */
/*ARGSUSED*/
static int
s5_sync(struct vfs *vfsp, short flag, struct cred *cr)
{
	s5_update(flag);
	return (0);
}

void
s5_sbupdate(struct vfs *vfsp)
{
	struct s5vfs *s5vfsp = (struct s5vfs *)vfsp->vfs_data;
	mutex_enter(&s5vfsp->vfs_lock);
	s5_sbwrite(s5vfsp);
	mutex_exit(&s5vfsp->vfs_lock);
}

static void
s5vfs_init(struct s5vfs *s5vfsp, int bsize)
{
	int i;

	for (i = bsize, s5vfsp->vfs_bshift = 0; i > 1; i >>= 1)
		s5vfsp->vfs_bshift++;
	s5vfsp->vfs_nindir = bsize / sizeof (daddr_t);
	s5vfsp->vfs_inopb = bsize / sizeof (struct dinode);
	s5vfsp->vfs_bsize = bsize;
	s5vfsp->vfs_bmask = bsize - 1;
	s5vfsp->vfs_nmask = s5vfsp->vfs_nindir - 1;
	for (i = bsize/512, s5vfsp->vfs_ltop = 0; i > 1; i >>= 1)
		s5vfsp->vfs_ltop++;
	for (i = s5vfsp->vfs_nindir, s5vfsp->vfs_nshift = 0; i > 1; i >>= 1)
		s5vfsp->vfs_nshift++;
	for (i = s5vfsp->vfs_inopb, s5vfsp->vfs_inoshift = 0; i > 1; i >>= 1)
		s5vfsp->vfs_inoshift++;
}

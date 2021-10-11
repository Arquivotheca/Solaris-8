/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ufs_lockfs.c	1.69	99/04/15 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/mman.h>
#include <sys/pathname.h>
#include <sys/debug.h>
#include <sys/vmmeter.h>
#include <sys/vmsystm.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/acct.h>
#include <sys/dnlc.h>
#include <sys/swap.h>

#include <sys/fs/ufs_fs.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_fsdir.h>
#include <sys/fs/ufs_trans.h>
#include <sys/fs/ufs_panic.h>
#include <sys/fs/ufs_mount.h>
#include <sys/fs/ufs_bio.h>
#include <sys/fs/ufs_log.h>
#include <sys/fs/ufs_quota.h>
#include <sys/dirent.h>		/* must be AFTER <sys/fs/fsdir.h>! */
#include <sys/errno.h>
#include <sys/sysinfo.h>

#include <vm/hat.h>
#include <vm/pvn.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/rm.h>
#include <vm/anon.h>
#include <sys/swap.h>
#include <sys/dnlc.h>

extern struct vnode *common_specvp(struct vnode *vp);

/* error lock status */
#define	UN_ERRLCK	(-1)
#define	SET_ERRLCK	1
#define	RE_ERRLCK	2
#define	NO_ERRLCK	0

/*
 * Validate lockfs request
 */
static int
ufs_getlfd(
	struct lockfs *lockfsp,		/* new lock request */
	struct lockfs *ul_lockfsp)	/* old lock state */
{
	int	error = 0;

	/*
	 * no input flags defined
	 */
	if (lockfsp->lf_flags != 0) {
		error = EINVAL;
		goto errout;
	}

	/*
	 * check key
	 */
	if (!LOCKFS_IS_ULOCK(ul_lockfsp))
		if (lockfsp->lf_key != ul_lockfsp->lf_key) {
			error = EINVAL;
			goto errout;
	}

	lockfsp->lf_key = ul_lockfsp->lf_key + 1;

errout:
	return (error);
}

#ifdef	SYSACCT
/*
 * ufs_checkaccton
 *	check if accounting is turned on on this fs
 */
extern struct vnode	*acctvp;

ufs_checkaccton(struct vnode *vp)
{
	if (acctvp && acctvp->v_vfsp == vp->v_vfsp)
		return (EDEADLK);
	return (0);
}
#endif	/* SYSACCT */

/*
 * ufs_checkswapon
 *	check if local swapping is to file on this fs
 */
ufs_checkswapon(struct vnode *vp)
{
	struct swapinfo	*sip;

	mutex_enter(&swapinfo_lock);
	for (sip = swapinfo; sip; sip = sip->si_next)
		if (sip->si_vp->v_vfsp == vp->v_vfsp) {
			mutex_exit(&swapinfo_lock);
			return (EDEADLK);
		}
	mutex_exit(&swapinfo_lock);
	return (0);
}

/*
 * ufs_freeze
 *	pend future accesses for current lock and desired lock
 */
void
ufs_freeze(struct ulockfs *ulp, struct lockfs *lockfsp)
{
	/*
	 * set to new lock type
	 */
	ulp->ul_lockfs.lf_lock = lockfsp->lf_lock;
	ulp->ul_lockfs.lf_key = lockfsp->lf_key;
	ulp->ul_lockfs.lf_comlen = lockfsp->lf_comlen;
	ulp->ul_lockfs.lf_comment = lockfsp->lf_comment;

	ulp->ul_fs_lock = (1 << ulp->ul_lockfs.lf_lock);
}

/*
 * ufs_unfreeze
 *	lock failed, reset the old lock
 */
void
ufs_unfreeze(struct ulockfs *ulp, struct lockfs *lockfsp)
{
	uint_t	comlen;
	caddr_t	comment;

	/*
	 * can't unfreeze a hlock
	 */
	if (LOCKFS_IS_HLOCK(&ulp->ul_lockfs)) {
		/*
		 * free up comment from reset lock
		 */
		comlen  = (uint_t)lockfsp->lf_comlen;
		comment = lockfsp->lf_comment;

		lockfsp->lf_comlen = 0;
		lockfsp->lf_comment = NULL;
		goto out;

	} else {
		/*
		 * free up comment from failed lock
		 */
		comlen  = (uint_t)ulp->ul_lockfs.lf_comlen;
		comment = ulp->ul_lockfs.lf_comment;

		ulp->ul_lockfs.lf_comlen = 0;
		ulp->ul_lockfs.lf_comment = NULL;
	}

	/*
	 * reset lock
	 */
	bcopy(lockfsp, &ulp->ul_lockfs, sizeof (struct lockfs));

	ulp->ul_fs_lock = (1 << lockfsp->lf_lock);
out:
	if (comment && comlen != 0)
		kmem_free(comment, comlen);
}
/*
 * ufs_quiesce
 *	wait for outstanding accesses to finish
 */
int
ufs_quiesce(struct ulockfs *ulp)
{
	int error = 0;

	/*
	 * Set a softlock to suspend future ufs_vnops so that
	 * this lockfs request will not be starved
	 */
	ULOCKFS_SET_SLOCK(ulp);

	/* check if there is any outstanding ufs vnodeops calls */
	while (ulp->ul_vnops_cnt)
		if (!cv_wait_sig(&ulp->ul_cv, &ulp->ul_lock)) {
			error = EINTR;
			goto out;
		}

out:
	/*
	 * unlock the soft lock
	 */
	ULOCKFS_CLR_SLOCK(ulp);

	return (error);
}
/*
 * ufs_flush_inode
 */
int
ufs_flush_inode(struct inode *ip, void *arg)
{
	int	error;
	int	saverror	= 0;

	/*
	 * wrong file system; keep looking
	 */
	if (ip->i_ufsvfs != (struct ufsvfs *)arg)
		return (0);

	/*
	 * asynchronously push all the dirty pages
	 */
	if (error = TRANS_SYNCIP(ip, B_ASYNC, 0, TOP_SYNCIP_FLUSHI))
		saverror = error;
	/*
	 * wait for io and discard all mappings
	 */
	if (error = TRANS_SYNCIP(ip, B_INVAL, 0, TOP_SYNCIP_FLUSHI))
		saverror = error;

	return (saverror);
}

/*
 * ufs_flush
 *	Flush everything that is currently dirty; this includes invalidating
 *	any mappings.
 */
int
ufs_flush(struct vfs *vfsp)
{
	int		error;
	int		saverror = 0;
	struct ufsvfs	*ufsvfsp	= (struct ufsvfs *)vfsp->vfs_data;
	struct fs	*fs		= ufsvfsp->vfs_fs;

	ASSERT(vfs_lock_held(vfsp));

	/*
	 * purge dnlc
	 */
	(void) dnlc_purge_vfsp(vfsp, 0);

	/*
	 * drain the delete and idle threads
	 */
	ufs_delete_drain(vfsp, 0, 0);
	ufs_idle_drain(vfsp);

	/*
	 * flush and invalidate quota records
	 */
	(void) qsync(ufsvfsp);

	/*
	 * flush w/invalidate the inodes for vfsp
	 */
	if (error = ufs_scan_inodes(0, ufs_flush_inode, ufsvfsp))
		saverror = error;

	/*
	 * synchronously flush superblock and summary info
	 */
	if (fs->fs_ronly == 0 && fs->fs_fmod) {
		fs->fs_fmod = 0;
		TRANS_SBUPDATE(ufsvfsp, vfsp, TOP_SBUPDATE_FLUSH);
	}
	/*
	 * flush w/invalidate block device pages and buf cache
	 */
	if ((error = VOP_PUTPAGE(common_specvp(ufsvfsp->vfs_devvp),
	    (offset_t)0, 0, B_INVAL, CRED())) > 0)
		saverror = error;

	(void) bflush((dev_t)vfsp->vfs_dev);
	(void) bfinval((dev_t)vfsp->vfs_dev, 0);

	/*
	 * drain the delete and idle threads again
	 */
	ufs_delete_drain(vfsp, 0, 0);
	ufs_idle_drain(vfsp);

	/*
	 * play with the clean flag
	 */
	if (saverror == 0)
		ufs_checkclean(vfsp);

	/*
	 * flush any outstanding transactions
	 */
	curthread->t_flag |= T_DONTBLOCK;
	TRANS_BEGIN_SYNC(ufsvfsp, TOP_COMMIT_FLUSH, TOP_COMMIT_SIZE);
	TRANS_END_SYNC(ufsvfsp, saverror, TOP_COMMIT_FLUSH, TOP_COMMIT_SIZE);
	curthread->t_flag &= ~T_DONTBLOCK;

	LUFS_EMPTY(ufsvfsp);

	return (saverror);
}

/*
 * ufs_thaw_wlock
 *	special processing when thawing down to wlock
 */
static int
ufs_thaw_wlock(struct inode *ip, void *arg)
{
	/*
	 * wrong file system; keep looking
	 */
	if (ip->i_ufsvfs != (struct ufsvfs *)arg)
		return (0);

	/*
	 * iupdat refuses to clear flags if the fs is read only.  The fs
	 * may become read/write during the lock and we wouldn't want
	 * these inodes being written to disk.  So clear the flags.
	 */
	rw_enter(&ip->i_contents, RW_WRITER);
	ip->i_flag &= ~(IMOD|IMODACC|IACC|IUPD|ICHG|IATTCHG);
	rw_exit(&ip->i_contents);

	/*
	 * pages are mlocked -- fail wlock
	 */
	if (ITOV(ip)->v_type != VCHR && ITOV(ip)->v_pages)
		return (EPERM);

	return (0);
}

/*
 * ufs_thaw_hlock
 *	special processing when thawing down to hlock or elock
 */
static int
ufs_thaw_hlock(struct inode *ip, void *arg)
{
	struct vnode	*vp	= ITOV(ip);

	/*
	 * wrong file system; keep looking
	 */
	if (ip->i_ufsvfs != (struct ufsvfs *)arg)
		return (0);

	/*
	 * blow away all pages - even if they are mlocked
	 */
	do {
		(void) TRANS_SYNCIP(ip, B_INVAL | B_FORCE, 0, TOP_SYNCIP_HLOCK);
	} while ((vp->v_type != VCHR) && vp->v_pages);
	rw_enter(&ip->i_contents, RW_WRITER);
	ip->i_flag &= ~(IMOD|IMODACC|IACC|IUPD|ICHG|IATTCHG);
	rw_exit(&ip->i_contents);

	return (0);
}

/*
 * ufs_thaw
 *	thaw file system lock down to current value
 */
int
ufs_thaw(struct vfs *vfsp, struct ufsvfs *ufsvfsp, struct ulockfs *ulp)
{
	int		error	= 0;
	int		noidel	= (int)(ulp->ul_flag & ULOCKFS_NOIDEL);

	/*
	 * if wlock or hlock or elock
	 */
	if (ULOCKFS_IS_WLOCK(ulp) || ULOCKFS_IS_HLOCK(ulp) ||
	    ULOCKFS_IS_ELOCK(ulp)) {

		/*
		 * don't keep access times
		 * don't free deleted files
		 * if superblock writes are allowed, limit them to me for now
		 */
		ulp->ul_flag |= (ULOCKFS_NOIACC|ULOCKFS_NOIDEL);
		if (ulp->ul_sbowner != (kthread_id_t)-1)
			ulp->ul_sbowner = curthread;

		/*
		 * wait for writes for deleted files and superblock updates
		 */
		(void) ufs_flush(vfsp);

		/*
		 * now make sure the quota file is up-to-date
		 *	expensive; but effective
		 */
		error = ufs_flush(vfsp);
		/*
		 * no one can write the superblock
		 */
		ulp->ul_sbowner = (kthread_id_t)-1;

		/*
		 * special processing for wlock/hlock/elock
		 */
		if (ULOCKFS_IS_WLOCK(ulp)) {
			if (error)
				goto errout;
			error = bfinval(ufsvfsp->vfs_dev, 0);
			if (error)
				goto errout;
			error = ufs_scan_inodes(0, ufs_thaw_wlock,
					(void *)ufsvfsp);
			if (error)
				goto errout;
		}
		if (ULOCKFS_IS_HLOCK(ulp) || ULOCKFS_IS_ELOCK(ulp)) {
			error = 0;
			(void) ufs_scan_inodes(0, ufs_thaw_hlock,
					(void *)ufsvfsp);
			(void) bfinval(ufsvfsp->vfs_dev, 1);
		}
	} else {

		/*
		 * okay to keep access times
		 * okay to free deleted files
		 * okay to write the superblock
		 */
		ulp->ul_flag &= ~(ULOCKFS_NOIACC|ULOCKFS_NOIDEL);
		ulp->ul_sbowner = NULL;

		/*
		 * flush in case deleted files are in memory
		 */
		if (noidel) {
			if (error = ufs_flush(vfsp))
				goto errout;
		}
	}

errout:
	cv_broadcast(&ulp->ul_cv);
	return (error);
}

/*
 * ufs_reconcile_fs
 *	reconcile incore superblock with ondisk superblock
 */
int
ufs_reconcile_fs(struct vfs *vfsp, struct ufsvfs *ufsvfsp, int errlck)
{
	struct fs	*mfs; 	/* in-memory superblock */
	struct fs	*dfs;	/* on-disk   superblock */
	struct buf	*bp;	/* on-disk   superblock buf */
	int		 needs_unlock;
	char		 finished_fsclean;

	mfs = ufsvfsp->vfs_fs;

	/*
	 * get the on-disk copy of the superblock
	 */
	bp = UFS_BREAD(ufsvfsp, vfsp->vfs_dev, SBLOCK, SBSIZE);
	bp->b_flags |= (B_STALE|B_AGE);
	if (bp->b_flags & B_ERROR) {
		brelse(bp);
		return (EIO);
	}
	dfs = bp->b_un.b_fs;

	/* error locks may only unlock after the fs has been made consistent */
	if (errlck == UN_ERRLCK) {
		if (dfs->fs_clean == FSFIX) {	/* being repaired */
			brelse(bp);
			return (EAGAIN);
		}
		/* repair not yet started? */
		finished_fsclean = TRANS_ISTRANS(ufsvfsp)? FSLOG: FSCLEAN;
		if (dfs->fs_clean != finished_fsclean) {
			brelse(bp);
			return (EBUSY);
		}
	}

	/*
	 * if superblock has changed too much, abort
	 */
	if ((mfs->fs_sblkno		!= dfs->fs_sblkno) ||
	    (mfs->fs_cblkno		!= dfs->fs_cblkno) ||
	    (mfs->fs_iblkno		!= dfs->fs_iblkno) ||
	    (mfs->fs_dblkno		!= dfs->fs_dblkno) ||
	    (mfs->fs_cgoffset		!= dfs->fs_cgoffset) ||
	    (mfs->fs_cgmask		!= dfs->fs_cgmask) ||
	    (mfs->fs_bsize		!= dfs->fs_bsize) ||
	    (mfs->fs_fsize		!= dfs->fs_fsize) ||
	    (mfs->fs_frag		!= dfs->fs_frag) ||
	    (mfs->fs_bmask		!= dfs->fs_bmask) ||
	    (mfs->fs_fmask		!= dfs->fs_fmask) ||
	    (mfs->fs_bshift		!= dfs->fs_bshift) ||
	    (mfs->fs_fshift		!= dfs->fs_fshift) ||
	    (mfs->fs_fragshift		!= dfs->fs_fragshift) ||
	    (mfs->fs_fsbtodb		!= dfs->fs_fsbtodb) ||
	    (mfs->fs_sbsize		!= dfs->fs_sbsize) ||
	    (mfs->fs_nindir		!= dfs->fs_nindir) ||
	    (mfs->fs_nspf		!= dfs->fs_nspf) ||
	    (mfs->fs_trackskew		!= dfs->fs_trackskew) ||
	    (mfs->fs_cgsize		!= dfs->fs_cgsize) ||
	    (mfs->fs_ntrak		!= dfs->fs_ntrak) ||
	    (mfs->fs_nsect		!= dfs->fs_nsect) ||
	    (mfs->fs_spc		!= dfs->fs_spc) ||
	    (mfs->fs_cpg		!= dfs->fs_cpg) ||
	    (mfs->fs_ipg		!= dfs->fs_ipg) ||
	    (mfs->fs_fpg		!= dfs->fs_fpg) ||
	    (mfs->fs_postblformat	!= dfs->fs_postblformat) ||
	    (mfs->fs_magic		!= dfs->fs_magic)) {
		brelse(bp);
		return (EACCES);
	}
	if (dfs->fs_clean == FSBAD || FSOKAY != dfs->fs_state + dfs->fs_time)
		if (mfs->fs_clean == FSLOG) {
			brelse(bp);
			return (EACCES);
		}

	/*
	 * get new summary info
	 */
	if (ufs_getsummaryinfo(vfsp->vfs_dev, ufsvfsp, dfs)) {
		brelse(bp);
		return (EIO);
	}

	/*
	 * release old summary info and update in-memory superblock
	 */
	kmem_free(mfs->fs_u.fs_csp, mfs->fs_cssize);
	mfs->fs_u.fs_csp = dfs->fs_u.fs_csp;	/* Only entry 0 used */

	/*
	 * update fields allowed to change
	 */
	mfs->fs_size		= dfs->fs_size;
	mfs->fs_dsize		= dfs->fs_dsize;
	mfs->fs_ncg		= dfs->fs_ncg;
	mfs->fs_minfree		= dfs->fs_minfree;
	mfs->fs_rotdelay	= dfs->fs_rotdelay;
	mfs->fs_rps		= dfs->fs_rps;
	mfs->fs_maxcontig	= dfs->fs_maxcontig;
	mfs->fs_maxbpg		= dfs->fs_maxbpg;
	mfs->fs_csmask		= dfs->fs_csmask;
	mfs->fs_csshift		= dfs->fs_csshift;
	mfs->fs_optim		= dfs->fs_optim;
	mfs->fs_csaddr		= dfs->fs_csaddr;
	mfs->fs_cssize		= dfs->fs_cssize;
	mfs->fs_ncyl		= dfs->fs_ncyl;
	mfs->fs_cstotal		= dfs->fs_cstotal;
	mfs->fs_reclaim		= dfs->fs_reclaim;

	/* XXX What to do about sparecon? */

	/* XXX need to copy volume label */

	/*
	 * ondisk clean flag overrides inmemory clean flag iff == FSBAD
	 * or if error-locked and ondisk is now clean
	 */
	needs_unlock = !MUTEX_HELD(&ufsvfsp->vfs_lock);
	if (needs_unlock)
		mutex_enter(&ufsvfsp->vfs_lock);

	if (errlck == UN_ERRLCK) {
		if (finished_fsclean == dfs->fs_clean)
			mfs->fs_clean = finished_fsclean;
		else
			mfs->fs_clean = FSBAD;
		mfs->fs_state = FSOKAY - dfs->fs_time;
	}

	if (FSOKAY != dfs->fs_state + dfs->fs_time ||
	    (dfs->fs_clean == FSBAD))
		mfs->fs_clean = FSBAD;

	if (needs_unlock)
		mutex_exit(&ufsvfsp->vfs_lock);

	brelse(bp);

	return (0);
}

/*
 * ufs_reconcile_inode
 *	reconcile ondisk inode with incore inode
 */
static int
ufs_reconcile_inode(struct inode *ip, void *arg)
{
	int		i;
	int		ndaddr;
	int		niaddr;
	struct dinode	*dp;		/* ondisk inode */
	struct buf	*bp	= NULL;
	uid_t		d_uid;
	gid_t		d_gid;
	int		error = 0;
	struct fs	*fs;

	/*
	 * not an inode we care about
	 */
	if (ip->i_ufsvfs != (struct ufsvfs *)arg)
		return (0);

	/*
	 * BIG BOO-BOO, reconciliation fails
	 */
	if (ip->i_flag & (IMOD|IMODACC|IACC|IUPD|ICHG|IATTCHG))
		return (EPERM);

	/*
	 * get the dinode
	 */
	fs = ip->i_fs;
	bp = UFS_BREAD(ip->i_ufsvfs,
			ip->i_dev, (daddr_t)fsbtodb(fs, itod(fs, ip->i_number)),
	    (int)fs->fs_bsize);
	if (bp->b_flags & B_ERROR) {
		brelse(bp);
		return (EIO);
	}
	dp  = bp->b_un.b_dino;
	dp += itoo(fs, ip->i_number);

	/*
	 * handle Sun's implementation of EFT
	 */
	d_uid = (dp->di_suid == UID_LONG) ? dp->di_uid : (uid_t)dp->di_suid;
	d_gid = (dp->di_sgid == GID_LONG) ? dp->di_gid : (uid_t)dp->di_sgid;

	/*
	 * some fields are not allowed to change
	 */
	if ((ip->i_mode  != dp->di_mode) ||
	    (ip->i_gen   != dp->di_gen) ||
	    (ip->i_uid   != d_uid) ||
	    (ip->i_gid   != d_gid)) {
		error = EACCES;
		goto out;
	}

	/*
	 * and some are allowed to change
	 */
	ip->i_size		= dp->di_size;
	ip->i_ic.ic_flags	= dp->di_ic.ic_flags;
	ip->i_blocks		= dp->di_blocks;
	ip->i_nlink		= dp->di_nlink;
	if (ip->i_flag & IFASTSYMLNK) {
		ndaddr = 1;
		niaddr = 0;
	} else {
		ndaddr = NDADDR;
		niaddr = NIADDR;
	}
	for (i = 0; i < ndaddr; ++i)
		ip->i_db[i] = dp->di_db[i];
	for (i = 0; i < niaddr; ++i)
		ip->i_ib[i] = dp->di_ib[i];

out:
	brelse(bp);
	return (error);
}

/*
 * ufs_reconcile
 *	reconcile ondisk superblock/inodes with any incore
 */
static int
ufs_reconcile(struct vfs *vfsp, struct ufsvfs *ufsvfsp, int errlck)
{
	int	error = 0;

	/*
	 * get rid of as much inmemory data as possible
	 */
	(void) ufs_flush(vfsp);

	/*
	 * reconcile the superblock and inodes
	 */
	if (error = ufs_reconcile_fs(vfsp, ufsvfsp, errlck))
		return (error);
	if (error = ufs_scan_inodes(0, ufs_reconcile_inode, ufsvfsp))
		return (error);
	/*
	 * allocation blocks may be incorrect; get rid of them
	 */
	(void) ufs_flush(vfsp);

	return (error);
}

/*
 * File system locking
 */
int
ufs_fiolfs(struct vnode *vp, struct lockfs *lockfsp, int from_log)
{
	return (ufs__fiolfs(vp, lockfsp, /* from_user */ 1, from_log));
}

/* kernel-internal interface, also used by fix-on-panic */
int
ufs__fiolfs(
	struct vnode *vp,
	struct lockfs *lockfsp,
	int from_user,
	int from_log)
{
	struct ulockfs	*ulp;
	struct lockfs	lfs;
	int		error;
	struct vfs	*vfsp;
	struct ufsvfs	*ufsvfsp;
	int		 errlck		= NO_ERRLCK;
	int		 poll_events	= POLLPRI;
	extern struct pollhead ufs_pollhd;

	/* check valid lock type */
	if (!lockfsp || lockfsp->lf_lock > LOCKFS_MAXLOCK)
		return (EINVAL);

	if (!vp || !vp->v_vfsp || !vp->v_vfsp->vfs_data)
		return (EIO);

	vfsp = vp->v_vfsp;
	ufsvfsp = (struct ufsvfs *)vfsp->vfs_data;
	ulp = &ufsvfsp->vfs_ulockfs;

	/*
	 * suspend the delete thread
	 *	this must be done outside the lockfs locking protocol
	 */
	ufs_thread_suspend(&ufsvfsp->vfs_delete);

	/*
	 * Acquire vfs_reflock around ul_lock to avoid deadlock with
	 * umount/remount/sync.
	 */
	vfs_lock_wait(vfsp);
	mutex_enter(&ulp->ul_lock);

	/*
	 * Quit if there is another lockfs request in progress
	 * that is waiting for existing ufs_vnops to complete.
	 */
	if (ULOCKFS_IS_BUSY(ulp)) {
		error = EBUSY;
		goto errexit;
	}

	/* cannot ulocked or downgrade a hard-lock */
	if (ULOCKFS_IS_HLOCK(ulp)) {
		error = EIO;
		goto errexit;
	}

	/* an error lock may be unlocked or relocked, only */
	if (ULOCKFS_IS_ELOCK(ulp)) {
		if (!LOCKFS_IS_ULOCK(lockfsp) && !LOCKFS_IS_ELOCK(lockfsp)) {
			error = EBUSY;
			goto errexit;
		}
	}

	/*
	 * a read-only error lock may only be upgraded to an
	 * error lock or hard lock
	 */
	if (ULOCKFS_IS_ROELOCK(ulp)) {
		if (!LOCKFS_IS_HLOCK(lockfsp) && !LOCKFS_IS_ELOCK(lockfsp)) {
			error = EBUSY;
			goto errexit;
		}
	}

	/*
	 * until read-only error locks are fully implemented
	 * just return EINVAL
	 */
	if (LOCKFS_IS_ROELOCK(lockfsp)) {
		error = EINVAL;
		goto errexit;
	}

	/*
	 * an error lock may only be applied if the file system is
	 * unlocked or already error locked.
	 * (this is to prevent the case where a fs gets changed out from
	 * underneath a fs that is locked for backup,
	 * that is, name/delete/write-locked.)
	 */
	if ((!ULOCKFS_IS_ULOCK(ulp) && !ULOCKFS_IS_ELOCK(ulp) &&
	    !ULOCKFS_IS_ROELOCK(ulp)) &&
	    (LOCKFS_IS_ELOCK(lockfsp) || LOCKFS_IS_ROELOCK(lockfsp))) {
		error = EBUSY;
		goto errexit;
	}

	/* get and validate the input lockfs request */
	if (error = ufs_getlfd(lockfsp, &ulp->ul_lockfs))
		goto errexit;

	/*
	 * save current ulockfs struct
	 */
	bcopy(&ulp->ul_lockfs, &lfs, sizeof (struct lockfs));

	/*
	 * Freeze the file system (pend future accesses)
	 */
	ufs_freeze(ulp, lockfsp);

	/*
	 * Set locking in progress because ufs_quiesce may free the
	 * ul_lock mutex.
	 */
	ULOCKFS_SET_BUSY(ulp);
	/* update the ioctl copy */
	LOCKFS_SET_BUSY(&ulp->ul_lockfs);

	/*
	 * Quiesce (wait for outstanding accesses to finish)
	 */
	if (error = ufs_quiesce(ulp))
		goto errout;

	/*
	 * can't wlock or (ro)elock fs with accounting or local swap file
	 */
	if ((ULOCKFS_IS_WLOCK(ulp) || ULOCKFS_IS_ELOCK(ulp) ||
	    ULOCKFS_IS_ROELOCK(ulp)) && !from_log) {
#ifdef	SYSACCT
		if (error = ufs_checkaccton(vp))
			goto errout;
#endif	SYSACCT
		if (error = ufs_checkswapon(vp))
			goto errout;
	}

	/*
	 * save error lock status to pass down to reconcilation
	 * routines and for later cleanup
	 */
	if (LOCKFS_IS_ELOCK(&lfs) && ULOCKFS_IS_ULOCK(ulp))
		errlck = UN_ERRLCK;

	if (ULOCKFS_IS_ELOCK(ulp) || ULOCKFS_IS_ROELOCK(ulp)) {
		int needs_unlock;
		int needs_sbwrite;

		poll_events |= POLLERR;
		errlck = LOCKFS_IS_ELOCK(&lfs) || LOCKFS_IS_ROELOCK(&lfs)?
							RE_ERRLCK: SET_ERRLCK;


		needs_unlock = !MUTEX_HELD(&ufsvfsp->vfs_lock);
		if (needs_unlock)
			mutex_enter(&ufsvfsp->vfs_lock);

		/* disable delayed i/o */
		needs_sbwrite = 0;

		if (errlck == SET_ERRLCK) {
			ufsvfsp->vfs_fs->fs_clean = FSBAD;
			needs_sbwrite = 1;
		}

		needs_sbwrite |= ufsvfsp->vfs_dio;
		ufsvfsp->vfs_dio = 0;

		if (needs_unlock)
			mutex_exit(&ufsvfsp->vfs_lock);

		if (needs_sbwrite) {
			ulp->ul_sbowner = curthread;
			TRANS_SBWRITE(ufsvfsp, TOP_SBWRITE_STABLE);

			if (needs_unlock)
				mutex_enter(&ufsvfsp->vfs_lock);

			ufsvfsp->vfs_fs->fs_fmod = 0;

			if (needs_unlock)
				mutex_exit(&ufsvfsp->vfs_lock);
		}
	}

	/*
	 * reconcile superblock and inodes if was wlocked
	 */
	if (LOCKFS_IS_WLOCK(&lfs) || LOCKFS_IS_ELOCK(&lfs)) {
		if (error = ufs_reconcile(vfsp, ufsvfsp, errlck))
			goto errout;
		/*
		 * in case the fs grew; reset the metadata map for harpy tests
		 */
		TRANS_MATA_UMOUNT(ufsvfsp);
		TRANS_MATA_MOUNT(ufsvfsp);
		TRANS_MATA_SI(ufsvfsp, ufsvfsp->vfs_fs);
	}

	/*
	 * At least everything *currently* dirty goes out.
	 */

	if ((error = ufs_flush(vfsp)) != 0 && !ULOCKFS_IS_HLOCK(ulp) &&
	    !ULOCKFS_IS_ELOCK(ulp))
		goto errout;

	/*
	 * thaw file system and wakeup pended processes
	 */
	if (error = ufs_thaw(vfsp, ufsvfsp, ulp))
		if (!ULOCKFS_IS_HLOCK(ulp) && !ULOCKFS_IS_ELOCK(ulp))
			goto errout;

	/*
	 * reset modified flag if not already write locked
	 */
	if (!LOCKFS_IS_WLOCK(&lfs))
		ULOCKFS_CLR_MOD(ulp);

	/*
	 * idle the lock struct
	 */
	ULOCKFS_CLR_BUSY(ulp);
	/* update the ioctl copy */
	LOCKFS_CLR_BUSY(&ulp->ul_lockfs);

	/*
	 * free current comment
	 */
	if (lfs.lf_comment && lfs.lf_comlen != 0) {
		kmem_free(lfs.lf_comment, lfs.lf_comlen);
		lfs.lf_comment = NULL;
		lfs.lf_comlen = 0;
	}

	/* do error lock cleanup */
	if (errlck == UN_ERRLCK)
		ufsfx_unlockfs(ufsvfsp);

	else if (errlck == RE_ERRLCK)
		ufsfx_lockfs(ufsvfsp);

	/* don't allow error lock from user to invoke panic */
	else if (from_user && errlck == SET_ERRLCK &&
		!(ufsvfsp->vfs_fsfx.fx_flags & (UFSMNT_ONERROR_PANIC >> 4)))
		(void) ufs_fault(ufsvfsp->vfs_root,
		    ulp->ul_lockfs.lf_comment && ulp->ul_lockfs.lf_comlen > 0 ?
		    ulp->ul_lockfs.lf_comment: "user-applied error lock");

	mutex_exit(&ulp->ul_lock);
	vfs_unlock(vfsp);

	if (ULOCKFS_IS_HLOCK(&ufsvfsp->vfs_ulockfs))
		poll_events |= POLLERR;

	pollwakeup(&ufs_pollhd, poll_events);

	/*
	 * allow the delete thread to continue
	 */
	ufs_thread_continue(&ufsvfsp->vfs_delete);

	return (0);

errout:
	/*
	 * if possible, apply original lock and clean up lock things
	 */
	ufs_unfreeze(ulp, &lfs);
	(void) ufs_thaw(vfsp, ufsvfsp, ulp);
	ULOCKFS_CLR_BUSY(ulp);
	LOCKFS_CLR_BUSY(&ulp->ul_lockfs);

errexit:
	mutex_exit(&ulp->ul_lock);
	vfs_unlock(vfsp);

	/*
	 * allow the delete thread to continue
	 */
	ufs_thread_continue(&ufsvfsp->vfs_delete);

	return (error);
}

/*
 * fiolfss
 * 	return the current file system locking state info
 */
int
ufs_fiolfss(struct vnode *vp, struct lockfs *lockfsp)
{
	struct ulockfs	*ulp;

	if (!vp || !vp->v_vfsp || !VTOI(vp))
		return (EINVAL);

	/* file system has been forcibly unmounted */
	if (VTOI(vp)->i_ufsvfs == NULL)
		return (EIO);

	ulp = VTOUL(vp);

	if (ULOCKFS_IS_HLOCK(ulp)) {
		*lockfsp = ulp->ul_lockfs;	/* structure assignment */
		return (0);
	}

	mutex_enter(&ulp->ul_lock);

	*lockfsp = ulp->ul_lockfs;	/* structure assignment */

	if (ULOCKFS_IS_MOD(ulp))
		lockfsp->lf_flags |= LOCKFS_MOD;

	mutex_exit(&ulp->ul_lock);

	return (0);
}

/*
 * ufs_check_lockfs
 *	check whether a ufs_vnops conflicts with the file system lock
 */
int
ufs_check_lockfs(struct ufsvfs *ufsvfsp, struct ulockfs *ulp, ulong_t mask)
{
	k_sigset_t	smask;
	int		sig, slock;

	ASSERT(MUTEX_HELD(&ulp->ul_lock));

	while (ulp->ul_fs_lock & mask) {
		slock = (int)ULOCKFS_IS_SLOCK(ulp);
		if ((curthread->t_flag & T_DONTPEND) && !slock) {
			curthread->t_flag |= T_WOULDBLOCK;
			return (EAGAIN);
		}
		curthread->t_flag &= ~T_WOULDBLOCK;

		if (ULOCKFS_IS_HLOCK(ulp))
			return (EIO);

		/*
		 * wait for lock status to change
		 *	maintain ul_wait_cnt for forcible unmount
		 */
		if (slock || ufsvfsp->vfs_nointr) {
			cv_wait(&ulp->ul_cv, &ulp->ul_lock);
		} else {
			sigintr(&smask, 1);
			sig = cv_wait_sig(&ulp->ul_cv, &ulp->ul_lock);
			sigunintr(&smask);
			if ((!sig && (ulp->ul_fs_lock & mask)) ||
				ufsvfsp->vfs_dontblock)
				return (EINTR);
		}
	}
	ulp->ul_vnops_cnt++;
	return (0);
}

/*
 * ufs_lockfs_begin - start the lockfs locking protocol
 */
int
ufs_lockfs_begin(struct ufsvfs *ufsvfsp, struct ulockfs **ulpp, ulong_t mask)
{
	int 		error;
	struct ulockfs *ulp;

	/*
	 * file system has been forcibly unmounted
	 */
	if (ufsvfsp == NULL)
		return (EIO);

	/*
	 * recursive VOP call
	 */
	if (curthread->t_flag & T_DONTBLOCK) {
		*ulpp = NULL;
		return (0);
	}

	/*
	 * Do lockfs protocol
	 */
	*ulpp = ulp = &ufsvfsp->vfs_ulockfs;
	mutex_enter(&ulp->ul_lock);
	if (ULOCKFS_IS_JUSTULOCK(ulp))
		ulp->ul_vnops_cnt++;
	else {
		if (error = ufs_check_lockfs(ufsvfsp, ulp, mask)) {
			mutex_exit(&ulp->ul_lock);
			return (error);
		}
	}
	mutex_exit(&ulp->ul_lock);

	curthread->t_flag |= T_DONTBLOCK;
	return (0);
}

/*
 * ufs_lockfs_end - terminate the lockfs locking protocol
 */
void
ufs_lockfs_end(struct ulockfs *ulp)
{

	/*
	 * end-of-VOP protocol
	 */
	if (ulp == NULL)
		return;
	curthread->t_flag &= ~T_DONTBLOCK;
	mutex_enter(&ulp->ul_lock);
	if (--ulp->ul_vnops_cnt == 0)
		cv_broadcast(&ulp->ul_cv);
	mutex_exit(&ulp->ul_lock);
}

/*
 * specialized version of ufs_lockfs_begin() called by ufs_getpage().
 */
int
ufs_lockfs_begin_getpage(
	struct ufsvfs	*ufsvfsp,
	struct ulockfs	**ulpp,
	struct seg	*seg,
	int		read_access,
	uint_t		*protp)
{
	ulong_t			mask;
	int 			error;
	struct ulockfs		*ulp	= &ufsvfsp->vfs_ulockfs;

	/*
	 * file system has been forcibly unmounted
	 */
	if (ufsvfsp == NULL)
		return (EIO);

	/*
	 * recursive VOP call; ignore
	 */
	if (curthread->t_flag & T_DONTBLOCK) {
		*ulpp = NULL;
		return (0);
	}

	/*
	 * Do lockfs protocol
	 */
	*ulpp = ulp = &ufsvfsp->vfs_ulockfs;
	mutex_enter(&ulp->ul_lock);
	if (ULOCKFS_IS_JUSTULOCK(ulp))
		/*
		 * fs is not locked, simply inc the active-ops counter
		 */
		ulp->ul_vnops_cnt++;
	else {
		if (seg->s_ops == &segvn_ops &&
		    ((struct segvn_data *)seg->s_data)->type != MAP_SHARED) {
			mask = (ulong_t)ULOCKFS_GETREAD_MASK;
		} else if (protp && read_access) {
			/*
			 * Restrict the mapping to readonly.
			 * Writes to this mapping will cause
			 * another fault which will then
			 * be suspended if fs is write locked
			 */
			*protp &= ~PROT_WRITE;
			mask = (ulong_t)ULOCKFS_GETREAD_MASK;
		} else
			mask = (ulong_t)ULOCKFS_GETWRITE_MASK;

		/*
		 * will sleep if this fs is locked against this VOP
		 */
		if (error = ufs_check_lockfs(ufsvfsp, ulp, mask)) {
			mutex_exit(&ulp->ul_lock);
			return (error);
		}
	}
	mutex_exit(&ulp->ul_lock);

	curthread->t_flag |= T_DONTBLOCK;
	return (0);
}

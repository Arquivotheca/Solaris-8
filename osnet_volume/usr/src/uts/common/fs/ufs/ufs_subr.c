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
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 *
 */

/*
 * Copyright (c) 1986-1989,1996-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)ufs_subr.c	2.125	99/11/05 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/fs/ufs_fs.h>
#include <sys/cmn_err.h>

#ifdef _KERNEL

#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/user.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/debug.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_trans.h>
#include <sys/fs/ufs_panic.h>
#include <sys/fs/ufs_bio.h>
#include <sys/fs/ufs_log.h>
#include <sys/kmem.h>
#include <sys/vtrace.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/pvn.h>
#include <vm/seg_map.h>
#include <sys/swap.h>
#include <vm/seg_kmem.h>

#else  /* _KERNEL */

#define	ASSERT(x)		/* don't use asserts for fsck et al */

#endif  /* _KERNEL */

#ifdef _KERNEL

/*
 * Used to verify that a given entry on the ufs_instances list (see below)
 * still refers to a mounted file system.
 *
 * XXX:	This is a crock that substitutes for proper locking to coordinate
 *	updates to and uses of the entries in ufs_instances.
 */
struct check_node {
	struct vfs *vfsp;
	struct ufsvfs *ufsvfs;
	dev_t vfs_dev;
};

static vfs_t *still_mounted(struct check_node *);

/*
 * All ufs file system instances are linked together into a list starting at
 * ufs_instances.  The list is updated as part of mount and unmount.  It's
 * consulted in ufs_update, to allow syncing out all ufs file system instances
 * in a batch.
 *
 * ufsvfs_mutex guards access to this list and to the {,old}ufsvfslist
 * manipulated in ufs_funmount_cleanup.  (A given ufs instance is always on
 * exactly one of these lists except while it's being allocated or
 * deallocated.)
 */
static struct ufsvfs	*ufs_instances;
extern kmutex_t		ufsvfs_mutex;	/* XXX: move this to ufs_inode.h? */

/*
 * ufsvfs list manipulation routines
 */

/*
 * Link ufsp in at the head of the list of ufs_instances.
 */
void
ufs_vfs_add(struct ufsvfs *ufsp)
{
	mutex_enter(&ufsvfs_mutex);
	ufsp->vfs_next = ufs_instances;
	ufs_instances = ufsp;
	mutex_exit(&ufsvfs_mutex);
}

/*
 * Remove ufsp from the list of ufs_instances.
 *
 * Does no error checking; ufsp is assumed to actually be on the list.
 */
void
ufs_vfs_remove(struct ufsvfs *ufsp)
{
	struct ufsvfs	**delpt = &ufs_instances;

	mutex_enter(&ufsvfs_mutex);
	for (; *delpt != NULL; delpt = &((*delpt)->vfs_next)) {
		if (*delpt == ufsp) {
			*delpt = ufsp->vfs_next;
			ufsp->vfs_next = NULL;
			break;
		}
	}
	mutex_exit(&ufsvfs_mutex);
}

/*
 * Clean up state resulting from a forcible unmount that couldn't be handled
 * directly during the unmount.  (See commentary in the unmount code for more
 * info.)
 */
static void
ufs_funmount_cleanup()
{
	struct ufsvfs		*ufsvfsp;
	extern struct ufsvfs	*oldufsvfslist, *ufsvfslist;

	/*
	 * Assumption: it's now safe to blow away the entries on
	 * oldufsvfslist.
	 */
	mutex_enter(&ufsvfs_mutex);
	while ((ufsvfsp = oldufsvfslist) != NULL) {
		oldufsvfslist = ufsvfsp->vfs_next;

		mutex_destroy(&ufsvfsp->vfs_lock);
		mutex_destroy(&ufsvfsp->vfs_rename_lock);
		kmem_free(ufsvfsp, sizeof (struct ufsvfs));
	}
	/*
	 * Rotate more recent unmount entries into place in preparation for
	 * the next time around.
	 */
	oldufsvfslist = ufsvfslist;
	ufsvfslist = NULL;
	mutex_exit(&ufsvfs_mutex);
}


/*
 * ufs_update performs the ufs part of `sync'.  It goes through the disk
 * queues to initiate sandbagged IO; goes through the inodes to write
 * modified nodes; and it goes through the mount table to initiate
 * the writing of the modified super blocks.
 */
extern time_t	time;
time_t		ufs_sync_time;
time_t		ufs_sync_time_secs = 1;

void
ufs_update(int flag)
{
	struct vfs *vfsp;
	struct fs *fs;
	struct ufsvfs *ufsp;
	struct ufsvfs *ufsnext;
	struct ufsvfs *update_list = NULL;
	int check_cnt = 0;
	size_t check_size;
	struct check_node *check_list, *ptr;
	int cheap = flag & SYNC_ATTR;

	/*
	 * This is a hack.  A design flaw in the forced unmount protocol
	 * could allow a thread to attempt to use a kmem_freed ufsvfs
	 * structure in ufs_lockfs_begin/ufs_check_lockfs.  This window
	 * is difficult to hit, even during the lockfs stress tests.
	 * So the hacky fix is to wait awhile before kmem_free'ing the
	 * ufsvfs structures for forcibly unmounted file systems.  `Awhile'
	 * is defined as every other call from fsflush (~60 seconds).
	 */
	if (cheap)
		ufs_funmount_cleanup();

	/*
	 * Examine all ufsvfs structures and add those that we can lock to the
	 * update list.  This is so that we don't hold the list lock for a
	 * long time.  If vfs_lock fails for a file system instance, then skip
	 * it because somebody is doing a unmount on it.
	 */
	mutex_enter(&ufsvfs_mutex);
	for (ufsp = ufs_instances; ufsp != NULL; ufsp = ufsp->vfs_next) {
		vfsp = ufsp->vfs_vfs;
		if (vfs_lock(vfsp) != 0)
			continue;
		ufsp->vfs_wnext = update_list;
		update_list = ufsp;
		check_cnt++;
	}
	mutex_exit(&ufsvfs_mutex);

	if (update_list == NULL)
		return;

	check_size = sizeof (struct check_node) * check_cnt;
	check_list = ptr = kmem_alloc(check_size, KM_NOSLEEP);

	/*
	 * Write back modified superblocks.
	 * Consistency check that the superblock of
	 * each file system is still in the buffer cache.
	 *
	 * Note that the update_list traversal is done without the protection
	 * of an overall list lock, so it's necessary to rely on the fact that
	 * each entry of the list is vfs_locked when moving from one entry to
	 * the next.  This works because a concurrent attempt to add an entry
	 * to another thread's update_list won't find it, since it'll already
	 * be locked.
	 */
	check_cnt = 0;
	for (ufsp = update_list; ufsp != NULL; ufsp = ufsnext) {
		/*
		 * Need to grab the next ptr before we unlock this one so
		 * another thread doesn't grab it and change it before we move
		 * on to the next vfs.  (Once we unlock it, it's ok if another
		 * thread finds it to add it to its own update_list; we don't
		 * attempt to refer to it through our list any more.)
		 */
		ufsnext = ufsp->vfs_wnext;
		vfsp = ufsp->vfs_vfs;

		/*
		 * Seems like this can't happen, so perhaps it should become
		 * an ASSERT(vfsp->vfs_data != NULL).
		 */
		if (!vfsp->vfs_data) {
			vfs_unlock(vfsp);
			continue;
		}

		fs = ufsp->vfs_fs;

		/*
		 * don't update a locked superblock during a panic; it
		 * may be in an inconsistent state
		 */
		if (panicstr) {
			if (!mutex_tryenter(&ufsp->vfs_lock)) {
				vfs_unlock(vfsp);
				continue;
			}
		} else
			mutex_enter(&ufsp->vfs_lock);
		/*
		 * Build up the STABLE check list, so we can unlock the vfs
		 * until we do the actual checking.
		 */
		if (check_list != NULL) {
			if ((fs->fs_ronly == 0) &&
			    (fs->fs_clean != FSBAD) &&
			    (fs->fs_clean != FSSUSPEND)) {
				ptr->vfsp = vfsp;
				ptr->ufsvfs = ufsp;
				ptr->vfs_dev = vfsp->vfs_dev;
				ptr++;
				check_cnt++;
			}
		}

		/*
		 * superblock is not modified
		 */
		if (fs->fs_fmod == 0) {
			mutex_exit(&ufsp->vfs_lock);
			vfs_unlock(vfsp);
			continue;
		}
		if (fs->fs_ronly != 0) {
			mutex_exit(&ufsp->vfs_lock);
			vfs_unlock(vfsp);
			(void) ufs_fault(ufsp->vfs_root,
					"fs = %s update: ro fs mod\n",
				fs->fs_fsmnt);
			/*
			 * XXX:	Why is this a return instead of a continue?
			 *	This may be an attempt to replace a panic with
			 *	something less drastic, but there's cleanup we
			 *	should be doing that's not being done (e.g.,
			 *	unlocking the remaining entries on the list).
			 */
			return;
		}
		fs->fs_fmod = 0;
		mutex_exit(&ufsp->vfs_lock);
		TRANS_SBUPDATE(ufsp, vfsp, TOP_SBUPDATE_UPDATE);
		vfs_unlock(vfsp);
	}

	ufs_sync_time = time;
	(void) ufs_scan_inodes(1, ufs_sync_inode, (void *)cheap);

	/*
	 * Force stale buffer cache information to be flushed,
	 * for all devices.  This should cause any remaining control
	 * information (e.g., cg and inode info) to be flushed back.
	 */
	bflush((dev_t)NODEV);

	if (check_list == NULL)
		return;

	/*
	 * For each UFS filesystem in the STABLE check_list, update
	 * the clean flag if warranted.
	 */
	for (ptr = check_list; check_cnt > 0; check_cnt--, ptr++) {
		int	error;

		/*
		 * still_mounted() returns with vfsp and the vfs_reflock
		 * held if ptr refers to a vfs that is still mounted.
		 */
		if ((vfsp = still_mounted(ptr)) == NULL)
			continue;
		ufs_checkclean(vfsp);
		/*
		 * commit any outstanding async transactions
		 */
		ufsp = (struct ufsvfs *)vfsp->vfs_data;
		curthread->t_flag |= T_DONTBLOCK;
		TRANS_BEGIN_SYNC(ufsp, TOP_COMMIT_UPDATE, TOP_COMMIT_SIZE);
		TRANS_END_SYNC(ufsp, error, TOP_COMMIT_UPDATE,
				TOP_COMMIT_SIZE);
		curthread->t_flag &= ~T_DONTBLOCK;

		vfs_unlock(vfsp);
	}

	kmem_free(check_list, check_size);
}

int
ufs_sync_inode(struct inode *ip, void *arg)
{
	int		cheap		= (int)arg;
	struct ufsvfs	*ufsvfsp	= ip->i_ufsvfs;

	/*
	 * if we are panic'ing; then don't update the inode if this
	 * file system is FSSTABLE.  Otherwise, we would have to
	 * force the superblock to FSACTIVE and the superblock
	 * may not be in a good state.  Also, if the inode is
	 * IREF'ed then it may be in an inconsistent state.  Don't
	 * push it.  Finally, don't push the inode if the fs is
	 * logging; the transaction will be discarded at boot.
	 */
	if (panicstr) {

		if (ip->i_flag & IREF)
			return (0);

		if (ip->i_ufsvfs == NULL ||
		    (ip->i_fs->fs_clean == FSSTABLE ||
		    ip->i_fs->fs_clean == FSLOG))
				return (0);
	}

	/*
	 * Limit access time only updates
	 */
	if (((ip->i_flag & (IMOD|IMODACC|IUPD|ICHG|IACC)) == IMODACC) &&
	    ufsvfsp) {
		/*
		 * if file system has deferred access time turned on and there
		 * was no IO recently, don't bother flushing it. It will be
		 * flushed when I/Os start again.
		 */
		if (cheap && (ufsvfsp->vfs_dfritime & UFS_DFRATIME) &&
		    (ufsvfsp->vfs_iotstamp + ufs_iowait < lbolt))
			return (0);
		/*
		 * an app issueing a sync() can take forever on a trans device
		 * when NetWorker or find is running because all of the
		 * directorys' access times have to be updated. So, we limit
		 * the time we spend updating access times per sync.
		 */
		if (TRANS_ISTRANS(ufsvfsp) && ((ufs_sync_time +
		    ufs_sync_time_secs) < time))
			return (0);
	}

	/*
	 * if we are running on behalf of the flush thread or this is
	 * a swap file, then simply do a delay update of the inode.
	 * Otherwise, push the pages and then do a delayed inode update.
	 */
	if (cheap || IS_SWAPVP(ITOV(ip))) {
		TRANS_IUPDAT(ip, 0);
	} else {
		(void) TRANS_SYNCIP(ip, B_ASYNC, I_ASYNC, TOP_SYNCIP_SYNC);
	}
	return (0);
}

/*
 * Flush all the pages associated with an inode using the given 'flags',
 * then force inode information to be written back using the given 'waitfor'.
 */
int
ufs_syncip(struct inode *ip, int flags, int waitfor)
{
	int	error;
	struct vnode *vp = ITOV(ip);

	TRACE_3(TR_FAC_UFS, TR_UFS_SYNCIP_START,
		"ufs_syncip_start:vp %p flags %x waitfor %x",
		vp, flags, waitfor);

	/*
	 * Return if file system has been forcibly umounted.
	 */
	if (ip->i_ufsvfs == NULL)
		return (0);
	/*
	 * don't need to VOP_PUTPAGE if there are no pages
	 */
	if (vp->v_pages == NULL || vp->v_type == VCHR) {
		error = 0;
	} else {
		error = VOP_PUTPAGE(vp, (offset_t)0, (size_t)0, flags, CRED());
	}
	if (panicstr && TRANS_ISTRANS(ip->i_ufsvfs))
		goto out;
	/*
	 * waitfor represents two things -
	 * 1. whether data sync or file sync.
	 * 2. if file sync then ufs_iupdat should 'waitfor' disk i/o or not.
	*/
	if (waitfor == I_DSYNC) {
		/*
		 * If data sync, only IATTCHG (size/block change) requires
		 * inode update, fdatasync()/FDSYNC implementation.
		 */
		if (ip->i_flag & (IBDWRITE|IATTCHG)) {
			rw_enter(&ip->i_contents, RW_WRITER);
			ip->i_flag &= ~IMODTIME;
			ufs_iupdat(ip, 1);
			rw_exit(&ip->i_contents);
		}
	} else {
		/* For file sync, any indoe change requires inode update */
		if (ip->i_flag & (IBDWRITE|IUPD|IACC|ICHG|IMOD|IMODACC)) {
			rw_enter(&ip->i_contents, RW_WRITER);
			ip->i_flag &= ~IMODTIME;
			ufs_iupdat(ip, waitfor);
			rw_exit(&ip->i_contents);
		}
	}

out:
	TRACE_2(TR_FAC_UFS, TR_UFS_SYNCIP_END,
		"ufs_syncip_end:vp %p error %d",
		vp, error);

	return (error);
}
/*
 * Flush all indirect blocks related to an inode.
 * Supports triple indirect blocks also.
 */
int
ufs_sync_indir(struct inode *ip)
{
	int i;
	daddr_t blkno;
	daddr_t lbn;	/* logical blkno of last blk in file */
	daddr_t clbn;	/* current logical blk */
	daddr32_t *bap;
	struct fs *fs;
	struct buf *bp;
	int bsize;
	struct ufsvfs *ufsvfsp;
	int j;
	daddr_t indirect_blkno;
	daddr32_t *indirect_bap;
	struct buf *indirect_bp;

	ufsvfsp = ip->i_ufsvfs;
	/*
	 * unnecessary when logging; allocation blocks are kept up-to-date
	 */
	if (TRANS_ISTRANS(ufsvfsp))
		return (0);

	fs = ufsvfsp->vfs_fs;
	bsize = fs->fs_bsize;
	lbn = (daddr_t)lblkno(fs, ip->i_size - 1);
	if (lbn < NDADDR)
		return (0);	/* No indirect blocks used */
	if (lbn < NDADDR + NINDIR(fs)) {
		/* File has one indirect block. */
		blkflush(ip->i_dev, (daddr_t)fsbtodb(fs, ip->i_ib[0]));
		return (0);
	}

	/* Write out all the first level indirect blocks */
	for (i = 0; i <= NIADDR; i++) {
		if ((blkno = ip->i_ib[i]) == 0)
			continue;
		blkflush(ip->i_dev, (daddr_t)fsbtodb(fs, blkno));
	}
	/* Write out second level of indirect blocks */
	if ((blkno = ip->i_ib[1]) == 0)
		return (0);
	bp = UFS_BREAD(ufsvfsp, ip->i_dev, (daddr_t)fsbtodb(fs, blkno), bsize);
	if (bp->b_flags & B_ERROR) {
		brelse(bp);
		return (EIO);
	}
	bap = bp->b_un.b_daddr;
	clbn = NDADDR + NINDIR(fs);
	for (i = 0; i < NINDIR(fs); i++) {
		if (clbn > lbn)
			break;
		clbn += NINDIR(fs);
		if ((blkno = bap[i]) == 0)
			continue;
		blkflush(ip->i_dev, (daddr_t)fsbtodb(fs, blkno));
	}

	brelse(bp);
	/* write out third level indirect blocks */

	if ((blkno = ip->i_ib[2]) == 0)
		return (0);

	bp = UFS_BREAD(ufsvfsp, ip->i_dev, (daddr_t)fsbtodb(fs, blkno), bsize);
	if (bp->b_flags & B_ERROR) {
		brelse(bp);
		return (EIO);
	}
	bap = bp->b_un.b_daddr;
	clbn = NDADDR + NINDIR(fs) + (NINDIR(fs) * NINDIR(fs));

	for (i = 0; i < NINDIR(fs); i++) {
		if (clbn > lbn)
			break;
		if ((indirect_blkno = bap[i]) == 0)
			continue;
		blkflush(ip->i_dev, (daddr_t)fsbtodb(fs, indirect_blkno));
		indirect_bp = UFS_BREAD(ufsvfsp, ip->i_dev,
			(daddr_t)fsbtodb(fs, indirect_blkno), bsize);
		if (indirect_bp->b_flags & B_ERROR) {
			brelse(indirect_bp);
			brelse(bp);
			return (EIO);
		}
		indirect_bap = indirect_bp->b_un.b_daddr;
		for (j = 0; j < NINDIR(fs); j++) {
			if (clbn > lbn)
				break;
			clbn += NINDIR(fs);
			if ((blkno = indirect_bap[j]) == 0)
				continue;
			blkflush(ip->i_dev, (daddr_t)fsbtodb(fs, blkno));
		}
		brelse(indirect_bp);
	}
	brelse(bp);

	return (0);
}

/*
 * Flush all indirect blocks related to an offset of a file.
 * read/write in sync mode may have to flush indirect blocks.
 */
int
ufs_indirblk_sync(struct inode *ip, offset_t off)
{
	daddr_t	lbn;
	struct	fs *fs;
	struct	buf *bp;
	int	i, j, shft;
	daddr_t	ob, nb, tbn;
	daddr32_t *bap;
	int	nindirshift, nindiroffset;
	struct ufsvfs *ufsvfsp;

	ufsvfsp = ip->i_ufsvfs;
	/*
	 * unnecessary when logging; allocation blocks are kept up-to-date
	 */
	if (TRANS_ISTRANS(ufsvfsp))
		return (0);

	fs = ufsvfsp->vfs_fs;

	lbn = (daddr_t)lblkno(fs, off);
	if (lbn < 0)
		return (EFBIG);

	/* The first NDADDR are direct so nothing to do */
	if (lbn < NDADDR)
		return (0);

	nindirshift = ip->i_ufsvfs->vfs_nindirshift;
	nindiroffset = ip->i_ufsvfs->vfs_nindiroffset;

	/* Determine level of indirect blocks */
	shft = 0;
	tbn = lbn - NDADDR;
	for (j = NIADDR; j > 0; j--) {
		longlong_t	sh;

		shft += nindirshift;
		sh = 1LL << shft;
		if (tbn < sh)
			break;
		tbn -= (daddr_t)sh;
	}

	if (j == 0)
		return (EFBIG);

	if ((nb = ip->i_ib[NIADDR - j]) == 0)
			return (0);		/* UFS Hole */

	/* Flush first level indirect block */
	blkflush(ip->i_dev, fsbtodb(fs, nb));

	/* Fetch through next levels */
	for (; j < NIADDR; j++) {
		ob = nb;
		bp = UFS_BREAD(ufsvfsp,
				ip->i_dev, fsbtodb(fs, ob), fs->fs_bsize);
		if (bp->b_flags & B_ERROR) {
			brelse(bp);
			return (EIO);
		}
		bap = bp->b_un.b_daddr;
		shft -= nindirshift;		/* sh / nindir */
		i = (tbn >> shft) & nindiroffset; /* (tbn /sh) & nindir */
		nb = bap[i];
		brelse(bp);
		if (nb == 0) {
			return (0); 		/* UFS hole */
		}
		blkflush(ip->i_dev, fsbtodb(fs, nb));
	}
	return (0);
}
/*
 * Check that a given indirect block contains blocks in range
 */
int
ufs_indir_badblock(struct inode *ip, daddr32_t *bap)
{
	int i;
	int err = 0;

	for (i = 0; i < NINDIR(ip->i_fs) - 1; i++)
		if (bap[i] != 0 && (err = ufs_badblock(ip, bap[i])))
			break;
	return (err);
}

/*
 * Check that a specified block number is in range.
 */
int
ufs_badblock(struct inode *ip, daddr_t bn)
{
	long	c;
	daddr_t	sum = 0;

	if (bn <= 0 || bn > ip->i_fs->fs_size)
		return (1);

	c = dtog(ip->i_fs, bn);
	if (c == 0) {
		sum = howmany(ip->i_fs->fs_cssize, ip->i_fs->fs_fsize);
	}
	/*
	 * if block no. is below this cylinder group,
	 * within the space reserved for superblock, inodes, (summary data)
	 * or if it is above this cylinder group
	 * then its invalid
	 * It's hard to see how we'd be outside this cyl, but let's be careful.
	 */
	if ((bn < cgbase(ip->i_fs, c)) ||
	    (bn >= cgsblock(ip->i_fs, c) && bn < cgdmin(ip->i_fs, c)+sum) ||
	    (bn >= cgbase(ip->i_fs, c+1)))
		return (1);

	return (0);	/* not a bad block */
}

/*
 * When i_rwlock is write-locked or has a writer pended, then the inode
 * is going to change in a way that the filesystem will be marked as
 * active. So no need to let the filesystem be mark as stable now.
 * Also to ensure the filesystem consistency during the directory
 * operations, filesystem cannot be marked as stable if i_rwlock of
 * the directory inode is write-locked.
 */

/*
 * Check for busy inodes for this filesystem.
 * NOTE: Needs better way to do this expensive operation in the future.
 */
static void
ufs_icheck(struct ufsvfs *ufsvfsp, int *isbusyp, int *isreclaimp)
{
	union  ihead	*ih;
	struct inode	*ip;
	int		i;
	int		isnottrans	= !TRANS_ISTRANS(ufsvfsp);
	int		isbusy		= *isbusyp;
	int		isreclaim	= *isreclaimp;

	for (i = 0, ih = ihead; i < inohsz; i++, ih++) {
		mutex_enter(&ih_lock[i]);
		for (ip = ih->ih_chain[0];
		    ip != (struct inode *)ih;
		    ip = ip->i_forw) {
			/*
			 * if inode is busy/modified/deleted, filesystem is busy
			 */
			if (ip->i_ufsvfs != ufsvfsp)
				continue;
			if ((ip->i_flag & (IMOD | IUPD | ICHG)) ||
			    (RW_ISWRITER(&ip->i_rwlock)))
				isbusy = 1;
			if ((ip->i_nlink <= 0) && (ip->i_flag & IREF))
				isreclaim = 1;
			if (isbusy && (isreclaim || isnottrans))
				break;
		}
		mutex_exit(&ih_lock[i]);
		if (isbusy && (isreclaim || isnottrans))
			break;
	}
	*isbusyp = isbusy;
	*isreclaimp = isreclaim;
}

/*
 * As part of the ufs 'sync' operation, this routine is called to mark
 * the filesystem as STABLE if there is no modified metadata in memory.
 */
void
ufs_checkclean(struct vfs *vfsp)
{
	struct ufsvfs	*ufsvfsp	= (struct ufsvfs *)vfsp->vfs_data;
	struct fs	*fs		= ufsvfsp->vfs_fs;
	int		isbusy;
	int		isreclaim;
	int		updatesb;

	ASSERT(vfs_lock_held(vfsp));

	/*
	 * filesystem is stable or cleanflag processing is disabled; do nothing
	 *	no transitions when panic'ing
	 */
	if (fs->fs_ronly ||
	    fs->fs_clean == FSBAD ||
	    fs->fs_clean == FSSUSPEND ||
	    fs->fs_clean == FSSTABLE ||
	    panicstr)
		return;

	/*
	 * if logging and nothing to reclaim; do nothing
	 */
	if ((fs->fs_clean == FSLOG) &&
	    (((fs->fs_reclaim & FS_RECLAIM) == 0) ||
	    (fs->fs_reclaim & FS_RECLAIMING)))
		return;

	/*
	 * FS_CHECKCLEAN is reset if the file system goes dirty
	 * FS_CHECKRECLAIM is reset if a file gets deleted
	 */
	mutex_enter(&ufsvfsp->vfs_lock);
	fs->fs_reclaim |= (FS_CHECKCLEAN | FS_CHECKRECLAIM);
	mutex_exit(&ufsvfsp->vfs_lock);

	updatesb = 0;

	/*
	 * if logging or buffers are busy; do nothing
	 */
	isbusy = isreclaim = 0;
	if ((fs->fs_clean == FSLOG) ||
	    (bcheck(vfsp->vfs_dev, ufsvfsp->vfs_bufp)))
		isbusy = 1;

	/*
	 * isreclaim == TRUE means can't change the state of fs_reclaim
	 */
	isreclaim =
		((fs->fs_clean == FSLOG) &&
		(((fs->fs_reclaim & FS_RECLAIM) == 0) ||
		(fs->fs_reclaim & FS_RECLAIMING)));

	/*
	 * if fs is busy or can't change the state of fs_reclaim; do nothing
	 */
	if (isbusy && isreclaim)
		return;

	/*
	 * look for busy or deleted inodes; (deleted == needs reclaim)
	 */
	ufs_icheck(ufsvfsp, &isbusy, &isreclaim);

	mutex_enter(&ufsvfsp->vfs_lock);

	/*
	 * IF POSSIBLE, RESET RECLAIM
	 */
	/*
	 * the reclaim thread is not running
	 */
	if ((fs->fs_reclaim & FS_RECLAIMING) == 0)
		/*
		 * no files were deleted during the scan
		 */
		if (fs->fs_reclaim & FS_CHECKRECLAIM)
			/*
			 * no deleted files were found in the inode cache
			 */
			if ((isreclaim == 0) && (fs->fs_reclaim & FS_RECLAIM)) {
				fs->fs_reclaim &= ~FS_RECLAIM;
				updatesb = 1;
			}
	/*
	 * IF POSSIBLE, SET STABLE
	 */
	/*
	 * not logging
	 */
	if (fs->fs_clean != FSLOG)
		/*
		 * file system has not gone dirty since the scan began
		 */
		if (fs->fs_reclaim & FS_CHECKCLEAN)
			/*
			 * nothing dirty was found in the buffer or inode cache
			 */
			if ((isbusy == 0) && (isreclaim == 0) &&
			    (fs->fs_clean != FSSTABLE)) {
				fs->fs_clean = FSSTABLE;
				updatesb = 1;
			}

	mutex_exit(&ufsvfsp->vfs_lock);
	if (updatesb) {
		TRANS_SBWRITE(ufsvfsp, TOP_SBWRITE_STABLE);
	}
}

/*
 * called whenever an unlink occurs
 */
void
ufs_setreclaim(struct inode *ip)
{
	struct ufsvfs	*ufsvfsp	= ip->i_ufsvfs;
	struct fs	*fs		= ufsvfsp->vfs_fs;

	if (ip->i_nlink || fs->fs_ronly || (fs->fs_clean != FSLOG))
		return;

	/*
	 * reclaim-needed bit is already set or we need to tell
	 * ufs_checkclean that a file has been deleted
	 */
	if ((fs->fs_reclaim & (FS_RECLAIM | FS_CHECKRECLAIM)) == FS_RECLAIM)
		return;

	mutex_enter(&ufsvfsp->vfs_lock);
	/*
	 * inform ufs_checkclean that the file system has gone dirty
	 */
	fs->fs_reclaim &= ~FS_CHECKRECLAIM;

	/*
	 * set the reclaim-needed bit
	 */
	if ((fs->fs_reclaim & FS_RECLAIM) == 0) {
		fs->fs_reclaim |= FS_RECLAIM;
		ufs_sbwrite(ufsvfsp);
	}
	mutex_exit(&ufsvfsp->vfs_lock);
}

/*
 * Before any modified metadata written back to the disk, this routine
 * is called to mark the filesystem as ACTIVE.
 */
void
ufs_notclean(struct ufsvfs *ufsvfsp)
{
	struct fs *fs = ufsvfsp->vfs_fs;

	ASSERT(MUTEX_HELD(&ufsvfsp->vfs_lock));
	ULOCKFS_SET_MOD((&ufsvfsp->vfs_ulockfs));

	/*
	 * inform ufs_checkclean that the file system has gone dirty
	 */
	fs->fs_reclaim &= ~FS_CHECKCLEAN;

	/*
	 * ignore if active or bad or suspended or readonly or logging
	 */
	if ((fs->fs_clean == FSACTIVE) || (fs->fs_clean == FSLOG) ||
	    (fs->fs_clean == FSBAD) || (fs->fs_clean == FSSUSPEND) ||
	    (fs->fs_ronly)) {
		mutex_exit(&ufsvfsp->vfs_lock);
		return;
	}
	fs->fs_clean = FSACTIVE;
	/*
	 * write superblock synchronously
	 */
	ufs_sbwrite(ufsvfsp);
	mutex_exit(&ufsvfsp->vfs_lock);
}

/*
 * ufs specific fbwrite()
 */
int
ufs_fbwrite(struct fbuf *fbp, struct inode *ip)
{
	struct ufsvfs	*ufsvfsp	= ip->i_ufsvfs;

	if (TRANS_ISTRANS(ufsvfsp))
		return (fbwrite(fbp));
	mutex_enter(&ufsvfsp->vfs_lock);
	ufs_notclean(ufsvfsp);
	return ((ufsvfsp->vfs_dio) ? fbdwrite(fbp) : fbwrite(fbp));
}

/*
 * ufs specific fbiwrite()
 */
int
ufs_fbiwrite(struct fbuf *fbp, struct inode *ip, daddr_t bn, long bsize)
{
	struct ufsvfs	*ufsvfsp	= ip->i_ufsvfs;
	o_mode_t	ifmt		= ip->i_mode & IFMT;
	buf_t		*bp;
	int		error;
	klwp_t		*lwp		= ttolwp(curthread);

	mutex_enter(&ufsvfsp->vfs_lock);
	ufs_notclean(ufsvfsp);
	if (ifmt == IFDIR || ifmt == IFSHAD ||
	    (ip->i_ufsvfs->vfs_qinod == ip)) {
		TRANS_DELTA(ufsvfsp, ldbtob(bn * (offset_t)(btod(bsize))),
			fbp->fb_count, DT_FBI, 0, 0);
	}
	/*
	 * Inlined version of fbiwrite()
	 */
	bp = pageio_setup((struct page *)NULL, fbp->fb_count,
			ip->i_devvp, B_WRITE);
	bp->b_flags &= ~B_PAGEIO;
	bp->b_un.b_addr = fbp->fb_addr;

	bp->b_blkno = bn * btod(bsize);
	bp->b_dev = cmpdev(ip->i_dev);	/* store in old dev format */
	bp->b_edev = ip->i_dev;
	bp->b_proc = NULL;			/* i.e. the kernel */

	if (ufsvfsp->vfs_log) {
		LUFS_STRATEGY(ufsvfsp->vfs_log, bp);
	} else {
		ufsvfsp->vfs_iotstamp = lbolt;
		ub.ub_fbiwrites.value.ul++;
		(void) bdev_strategy(bp);
		if (lwp != NULL)
			lwp->lwp_ru.oublock++;
	}
	error = biowait(bp);
	pageio_done(bp);
	fbrelse(fbp, S_OTHER);
	return (error);
}

/*
 * Write the ufs superblock only.
 */
void
ufs_sbwrite(struct ufsvfs *ufsvfsp)
{
	char sav_fs_fmod;
	struct fs *fs = ufsvfsp->vfs_fs;
	struct buf *bp = ufsvfsp->vfs_bufp;

	ASSERT(MUTEX_HELD(&ufsvfsp->vfs_lock));

	/*
	 * for ulockfs processing, limit the superblock writes
	 */
	if ((ufsvfsp->vfs_ulockfs.ul_sbowner) &&
	    (curthread != ufsvfsp->vfs_ulockfs.ul_sbowner)) {
		/* try again later */
		fs->fs_fmod = 1;
		return;
	}

	ULOCKFS_SET_MOD((&ufsvfsp->vfs_ulockfs));
	/*
	 * update superblock timestamp and fs_clean checksum
	 * if marked FSBAD, we always want an erroneous
	 * checksum to force repair
	 */
	fs->fs_time = hrestime.tv_sec;
	fs->fs_state = fs->fs_clean != FSBAD? FSOKAY - fs->fs_time:
						-(FSOKAY - fs->fs_time);
	switch (fs->fs_clean) {
	case FSCLEAN:
	case FSSTABLE:
		fs->fs_reclaim &= ~FS_RECLAIM;
		break;
	case FSACTIVE:
	case FSSUSPEND:
	case FSBAD:
	case FSLOG:
		break;
	default:
		fs->fs_clean = FSACTIVE;
		break;
	}
	/*
	 * reset incore only bits
	 */
	fs->fs_reclaim &= ~(FS_CHECKCLEAN | FS_CHECKRECLAIM);

	/*
	 * delta the whole superblock
	 */
	TRANS_DELTA(ufsvfsp, ldbtob(SBLOCK), sizeof (struct fs),
		DT_SB, NULL, 0);
	/*
	 * retain the incore state of fs_fmod; set the ondisk state to 0
	 */
	sav_fs_fmod = fs->fs_fmod;
	fs->fs_fmod = 0;

	/*
	 * Don't release the buffer after written to the disk
	 */
	UFS_BWRITE2(ufsvfsp, bp);
	fs->fs_fmod = sav_fs_fmod;	/* reset fs_fmod's incore state */
}

/*
 * Returns 1 and hold the lock if the vfs is still being mounted.
 * Otherwise, returns 0.
 *
 * For our purposes, "still mounted" means that the file system still appears
 * on the list of UFS file system instances.
 */
static vfs_t *
still_mounted(struct check_node *checkp)
{
	struct vfs	*vfsp;
	struct ufsvfs	*ufsp;

	mutex_enter(&ufsvfs_mutex);
	for (ufsp = ufs_instances; ufsp != NULL; ufsp = ufsp->vfs_next) {
		if (ufsp != checkp->ufsvfs)
			continue;
		/*
		 * Tentative match:  verify it and try to lock.  (It's not at
		 * all clear how the verification could fail, given that we've
		 * gotten this far.  We would have had to reallocate the
		 * ufsvfs struct at hand for a new incarnation; is that really
		 * possible in the interval from constructing the check_node
		 * to here?)
		 */
		vfsp = ufsp->vfs_vfs;
		if (vfsp != checkp->vfsp)
			continue;
		if (vfsp->vfs_dev != checkp->vfs_dev)
			continue;
		if (vfs_lock(vfsp) != 0)
			continue;

		mutex_exit(&ufsvfs_mutex);
		return (vfsp);
	}
	mutex_exit(&ufsvfs_mutex);
	return (NULL);
}

/*
 * ufs_getsummaryinfo
 */
int
ufs_getsummaryinfo(dev_t dev, struct ufsvfs *ufsvfsp, struct fs *fs)
{
	int		i;		/* `for' loop counter */
	ssize_t		size;		/* bytes of summary info to read */
	daddr_t		frags;		/* frags of summary info to read */
	caddr_t		sip;		/* summary info */
	struct buf	*tp;		/* tmp buf */

	/*
	 * maintain metadata map for trans device (debug only)
	 */
	TRANS_MATA_SI(ufsvfsp, fs);

	/*
	 * Compute #frags and allocate space for summary info
	 */
	frags = howmany(fs->fs_cssize, fs->fs_fsize);
	sip = kmem_alloc((size_t)fs->fs_cssize, KM_SLEEP);
	fs->fs_u.fs_csp = (struct csum *)sip;

	/* Read summary info a fs block at a time */
	size = fs->fs_bsize;
	for (i = 0; i < frags; i += fs->fs_frag) {
		if (i + fs->fs_frag > frags)
			/*
			 * This happens only the last iteration, so
			 * don't worry about size being reset
			 */
			size = (frags - i) * fs->fs_fsize;
		tp = UFS_BREAD(ufsvfsp,
			dev, (daddr_t)fsbtodb(fs, fs->fs_csaddr+i), size);
		tp->b_flags |= B_STALE | B_AGE;
		if (tp->b_flags & B_ERROR) {
			kmem_free(fs->fs_u.fs_csp, fs->fs_cssize);
			fs->fs_u.fs_csp = NULL;
			brelse(tp);
			return (EIO);
		}
		bcopy(tp->b_un.b_addr, sip, size);
		sip += size;
		brelse(tp);
	}
	bzero((caddr_t)&fs->fs_cstotal, sizeof (fs->fs_cstotal));
	for (i = 0; i < fs->fs_ncg; ++i) {
		fs->fs_cstotal.cs_ndir += fs->fs_cs(fs, i).cs_ndir;
		fs->fs_cstotal.cs_nbfree += fs->fs_cs(fs, i).cs_nbfree;
		fs->fs_cstotal.cs_nifree += fs->fs_cs(fs, i).cs_nifree;
		fs->fs_cstotal.cs_nffree += fs->fs_cs(fs, i).cs_nffree;
	}
	return (0);
}

/*
 * Decide whether it is okay to remove within a sticky directory.
 * Two conditions need to be met:  write access to the directory
 * is needed.  In sticky directories, write access is not sufficient;
 * you can remove entries from a directory only if you own the directory,
 * if you are the superuser, if you own the entry or if the entry is
 * a plain file and you have write access to that file.
 * Function returns 0 if remove access is granted.
 */
int
ufs_sticky_remove_access(struct inode *dp, struct inode *ip, struct cred *cr)
{
	if ((dp->i_mode & ISVTX) && (cr->cr_uid != 0) &&
	    (cr->cr_uid != dp->i_uid) && (cr->cr_uid != ip->i_uid)) {
		if ((ip->i_mode & IFMT) == IFREG)
			return (ufs_iaccess(ip, IWRITE, cr));
		else
			return (EACCES);
	}
	return (0);
}
#endif	_KERNEL

extern	int around[9];
extern	int inside[9];
extern	uchar_t *fragtbl[];

/*
 * Update the frsum fields to reflect addition or deletion
 * of some frags.
 */
void
fragacct(struct fs *fs, int fragmap, int32_t *fraglist, int cnt)
{
	int inblk;
	int field, subfield;
	int siz, pos;

	/*
	 * ufsvfsp->vfs_lock is held when calling this.
	 */
	inblk = (int)(fragtbl[fs->fs_frag][fragmap]) << 1;
	fragmap <<= 1;
	for (siz = 1; siz < fs->fs_frag; siz++) {
		if ((inblk & (1 << (siz + (fs->fs_frag % NBBY)))) == 0)
			continue;
		field = around[siz];
		subfield = inside[siz];
		for (pos = siz; pos <= fs->fs_frag; pos++) {
			if ((fragmap & field) == subfield) {
				fraglist[siz] += cnt;
				ASSERT(fraglist[siz] >= 0);
				pos += siz;
				field <<= siz;
				subfield <<= siz;
			}
			field <<= 1;
			subfield <<= 1;
		}
	}
}

/*
 * Block operations
 */

/*
 * Check if a block is available
 */
int
isblock(struct fs *fs, uchar_t *cp, daddr_t h)
{
	uchar_t mask;

	ASSERT(fs->fs_frag == 8 || fs->fs_frag == 4 || fs->fs_frag == 2 || \
		    fs->fs_frag == 1);
	/*
	 * ufsvfsp->vfs_lock is held when calling this.
	 */
	switch ((int)fs->fs_frag) {
	case 8:
		return (cp[h] == 0xff);
	case 4:
		mask = 0x0f << ((h & 0x1) << 2);
		return ((cp[h >> 1] & mask) == mask);
	case 2:
		mask = 0x03 << ((h & 0x3) << 1);
		return ((cp[h >> 2] & mask) == mask);
	case 1:
		mask = 0x01 << (h & 0x7);
		return ((cp[h >> 3] & mask) == mask);
	default:
#ifndef _KERNEL
		cmn_err(CE_PANIC, "isblock: illegal fs->fs_frag value (%d)",
			    fs->fs_frag);
#endif /* _KERNEL */
		return (0);
	}
}

/*
 * Take a block out of the map
 */
void
clrblock(struct fs *fs, uchar_t *cp, daddr_t h)
{
	ASSERT(fs->fs_frag == 8 || fs->fs_frag == 4 || fs->fs_frag == 2 || \
		fs->fs_frag == 1);
	/*
	 * ufsvfsp->vfs_lock is held when calling this.
	 */
	switch ((int)fs->fs_frag) {
	case 8:
		cp[h] = 0;
		return;
	case 4:
		cp[h >> 1] &= ~(0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] &= ~(0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] &= ~(0x01 << (h & 0x7));
		return;
	default:
#ifndef _KERNEL
		cmn_err(CE_PANIC, "clrblock: illegal fs->fs_frag value (%d)",
			    fs->fs_frag);
#endif /* _KERNEL */
		return;
	}
}

/*
 * Is block allocated?
 */
int
isclrblock(struct fs *fs, uchar_t *cp, daddr_t h)
{
	uchar_t	mask;
	int	frag;
	/*
	 * ufsvfsp->vfs_lock is held when calling this.
	 */
	frag = fs->fs_frag;
	ASSERT(frag == 8 || frag == 4 || frag == 2 || frag == 1);
	switch (frag) {
	case 8:
		return (cp[h] == 0);
	case 4:
		mask = ~(0x0f << ((h & 0x1) << 2));
		return (cp[h >> 1] == (cp[h >> 1] & mask));
	case 2:
		mask =	~(0x03 << ((h & 0x3) << 1));
		return (cp[h >> 2] == (cp[h >> 2] & mask));
	case 1:
		mask = ~(0x01 << (h & 0x7));
		return (cp[h >> 3] == (cp[h >> 3] & mask));
	default:
#ifndef _KERNEL
		cmn_err(CE_PANIC, "isclrblock: illegal fs->fs_frag value (%d)",
			    fs->fs_frag);
#endif /* _KERNEL */
		break;
	}
	return (0);
}

/*
 * Put a block into the map
 */
void
setblock(struct fs *fs, uchar_t *cp, daddr_t h)
{
	ASSERT(fs->fs_frag == 8 || fs->fs_frag == 4 || fs->fs_frag == 2 || \
		    fs->fs_frag == 1);
	/*
	 * ufsvfsp->vfs_lock is held when calling this.
	 */
	switch ((int)fs->fs_frag) {
	case 8:
		cp[h] = 0xff;
		return;
	case 4:
		cp[h >> 1] |= (0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] |= (0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] |= (0x01 << (h & 0x7));
		return;
	default:
#ifndef _KERNEL
		cmn_err(CE_PANIC, "setblock: illegal fs->fs_frag value (%d)",
			    fs->fs_frag);
#endif /* _KERNEL */
		return;
	}
}

int
skpc(char c, uint_t len, char *cp)
{
	if (len == 0)
		return (0);
	while (*cp++ == c && --len)
		;
	return (len);
}

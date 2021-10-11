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
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)s5_subr.c	1.9	98/01/23 SMI"
/* from ufs_subr.c	2.64	92/12/02 SMI */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/rwlock.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/fs/s5_fs.h>
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
#include <sys/fs/s5_inode.h>
#include <sys/kmem.h>
#include <sys/vtrace.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg_map.h>
#include <sys/swap.h>
#include <vm/seg_kmem.h>

#else  /* _KERNEL */

#define	ASSERT(x)		/* don't use asserts for fsck et al */

#endif  /* _KERNEL */

#ifdef _KERNEL

/*
 * Used to verify that a given entry on the s5fs_instances list (see below)
 * still refers to a mounted file system.
 *
 * XXX:	This is a crock that substitutes for proper locking to coordinate
 *	updates to and uses of the entries in s5fs_instances.
 */
struct check_node {
	struct vfs *vfsp;
	struct s5vfs *s5vfs;
	dev_t vfs_dev;
};

static vfs_t *still_mounted(struct check_node *);

/*
 * All s5 file system instances are linked together into a list starting at
 * s5fs_instances.  The list is updated as part of mount and unmount.  It's
 * consulted in s5_update, to allow syncing out all s5 file system instances
 * in a batch.
 *
 * s5vfs_mutex guards access to this list and to the {,old}s5vfslist
 * manipulated in s5_funmount_cleanup.  (A given s5 instance is always on
 * exactly one of these lists except while it's being allocated or
 * deallocated.)
 */
static struct s5vfs	*s5fs_instances;
extern kmutex_t		s5vfs_mutex;	/* XXX: move this to s5_inode.h? */

/*
 * s5vfs list manipulation routines
 */

/*
 * Link s5vfsp in at the head of the list of s5fs_instances.
 */
void
s5_vfs_add(struct s5vfs *s5vfsp)
{
	mutex_enter(&s5vfs_mutex);
	s5vfsp->vfs_next = s5fs_instances;
	s5fs_instances = s5vfsp;
	mutex_exit(&s5vfs_mutex);
}

/*
 * Remove s5vfsp from the list of s5fs_instances.
 *
 * Does no error checking; s5vfsp is assumed to actually be on the list.
 */
void
s5_vfs_remove(struct s5vfs *s5vfsp)
{
	struct s5vfs	**delpt = &s5fs_instances;

	mutex_enter(&s5vfs_mutex);
	for (; *delpt != NULL; delpt = &((*delpt)->vfs_next)) {
		if (*delpt == s5vfsp) {
			*delpt = s5vfsp->vfs_next;
			s5vfsp->vfs_next = NULL;
			break;
		}
	}
	mutex_exit(&s5vfs_mutex);
}


/*
 * s5_update performs the s5 part of `sync'.  It goes through the disk
 * queues to initiate sandbagged IO; goes through the inodes to write
 * modified nodes; and it goes through the mount table to initiate
 * the writing of the modified super blocks.
 */
void
s5_update(int flag)
{
	struct vfs *vfsp;
	struct filsys *fs;
	struct s5vfs *s5fsp;
	struct s5vfs *s5fsnext;
	struct s5vfs *update_list = NULL;
	time_t start_time;
	int check_cnt = 0;
	size_t check_size;
	struct check_node *check_list, *ptr;
	extern void s5_sbupdate(struct vfs *vfsp);

	mutex_enter(&s5_syncbusy);
	/*
	 * Examine all s5vfs structures and add those that we can lock to the
	 * update list.  This is so that we don't hold the list lock for a
	 * long time.  If vfs_lock fails for a file system instance, then skip
	 * it because somebody is doing a unmount on it.
	 */
	mutex_enter(&s5vfs_mutex);
	for (s5fsp = s5fs_instances; s5fsp != NULL; s5fsp = s5fsp->vfs_next) {
		vfsp = s5fsp->vfs_vfs;
		if (vfs_lock(vfsp) != 0)
			continue;
		s5fsp->vfs_wnext = update_list;
		update_list = s5fsp;
		check_cnt++;
	}
	mutex_exit(&s5vfs_mutex);

	if (update_list == NULL) {
		mutex_exit(&s5_syncbusy);
		return;
	}

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
	for (s5fsp = update_list; s5fsp != NULL; s5fsp = s5fsnext) {
		/*
		 * Need to grab the next ptr before we unlock this one so
		 * another thread doesn't grab it and change it before we move
		 * on to the next vfs.  (Once we unlock it, it's ok if another
		 * thread finds it to add it to its own update_list; we don't
		 * attempt to refer to it through our list any more.)
		 */
		s5fsnext = s5fsp->vfs_wnext;
		vfsp = s5fsp->vfs_vfs;

		/*
		 * Seems like this can't happen, so perhaps it should become
		 * an ASSERT(vfsp->vfs_data != NULL).
		 */
		if (!vfsp->vfs_data) {
			vfs_unlock(vfsp);
			continue;
		}

		fs = (struct filsys *)s5fsp->vfs_fs;

		mutex_enter(&s5fsp->vfs_lock);

		/*
		 * Build up the STABLE check list, so we can unlock the vfs
		 * until we do the actual checking.
		 */
		if (check_list != NULL) {
			if ((fs->s_ronly == 0) &&
			    (fs->s_state != FsBADBLK) &&
			    (fs->s_state != FsBAD)) {
				/* ignore if read only or BAD or SUPEND */
				ptr->vfsp = vfsp;
				ptr->s5vfs = s5fsp;
				ptr->vfs_dev = vfsp->vfs_dev;
				ptr++;
				check_cnt++;
			}
		}

		if (fs->s_fmod == 0) {
			mutex_exit(&s5fsp->vfs_lock);
			vfs_unlock(vfsp);
			continue;
		}
		if (fs->s_ronly != 0) {
			mutex_exit(&s5fsp->vfs_lock);
			mutex_exit(&s5_syncbusy);
			cmn_err(CE_PANIC, "update ro S5FS mod\n");
		}
		fs->s_fmod = 0;
		mutex_exit(&s5fsp->vfs_lock);

		s5_sbupdate(vfsp);
		vfs_unlock(vfsp);
	}
	s5_flushi(flag);
	/*
	 * Force stale buffer cache information to be flushed,
	 * for all devices.  This should cause any remaining control
	 * information (e.g., inode info) to be flushed back.
	 */
	bflush((dev_t)NODEV);

	if (check_list == NULL) {
		mutex_exit(&s5_syncbusy);
		return;
	}

	/*
	 * For each S5 filesystem in the STABLE check_list, update
	 * the clean flag if warranted.
	 */
	start_time = hrestime.tv_sec;
	for (ptr = check_list; check_cnt > 0; check_cnt--, ptr++) {
		/*
		 * still_mounted() returns with vfsp and the vfs_reflock
		 * held if ptr refers to a vfs that is still mounted.
		 */
		if ((vfsp = still_mounted(ptr)) == NULL)
			continue;

		s5_checkclean(vfsp, ptr->s5vfs, ptr->vfs_dev, start_time);
		vfs_unlock(vfsp);
	}

	mutex_exit(&s5_syncbusy);
	kmem_free(check_list, check_size);
}

void
s5_flushi(flag)
	int flag;
{
	struct inode *ip, *lip;
	struct vnode *vp;
	int cheap = flag & SYNC_ATTR;

	/*
	 * Write back each (modified) inode,
	 * but don't sync back pages if vnode is
	 * part of the virtual swap device.
	 */
	union  ihead *ih;
	extern krwlock_t	icache_lock;

	rw_enter(&icache_lock, RW_READER);
	for (ih = ihead; ih < &ihead[INOHSZ]; ih++) {
		for (ip = ih->ih_chain[0], lip = NULL;
		    ip && ip != (struct inode *)ih; ip = ip->i_forw) {
			int	flag = ip->i_flag;	/* Atomic read */

			vp = ITOV(ip);
			/*
			 * Skip locked & inactive inodes.
			 * Skip inodes w/ no pages and no inode changes.
			 * Skip inodes from read only vfs's.
			 */
			if ((flag & IREF) == 0 ||
			    ((vp->v_pages == NULL) &&
			    ((flag & (IMOD|IACC|IUPD|ICHG)) == 0)) ||
			    (vp->v_vfsp == NULL) ||
			    ((vp->v_vfsp->vfs_flag & VFS_RDONLY) != 0))
				continue;

			if (!rw_tryenter(&ip->i_contents, RW_WRITER))
				continue;

			VN_HOLD(vp);
			/*
			 * Can't call s5_iput with ip because we may
			 * kmem_free the inode destorying i_forw.
			 */
			if (lip != NULL)
				s5_iput(lip);
			lip = ip;

			/*
			 * If this is an inode sync for file system hardening
			 * or this is a full sync but file is a swap file,
			 * don't sync pages but make sure the inode is up
			 * to date.  In other cases, push everything out.
			 */
			if (cheap || IS_SWAPVP(vp)) {
				s5_iupdat(ip, 0);
			} else {
				(void) s5_syncip(ip, B_ASYNC, I_SYNC);
			}
			rw_exit(&ip->i_contents);
		}
		if (lip != NULL)
			s5_iput(lip);
	}
	rw_exit(&icache_lock);
}

/*
 * Flush all the pages associated with an inode using the given 'flags',
 * then force inode information to be written back using the given 'waitfor'.
 */
int
s5_syncip(struct inode *ip, int flags, int waitfor)
{
	int	error;
	struct vnode *vp = ITOV(ip);

	ASSERT(RW_WRITE_HELD(&ip->i_contents));

	TRACE_3(TR_FAC_S5, TR_S5_SYNCIP_START,
		"s5_syncip_start:vp %x flags %x waitfor %x",
		vp, flags, waitfor);

	/*
	 * Return if file system has been forcibly umounted.
	 * (Shouldn't happen with S5.)
	 */
	if (ip->i_s5vfs == NULL)
		return (0);

	/*
	 * The data for directories is always written synchronously
	 * so we can skip pushing pages if we are just doing a B_ASYNC
	 */
	if (vp->v_pages == NULL || vp->v_type == VCHR ||
	    (vp->v_type == VDIR && flags == B_ASYNC)) {
		error = 0;
	} else {
		rw_exit(&ip->i_contents);
		error = VOP_PUTPAGE(vp, (offset_t)0, (u_int)0, flags, CRED());
		rw_enter(&ip->i_contents, RW_WRITER);
	}
	if (ip->i_flag & (IUPD |IACC | ICHG | IMOD))
		s5_iupdat(ip, waitfor);

	TRACE_2(TR_FAC_S5, TR_S5_SYNCIP_END,
		"s5_syncip_end:vp %x error %d",
		vp, error);

	return (error);
}

/*
 * Flush all indirect blocks related to an inode.
 */
int
s5_sync_indir(struct inode *ip)
{
	int i;
	daddr_t lbn;	/* logical blkno of last blk in file */
	daddr_t sbn;	/* starting blkno of next indir block */
	struct filsys *fs;
	int bsize;
	static int s5_flush_indir();
	int error;

	fs = ITOF(ip);
	bsize = ip->i_s5vfs->vfs_bsize;
	lbn = lblkno(ip->i_s5vfs, ip->i_size - 1);
	sbn = NDADDR;

	for (i = 0; i < 3; i++) {
		if (lbn < sbn)
			return (0);
		if (error = s5_flush_indir(fs, ip, i, ip->i_addr[NADDR+i],
		    lbn, bsize, &sbn))
			return (error);
	}
	return (EFBIG);	/* Shouldn't happen */
}

static int
s5_flush_indir(fs, ip, lvl, iblk, lbn, bsize, sbnp)
	struct filsys *fs;
	struct inode *ip;
	int lvl;			/* indirect block level */
	daddr_t iblk;			/* indirect block */
	daddr_t lbn;			/* last block number in file */
	int bsize;
	daddr_t *sbnp;			/* ptr to blkno of starting block */
{
	int i;
	daddr_t clbn = *sbnp;	/* current logical blk */
	daddr_t *bap;
	struct buf *bp;
	int nindir = NINDIR(ip->i_s5vfs);
	int error = 0;

	if (iblk) {
		if (lvl > 0) {
			bp = bread(ip->i_dev, (daddr_t)fsbtodb(fs, iblk),
			    bsize);
			if (bp->b_flags & B_ERROR) {
				brelse(bp);
				error = EIO;
				goto out;
			}
			bap = (daddr_t *)bp->b_un.b_daddr;
			for (i = 0; i < nindir; i++) {
				if (clbn > lbn)
					break;
				if (error = s5_flush_indir(fs, ip, lvl-1,
				    bap[i], lbn, bsize, &clbn))
					break;
			}
			brelse(bp);
		}
		blkflush(ip->i_dev, (daddr_t)fsbtodb(fs, iblk));
	}

out:
	for (i = nindir; --lvl >= 0; i *= nindir)
		/* null */;
	*sbnp += i;
	return (error);
}

/*
 * As part of the s5 'sync' operation, this routine is called to mark
 * the filesystem as STABLE if there is no modified metadata in memory.
 * S5, unlike UFS, doesn't have a "STABLE" state; instead, we write all
 * superblocks that pass the bcheck and icheck tests.
 */
void
s5_checkclean(vfsp, s5vfsp, dev, timev)
	struct vfs *vfsp;
	struct s5vfs *s5vfsp;
	dev_t dev;
	time_t timev;
{
	struct filsys *fs;
	static int s5_icheck();
	struct ulockfs *ulp = &s5vfsp->vfs_ulockfs;

	ASSERT(vfs_lock_held(vfsp) || MUTEX_HELD(&ulp->ul_lock));

	fs = (struct filsys *)s5vfsp->vfs_fs;
	/*
	 * ignore if buffers or inodes are busy
	 */
	if ((bcheck(dev, s5vfsp->vfs_bufp)) || (s5_icheck(s5vfsp)))
		return;
	mutex_enter(&s5vfsp->vfs_lock);
	/*
	 * ignore if someone else modified the superblock while we
	 * are doing the "stable" checking.
	 * for S5, the time check here is coarser than for UFS.
	 */
	if (fs->s_time > timev) {
		mutex_exit(&s5vfsp->vfs_lock);
		return;
	}

	/*
	 * write superblock synchronously
	 */
	s5_sbwrite(s5vfsp);
	mutex_exit(&s5vfsp->vfs_lock);
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
static int
s5_icheck(s5vfsp)
	struct s5vfs	*s5vfsp;
{
	union  ihead	*ih;
	struct inode	*ip;
	extern krwlock_t icache_lock;

	rw_enter(&icache_lock, RW_READER);
	for (ih = ihead; ih < &ihead[INOHSZ]; ih++) {
		for (ip = ih->ih_chain[0];
		    ((ip != NULL) && (ip != (struct inode *)ih));
		    ip = ip->i_forw) {
			/*
			 * if inode is busy/modified/deleted, filesystem is busy
			 */
			if ((ip->i_s5vfs == s5vfsp) &&
			    ((ip->i_flag & (IMOD|IUPD|ICHG)) ||
			    (RW_ISWRITER(&ip->i_rwlock)) ||
			    ((ip->i_nlink <= 0) && (ip->i_flag & IREF)))) {
				rw_exit(&icache_lock);
				return (1);
			}
		}

	}
	rw_exit(&icache_lock);
	return (0);
}

/*
 * s5 specific fbwrite()
 */
/*ARGSUSED1*/
int
s5_fbwrite(fbp, ip)
	struct fbuf *fbp;
	struct inode *ip;
{
	return (fbwrite(fbp));
}

/*
 * s5 specific fbiwrite()
 */
int
s5_fbiwrite(fbp, ip, bn, bsize)
	struct fbuf *fbp;
	struct inode *ip;
	daddr_t bn;
	long bsize;
{
	return (fbiwrite(fbp, ip->i_devvp, bn, (int)bsize));
}

/*
 * Write the s5 superblock only.
 */
void
s5_sbwrite(struct s5vfs *s5vfsp)
{
	struct filsys *fs = (struct filsys *)s5vfsp->vfs_fs;
	char save_mod;
	int  isactive;

	ASSERT(MUTEX_HELD(&s5vfsp->vfs_lock));
	/*
	 * update superblock timestamp and s_state
	 */
	fs->s_time = hrestime.tv_sec;
	if (fs->s_state == FsACTIVE) {
		fs->s_state = FsOKAY - (long)fs->s_time;
		isactive = 1;
	} else
		isactive = 0;

	save_mod = fs->s_fmod;
	fs->s_fmod = 0;	/* s_fmod must always be 0 */
	/*
	 * Don't release the buffer after writing to the disk
	 */
	bwrite2(s5vfsp->vfs_bufp);		/* update superblock */

	if (isactive)
		fs->s_state = FsACTIVE;

	fs->s_fmod = save_mod;
}

/*
 * Returns a pointer to the vfs and holds the lock if the vfs is still
 * being mounted.
 * Otherwise, returns NULL.
 *
 * For our purposes, "still mounted" means that the file system still appears
 * on the list of s5 file system instances.
 */
static vfs_t *
still_mounted(struct check_node *checkp)
{
	struct vfs	*vfsp;
	struct s5vfs	*s5fsp;

	mutex_enter(&s5vfs_mutex);
	for (s5fsp = s5fs_instances; s5fsp != NULL; s5fsp = s5fsp->vfs_next) {
		if (s5fsp != checkp->s5vfs)
			continue;
		/*
		 * Tentative match:  verify it and try to lock.  (It's not at
		 * all clear how the verification could fail, given that we've
		 * gotten this far.  We would have had to reallocate the
		 * s5vfs struct at hand for a new incarnation; is that really
		 * possible in the interval from constructing the check_node
		 * to here?)
		 */
		vfsp = s5fsp->vfs_vfs;
		if (vfsp != checkp->vfsp)
			continue;
		if (vfsp->vfs_dev != checkp->vfs_dev)
			continue;
		if (vfs_lock(vfsp) != 0)
			continue;

		mutex_exit(&s5vfs_mutex);
		return (vfsp);
	}
	mutex_exit(&s5vfs_mutex);
	return (NULL);
}
#endif	_KERNEL

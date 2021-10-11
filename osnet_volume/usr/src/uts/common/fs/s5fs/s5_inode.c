/*	copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
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
 * 	(c) 1986,1987,1988,1989,1993,1996  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */
#pragma ident	"@(#)s5_inode.c	1.14	99/05/04 SMI"
/* from ufs_inode.c	2.101	92/12/02 */

/*
 * Copyright (c) 1993,1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/dnlc.h>
#include <sys/mode.h>
#include <sys/cmn_err.h>
#include <sys/isa_defs.h>
#include <sys/fs/s5_inode.h>
#include <sys/fs/s5_fs.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/pvn.h>
#include <vm/seg.h>
#include <sys/swap.h>
#include <sys/cpuvar.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <fs/fs_subr.h>

int	s5_ninode = 200;

struct	instats s5ins;		/* XXX not protected */

/* Current # of inodes kmem_allocated. Protected by icache_lock */
static int ino_new;

union ihead ihead[INOHSZ];	/* inode LRU cache, Chris Maltby */
static struct inode *ifreeh, **ifreet;
static kmutex_t	ifree_lock;	/* mutex protects inode free list */
krwlock_t icache_lock;		/* protect inode cache hash table */

void ihinit(), idrop(), s5_iinactive(), s5_iupdat(), s5_ilock();
void s5_iaddfree(), s5_irmfree();

extern int dnlc_fs_purge1();
extern void pvn_vpzero();

/*
 * Initialize the vfs structure
 */

int s5fstype;
extern struct vnodeops s5_vnodeops;
extern struct vfsops s5_vfsops;

int
s5init(vswp, fstype)
	struct vfssw *vswp;
	int fstype;
{
	ihinit();

	rw_init(&icache_lock, NULL, RW_DEFAULT, NULL);

	/*
	 * NOTE: Sun's auto-config initializes this in
	 * 	vfs_conf.c
	 * Associate vfs and vnode operations
	 */

	vswp->vsw_vfsops = &s5_vfsops;
	s5fstype = fstype;
	return (0);
}

/*
 * Initialize hash links for inodes
 * and build inode free list.
 */
void
ihinit()
{
	register int i;
	register union	ihead *ih = ihead;

	for (i = INOHSZ; --i >= 0; ih++) {
		ih->ih_head[0] = ih;
		ih->ih_head[1] = ih;
	}
	ifreeh = NULL;
	ifreet = NULL;
}

/*
 * Remove an entry from the cache
 */
void
s5_delcache(ip)
	register struct inode *ip;
{
	rw_enter(&icache_lock, RW_WRITER);
	remque(ip);
	ip->i_forw = ip->i_back = ip;
	rw_exit(&icache_lock);
}

/*
 * Look up an inode by device, inumber.  If it is in core (in the
 * inode structure), honor the locking protocol.  If it is not in
 * core, read it in from the specified device after freeing any pages.
 * In all cases, a pointer to a VN_HELD inode structure is returned.
 */
/* ARGSUSED */
int
s5_iget(vfsp, fs, ino, ipp, cr)
	register struct vfs *vfsp;
	register struct filsys *fs;
	ino_t ino;
	struct inode **ipp;
	struct cred *cr;
{
	register struct inode *ip;
	register union  ihead *ih;
	register struct buf *bp;
	register struct dinode *dp;
	register struct inode *iq;
	register struct vnode *vp;
	int error;
	int nomem = 0;
	register int i;
	register int ftype;	/* XXX - Remove later on */
	dev_t	vfs_dev;

	/*
	 * Lookup inode in cache.
	 */
	vfs_dev = vfsp->vfs_dev;
loop:
	rw_enter(&icache_lock, RW_READER);
	ASSERT(getfs(vfsp) == fs);

	ih = &ihead[INOHASH(vfsp->vfs_dev, ino)];
	ip = ih->ih_chain[0];
	while (ip != (struct inode *)ih) {
		/*
		 * The icache_lock is used to allow read access to i_number
		 * and i_dev fields of inodes. Updates to these fields
		 * are done while holding the (write) icache_lock and/or
		 * (write) i_contents lock.
		 */
		if (ino == ip->i_number && vfs_dev == ip->i_dev) {
			struct vnode *vp;
			/*
			 * Found the interesting inode, drop cache lock
			 * get exclusive access to inode and make sure
			 * the idenitity still matches.
			 * Hold the vnode so it can't get destroyed
			 * in iinactive before we look at it.
			 */
			vp = ITOV(ip);	/* for locknest */
			VN_HOLD(vp);
			rw_enter(&ip->i_contents, RW_READER);
			if (ino != ip->i_number || vfs_dev != ip->i_dev) {
				/* Idenitity changed, start over */
				rw_exit(&ip->i_contents);
				rw_exit(&icache_lock);
				VN_RELE(vp);
				goto loop;
			}
			mutex_enter(&ip->i_tlock);
			if ((ip->i_flag & IREF) == 0) {
				mutex_enter(&ifree_lock);
				iq = ip->i_freef;
				if (iq)
					iq->i_freeb = ip->i_freeb;
				else
					ifreet = ip->i_freeb;
				*ip->i_freeb = iq;
				ip->i_freef = NULL;
				ip->i_freeb = NULL;
				mutex_exit(&ifree_lock);
			}
			/*
			 * Mark it referenced.
			 */
			ip->i_flag |= IREF;
			mutex_exit(&ip->i_tlock);
			rw_exit(&icache_lock);

			*ipp = ip;
			s5ins.in_hits++;
			if (ino != ip->i_number ||
			    vfsp->vfs_dev != ip->i_dev)
				cmn_err(CE_PANIC, "iget: bad ino or dev\n");
			rw_exit(&ip->i_contents);
			return (0);
		}
		ip = ip->i_forw;
	}
	/*
	 * We didn't find the inode in the cache. Upgrade to a write
	 * lock and add this inode to the cache. The inode may have
	 * slipped into the cache during the lock upgrade (if the
	 * upgrade failed) so we have to check one more time.
	 */
	if (rw_tryupgrade(&icache_lock)) {
		/* Sucessful upgrade, no need to search cache again */
		goto	addentry;
	}
	rw_exit(&icache_lock);
	rw_enter(&icache_lock, RW_WRITER);

	ip = ih->ih_chain[0];
	while (ip != (struct inode *)ih) {
		if (ino == ip->i_number && vfs_dev == ip->i_dev) {
			struct vnode *vp;

			/*
			 * Found the interesting inode, drop cache lock
			 * get exclusive access to inode and make sure
			 * the idenitity still matches.
			 */
			vp = ITOV(ip);	/* for locknest */
			VN_HOLD(vp);
			rw_enter(&ip->i_contents, RW_READER);
			if (ino != ip->i_number || vfs_dev != ip->i_dev) {
				/* Idenitity changed, start over */
				rw_exit(&ip->i_contents);
				rw_exit(&icache_lock);
				VN_RELE(vp);
				goto loop;
			}
			/*
			 * If the inode is on the freelist, remove it
			 * now. This leaves a unref inode in the hash
			 * list but not on the freelist temporarily.
			 */
			mutex_enter(&ip->i_tlock);
			if ((ip->i_flag & IREF) == 0) {
				mutex_enter(&ifree_lock);
				iq = ip->i_freef;
				if (iq)
					iq->i_freeb = ip->i_freeb;
				else
					ifreet = ip->i_freeb;
				*ip->i_freeb = iq;
				ip->i_freef = NULL;
				ip->i_freeb = NULL;
				mutex_exit(&ifree_lock);
			}
			/*
			 * Mark it referenced.
			 */
			ip->i_flag |= IREF;
			mutex_exit(&ip->i_tlock);
			rw_exit(&icache_lock);

			*ipp = ip;
			s5ins.in_hits++;
			if (ino != ip->i_number ||
			    vfsp->vfs_dev != ip->i_dev)
				cmn_err(CE_PANIC, "s5_iget: bad ino or dev\n");
			rw_exit(&ip->i_contents);
			return (0);
		}
		ip = ip->i_forw;
	}
addentry:
	/*
	 * Inode was not in cache.
	 *
	 * If over high-water mark, and no inodes available on freelist
	 * without attached pages, try to free one up from dnlc.
	 */
	s5ins.in_misses++;
tryagain:
	mutex_enter(&ifree_lock);
	if (ino_new >= s5_ninode)  {
		int	purged;
		while (ifreeh == NULL || ITOV(ifreeh)->v_pages) {
			/*
			 * Try to put an inode on the freelist that's
			 * sitting in the dnlc.
			 */
			mutex_exit(&ifree_lock);
			purged = dnlc_fs_purge1(&s5_vnodeops);
			mutex_enter(&ifree_lock);
			if (!purged) {
				break;
			}
		}
	}

	/*
	 * If there's a free one available and it has no pages attached
	 * take it. If we're over the high water mark, take it even if
	 * it has attached pages. Otherwise, make a new one.
	 */
	if (ifreeh &&
	    (nomem || ITOV(ifreeh)->v_pages == NULL ||
	    ino_new >= s5_ninode)) {
		ip = ifreeh;
		vp = ITOV(ip);

		iq = ip->i_freef;
		if (iq)
			iq->i_freeb = &ifreeh;
		ifreeh = iq;
		ip->i_freef = NULL;
		ip->i_freeb = NULL;

		mutex_exit(&ifree_lock);

		if (ip->i_flag & IREF)
			cmn_err(CE_PANIC, "s5_iget: bad i_flag \n");
		rw_enter(&ip->i_contents, RW_WRITER);

		/*
		 * We call s5_syncip() to synchronously destroy all pages
		 * associated with the vnode before re-using it. The pageout
		 * thread may have beat us to this page so our v_count can
		 * be > 0 at this point even though we are on the freelist.
		 *
		 * XXX!
		 * We are still holding the (write) icache_lock. This may be a
		 * bottleneck in the system if s5_syncip really has to wait
		 * for any I/O.
		 */
		mutex_enter(&ip->i_tlock);
		ip->i_flag = (ip->i_flag & IMODTIME) | IREF;
		mutex_exit(&ip->i_tlock);

		VN_HOLD(vp);
		if (s5_syncip(ip, B_INVAL, I_SYNC) != 0) {
			idrop(ip);
			rw_exit(&icache_lock);
			goto loop;
		}

		mutex_enter(&ip->i_tlock);
		ip->i_flag &= ~IMODTIME;
		mutex_exit(&ip->i_tlock);
		/*
		 * The pageout thread may not have had a chance to release
		 * its hold on the vnode (if it was active with this vp),
		 * but the pages should all be invalidated.
		 */
	} else {
		mutex_exit(&ifree_lock);
		/*
		 * Try to get memory for this inode without blocking.
		 * If we can't and there is something on the freelist,
		 * go ahead and use it, otherwise block waiting for
		 * memory holding the icache_lock. We expose a potential
		 * deadlock if all users of memory have to do a s5_iget()
		 * before releasing memory.
		 */
		ip = (struct inode *)kmem_zalloc(sizeof (*ip), KM_NOSLEEP);
		if (ip == NULL) {
			if (ifreeh) {
				nomem = 1;
				goto tryagain;
			} else {
				ip = (struct inode *)
					kmem_zalloc(sizeof (*ip), KM_SLEEP);
			}
		}
		vp = ITOV(ip);

		rw_init(&ip->i_rwlock, NULL, RW_DEFAULT, NULL);
		rw_init(&ip->i_contents, NULL, RW_DEFAULT, NULL);
		mutex_init(&ip->i_tlock, NULL, MUTEX_DEFAULT, NULL);
		mutex_init(&ITOV(ip)->v_lock, NULL, MUTEX_DEFAULT, NULL);

		ip->i_forw = ip;
		ip->i_back = ip;
		ip->i_vnode.v_data = (caddr_t)ip;
		ip->i_vnode.v_op = &s5_vnodeops;
		ip->i_flag = IREF;
		cv_init(&ip->i_wrcv, NULL, CV_DRIVER, NULL);
		s5ins.in_malloc++;
		ino_new++;
		s5ins.in_maxsize = MAX(s5ins.in_maxsize, ino_new);
		rw_enter(&ip->i_contents, RW_WRITER);

		vp->v_count = 1;
	}

	if (vp->v_count < 1)
		cmn_err(CE_PANIC, "s5_iget: v_count < 1\n");
	if (vp->v_pages != NULL)
		cmn_err(CE_PANIC, "s5_iget: v_pages not NULL\n");

	/*
	 * Move the inode on the chain for its new (ino, dev) pair
	 */
	remque(ip);
	insque(ip, ih);

	ip->i_dev = vfsp->vfs_dev;
	ip->i_s5vfs = (struct s5vfs *)vfsp->vfs_data;
	ip->i_devvp = ip->i_s5vfs->vfs_devvp;
	ip->i_number = ino;
	ip->i_nextr = 0;
	ip->i_vcode = 0;
	ip->i_blocks = 0;
	/*
	 * Even though we haven't completed the initialization of the
	 * inode in the cache, we can unlock the cache for other users
	 * at this point. Lookups on this inode will block because
	 * we still hold the (write) contents lock.
	 */
	rw_exit(&icache_lock);
	bp = bread(ip->i_dev, (daddr_t)fsbtodb(fs, itod(ip->i_s5vfs, ino)),
	    (int)ip->i_s5vfs->vfs_bsize);
	/*
	 * Check I/O errors and get vcode
	 */
	if (bp->b_flags & B_ERROR)
		error = EIO;
	else if (IFTOVT((mode_t)ip->i_mode) == VREG)
		error = fs_vcode(vp, &ip->i_vcode);
	else
		error = 0;
	if (error) {
		brelse(bp);
		/*
		 * The inode may not contain anything useful. Mark it as
		 * having an error and let anyone else who was waiting for
		 * this know there was an error. Callers waiting for
		 * access to this inode in s5_iget will find
		 * the i_number == 0, so there won't be a match.
		 * It remains in the cache. Put it back on the freelist.
		 */
		mutex_enter(&vp->v_lock);
		vp->v_count--;
		mutex_exit(&vp->v_lock);
		ip->i_number = 0;
		ip->i_flag = 0;
		/* Put the inode at the front of the freelist */
		mutex_enter(&ifree_lock);
		if (ifreeh)
			ifreeh->i_freeb = &ip->i_freef;
		else
			ifreet = &ip->i_freef;
		ip->i_freeb = &ifreeh;
		ip->i_freef = ifreeh;
		ifreeh = ip;
		mutex_exit(&ifree_lock);

		rw_exit(&ip->i_contents);
		return (error);
	}

	{	register char *p1, *p2;

		dp = (struct dinode *)bp->b_un.b_addr;
		dp += itoo(ip->i_s5vfs, ino);
		ip->i_nlink = dp->di_nlink;
		ip->i_uid = dp->di_uid;
		ip->i_gid = dp->di_gid;
		ip->i_size = dp->di_size;
		ip->i_mode = dp->di_mode;
		ip->i_atime = dp->di_atime;
		ip->i_mtime = dp->di_mtime;
		ip->i_ctime = dp->di_ctime;
		ip->i_number = ino;
		ip->i_nextr = 0;
		ip->i_gen = dp->di_gen;
		p1 = (char *)ip->i_addr;
		p2 = (char *)dp->di_addr;
		for (i = 0; i < NADDR; i++) {
#ifdef _BIG_ENDIAN
			*p1++ = 0;
#endif
			*p1++ = *p2++;
			*p1++ = *p2++;
			*p1++ = *p2++;
#ifdef _LITTLE_ENDIAN
			*p1++ = 0;
#endif
		}
	}

	ftype = ip->i_mode & IFMT;
	if (ftype == IFBLK || ftype == IFCHR) {
		if (ip->i_bcflag & NDEVFORMAT)
			ip->i_rdev = makedevice(ip->i_major, ip->i_minor);
		else
			ip->i_rdev = expdev(ip->i_oldrdev);
	} else if (ip->i_mode & IFNAM)
		ip->i_rdev = ip->i_oldrdev;

	/*
	 * Fill in the rest.  Don't bother with the vnode lock because nobody
	 * should be looking at this vnode.  We have already invalidated the
	 * pages if it had any so pageout shouldn't be referencing this vnode
	 * and we are holding the write contents lock so a look up can't use
	 * the vnode.
	 */
	vp->v_vfsp = vfsp;
	vp->v_stream = NULL;
	vp->v_pages = NULL;
	vp->v_filocks = NULL;
	vp->v_shrlocks = NULL;
	vp->v_type = IFTOVT((mode_t)ip->i_mode);
	vp->v_rdev = ip->i_rdev;
	if (ino == (ino_t)S5ROOTINO)
		vp->v_flag = VROOT;
	else
		vp->v_flag = 0;
	brelse(bp);
	*ipp = ip;
	rw_exit(&ip->i_contents);
	return (0);
}
/*
 * Update times and vrele associated vnode
 */
void
s5_iput(ip)
	register struct inode *ip;
{
	ITIMES(ip);
	VN_RELE(ITOV(ip));
}

/*
 * Release associated vnode.
 */
void
irele(ip)
	register struct inode *ip;
{
	ITIMES(ip);
	VN_RELE(ITOV(ip));
}

/*
 * Drop inode without going through the normal
 * chain of unlocking and releasing.
 */
void
idrop(ip)
	register struct inode *ip;
{
	register struct vnode *vp = ITOV(ip);

	ASSERT(RW_WRITE_HELD(&ip->i_contents));
	mutex_enter(&vp->v_lock);
	if (vp->v_count > 1) {
		/* Someone else will release it when they're done */
		vp->v_count--;
		mutex_exit(&vp->v_lock);
		rw_exit(&ip->i_contents);
		return;
	}
	vp->v_count = 0;
	mutex_exit(&vp->v_lock);

	mutex_enter(&ip->i_tlock);
	/* retain the fast symlnk and imodtime flag */
	ip->i_flag &= IMODTIME;
	mutex_exit(&ip->i_tlock);
	/*
	 *  if inode is invalid or there is no page associated with
	 *  this inode, put the inode in the front of the free list
	 */
	mutex_enter(&ifree_lock);
	if (vp->v_pages == NULL || ip->i_mode == 0) {
		if (ifreeh)
			ifreeh->i_freeb = &ip->i_freef;
		else
			ifreet = &ip->i_freef;
		ip->i_freeb = &ifreeh;
		ip->i_freef = ifreeh;
		ifreeh = ip;
	} else {
		/*
		 * Otherwise, put the inode back on the end of the free list.
		 */
		if (ifreeh) {
			*ifreet = ip;
			ip->i_freeb = ifreet;
		} else {
			ifreeh = ip;
			ip->i_freeb = &ifreeh;
		}
		ip->i_freef = NULL;
		ifreet = &ip->i_freef;
	}
	mutex_exit(&ifree_lock);
	rw_exit(&ip->i_contents);
}

/*
 * Vnode is no longer referenced, write the inode out
 * and if necessary, truncate and deallocate the file.
 */
void
s5_iinactive(ip, cr)
	struct inode *ip;
	struct cred *cr;
{
	register struct vnode *vp;
	mode_t mode;
	int	busy = 0;

	/*
	 * Get exclusive access to inode data.
	 */
	rw_enter(&ip->i_contents, RW_WRITER);
	/*
	 * Make sure no one reclaimed the inode before we put
	 * it on the freelist or destroy it. We keep our 'hold'
	 * on the vnode from vn_rele until we are ready to
	 * do something with the inode (freelist/destroy).
	 *
	 * Pageout may put a VN_HOLD/VN_RELE at anytime during this
	 * operation via an async putpage, so we must make sure
	 * we don't free/destroy the inode more than once. s5_iget
	 * may also put a VN_HOLD on the inode before it grabs
	 * the i_contents lock. This is done so we don't kmem_free
	 * an inode that a thread is waiting on.
	 */
	vp = ITOV(ip);
	mutex_enter(&vp->v_lock);
	if (vp->v_count < 1)
		cmn_err(CE_PANIC, "s5iinactive: v_count < 1\n");
	if (vp->v_count > 1 || (ip->i_flag & IREF) == 0) {
		vp->v_count--;	/* release our hold from vn_rele */
		mutex_exit(&vp->v_lock);
		rw_exit(&ip->i_contents);
		return;
	}
	mutex_exit(&vp->v_lock);

	/*
	 * For forced umount case: if s5vfs is NULL, the contents of
	 * the inode and all the pages have already been pushed back
	 * to disk. It can be safely destroyed.
	 */
	if (ip->i_s5vfs == NULL) {
		rw_exit(&ip->i_contents);

		rw_enter(&icache_lock, RW_WRITER);
		ino_new--;
		rw_exit(&icache_lock);

		cv_destroy(&ip->i_wrcv);  /* throttling */
		rw_destroy(&ip->i_rwlock);
		rw_destroy(&ip->i_contents);
		kmem_free(ip, sizeof (*ip));
		s5ins.in_mfree++;
		return;
	}
	if (ITOF(ip)->s_ronly == 0) {
		if (ip->i_nlink <= 0) {
			ip->i_nlink = 1;	/* prevent free-ing twice */
			ip->i_gen++;
			(void) s5_itrunc(ip, (ulong_t)0, cr);
			mode = ip->i_mode;
			ip->i_mode = 0;
			ip->i_uid = 0;
			ip->i_gid = 0;
			ip->i_rdev = 0;	/* Zero in core version of rdev */
			ip->i_oldrdev = 0;
			mutex_enter(&ip->i_tlock);
			ip->i_flag |= IUPD|ICHG;
			mutex_exit(&ip->i_tlock);
			s5_ifree(ip, ip->i_number, mode);
			s5_iupdat(ip, 0);

		} else if (!IS_SWAPVP(vp)) {
			/*
			 * Write the inode out if dirty. Pages are
			 * written back and put on the freelist.
			 */
			(void) s5_syncip(ip, B_FREE | B_ASYNC, 0);
			/*
			 * Do nothing if inode is now busy -- inode may
			 * have gone busy because s5_syncip
			 * releases/reacquires the i_contents lock
			 */
			mutex_enter(&vp->v_lock);
			if (vp->v_count > 1) {
				vp->v_count--;
				mutex_exit(&vp->v_lock);
				rw_exit(&ip->i_contents);
				return;
			}
			mutex_exit(&vp->v_lock);
		} else
			s5_iupdat(ip, 0);
	}

	mutex_enter(&ip->i_tlock);
	/* retain the imodtime flag */
	ip->i_flag &= IMODTIME;
	mutex_exit(&ip->i_tlock);
	/*
	 * Put the inode on the end of the free list.
	 * Possibly in some cases it would be better to
	 * put the inode at the head of the free list,
	 * (e.g.: where i_mode == 0 || i_number == 0)
	 * but I will think about that later.
	 * (i_number is rarely 0 - only after an i/o error in s5_iget,
	 * where i_mode == 0, the inode will probably be wanted
	 * again soon for an ialloc, so possibly we should keep it)
	 */

	/*
	 * If inode is invalid or there is no page associated with
	 * this inode, put the inode in the front of the free list.
	 * Since we have a VN_HOLD on the vnode, and checked that it
	 * wasn't already on the freelist when we entered, we can safely
	 * put it on the freelist even if another thread puts a VN_HOLD
	 * on it (pageout/s5_iget).
	 */
tryagain:
	if (vp->v_pages) {
		mutex_enter(&vp->v_lock);
		vp->v_count--;
		mutex_exit(&vp->v_lock);

		mutex_enter(&ifree_lock);
		if (ifreeh) {
			*ifreet = ip;
			ip->i_freeb = ifreet;
		} else {
			ifreeh = ip;
			ip->i_freeb = &ifreeh;
		}
		ip->i_freef = NULL;
		ifreet = &ip->i_freef;
		mutex_exit(&ifree_lock);
		rw_exit(&ip->i_contents);

		s5ins.in_frback++;
	} else if (busy || ino_new < s5_ninode) {
		/*
		 * We're not over our high water mark, or it's
		 * not safe to kmem_free the inode, so put it
		 * on the freelist.
		 */
		mutex_enter(&vp->v_lock);
		if (vp->v_pages != NULL)
			cmn_err(CE_PANIC, "s5_iinactive: v_pages not NULL\n");
		vp->v_count--;
		mutex_exit(&vp->v_lock);

		mutex_enter(&ifree_lock);
		if (ifreeh) {
			ip->i_freef = ifreeh;
			ifreeh->i_freeb = &ip->i_freef;
		} else {
			ip->i_freef = NULL;
			ifreet = &ip->i_freef;
		}
		ifreeh = ip;
		ip->i_freeb = &ifreeh;
		mutex_exit(&ifree_lock);
		rw_exit(&ip->i_contents);

		s5ins.in_frfront++;
	} else {
		if (vp->v_pages != NULL)
			cmn_err(CE_PANIC, "s5_iinactive: v_pages not NULL\n");
		/*
		 * Try to free the inode. We must make sure
		 * it's o.k. to destroy this inode. We can't destroy
		 * if a thread is waiting for this inode. If we can't get the
		 * icache_lock now, put it back on the freelist.
		 */
		if (!rw_tryenter(&icache_lock, RW_WRITER)) {
			busy = 1;
			goto	tryagain;
		}

		mutex_enter(&vp->v_lock);
		if (vp->v_count > 1) {
			/* inode is wanted in s5_iget */
			busy = 1;
			mutex_exit(&vp->v_lock);
			rw_exit(&icache_lock);
			goto	tryagain;
		}
		mutex_exit(&vp->v_lock);
		remque(ip);
		ino_new--;
		rw_exit(&icache_lock);
		s5_iupdat(ip, 0);
		cv_destroy(&ip->i_wrcv);  /* throttling */
		rw_destroy(&ip->i_rwlock);
		rw_destroy(&ip->i_contents);
		kmem_free(ip, sizeof (*ip));

		s5ins.in_mfree++;
	}
}

/*
 * Check accessed and update flags on an inode structure.
 * If any are on, update the inode with the (unique) current time.
 * If waitfor is given, insure I/O order so wait for write to complete.
 */
void
s5_iupdat(struct inode *ip, int waitfor)
{
	register struct buf *bp;
	register struct filsys *fp;
	struct dinode *dp;
	ushort_t flag;
	register char *p1, *p2;
	register int i;

	ASSERT(RW_WRITE_HELD(&ip->i_contents) || RW_WRITE_HELD(&ip->i_rwlock));

	/*
	 * EFT check - if we're asking the filesystem to write out uids
	 * or gids that are simply too large for the on-disk format to
	 * represent, then something bad happened.
	 */
	ASSERT((ulong_t)ip->i_uid <= (ulong_t)USHRT_MAX);
	ASSERT((ulong_t)ip->i_gid <= (ulong_t)USHRT_MAX);

	/*
	 * Return if file system has been forcibly umounted.
	 */
	if (ip->i_s5vfs == NULL)
		return;

	fp = ITOF(ip);
	flag = ip->i_flag;	/* Atomic read */
	if ((flag & (IUPD|IACC|ICHG|IMOD|IMODACC)) != 0) {
		if (fp->s_ronly)
			return;

		bp = bread(ip->i_dev,
		    (daddr_t)fsbtodb(fp, itod(ip->i_s5vfs, ip->i_number)),
		    (int)ip->i_s5vfs->vfs_bsize);
		if (bp->b_flags & B_ERROR) {
			brelse(bp);
			return;
		}

		mutex_enter(&ip->i_tlock);
		if (ip->i_flag & (IUPD|IACC|ICHG))
			IMARK(ip);
		ip->i_flag &= ~(IUPD|IACC|ICHG|IMOD|IMODACC);
		mutex_exit(&ip->i_tlock);

		dp = (struct dinode *)bp->b_un.b_addr +
		    itoo(ip->i_s5vfs, ip->i_number);

		dp->di_mode = ip->i_mode;
		dp->di_nlink = ip->i_nlink;
		dp->di_uid = ip->i_uid;
		dp->di_gid = ip->i_gid;
		dp->di_size = ip->i_size;
		p1 = (char *)dp->di_addr;
		p2 = (char *)ip->i_addr;
		for (i = 0; i < NADDR; i++) {
#ifdef _BIG_ENDIAN
			p2++;
#endif
			*p1++ = *p2++;
			*p1++ = *p2++;
			*p1++ = *p2++;
#ifdef _LITTLE_ENDIAN
			p2++;
#endif
		}
		dp->di_gen = ip->i_gen;
		dp->di_atime = ip->i_atime;
		dp->di_mtime = ip->i_mtime;
		dp->di_ctime = ip->i_ctime;

		if (waitfor)
			bwrite(bp);
		else
			bdwrite(bp);
	}
}

#define	SINGLE	0	/* index of single indirect block */
#define	DOUBLE	1	/* index of double indirect block */
#define	TRIPLE	2	/* index of triple indirect block */

/*
 * Release blocks associated with the inode ip and
 * stored in the indirect block bn.  Blocks are free'd
 * in LIFO order up to (but not including) lastbn.  If
 * level is greater than SINGLE, the block is an indirect
 * block and recursive calls to indirtrunc must be used to
 * cleanse other indirect blocks.
 *
 * N.B.: triple indirect blocks are untested.
 */
static long
indirtrunc(ip, bn, lastbn, level)
	register struct inode *ip;
	daddr_t bn, lastbn;
	int level;
{
	register int i;
	struct buf *bp, *copy;
	register daddr_t *bap;
	register struct filsys *fs = ITOF(ip);
	daddr_t nb, last;
	long factor;
	int blocksreleased = 0, nblocks;
	int bsize = ip->i_s5vfs->vfs_bsize;

	ASSERT(RW_WRITE_HELD(&ip->i_contents));
	/*
	 * Calculate index in current block of last
	 * block to be kept.  -1 indicates the entire
	 * block so we need not calculate the index.
	 */
	factor = 1;
	for (i = SINGLE; i < level; i++)
		factor *= NINDIR(ip->i_s5vfs);
	last = lastbn;
	if (lastbn > 0)
		last /= factor;
	nblocks = btodb(bsize);
	/*
	 * Get buffer of block pointers, zero those
	 * entries corresponding to blocks to be free'd,
	 * and update on disk copy first.
	 */
	copy = ngeteblk(bsize);
	bp = bread(ip->i_dev, (daddr_t)fsbtodb(fs, bn),
		(int)bsize);
	if (bp->b_flags & B_ERROR) {
		brelse(copy);
		brelse(bp);
		return (0);
	}
	bap = (daddr_t *)bp->b_un.b_daddr;
	bcopy((caddr_t)bap, (caddr_t)copy->b_un.b_daddr, (uint_t)bsize);
	bzero((caddr_t)&bap[last + 1],
	    (uint_t)(NINDIR(ip->i_s5vfs) - (last + 1)) * sizeof (daddr_t));
	bwrite(bp);
	bp = copy, bap = (daddr_t *)bp->b_un.b_daddr;

	/*
	 * Recursively free totally unused blocks.
	 */
	for (i = NINDIR(ip->i_s5vfs) - 1; i > last; i--) {
		nb = bap[i];
		if (nb == 0)
			continue;
		if (level > SINGLE)
			blocksreleased +=
			    indirtrunc(ip, nb, (daddr_t)-1, level - 1);
		(void) s5_free(ip, nb, (off_t)bsize);
		blocksreleased += nblocks;
	}

	/*
	 * Recursively free last partial block.
	 */
	if (level > SINGLE && lastbn >= 0) {
		last = lastbn % factor;
		nb = bap[i];
		if (nb != 0)
			blocksreleased += indirtrunc(ip, nb, last, level - 1);
	}
	brelse(bp);
	return (blocksreleased);
}

/*
 * Truncate the inode ip to at most length size.
 * Free affected disk blocks -- the blocks of the
 * file are removed in reverse order.
 *
 * N.B.: triple indirect blocks are untested.
 */
s5_itrunc(oip, length, cr)
	register struct inode *oip;
	ulong_t length;
	struct cred *cr;
{
	register struct inode *ip;
	register daddr_t lastblock;
	register struct s5vfs *s5vfs = oip->i_s5vfs;
	register off_t bsize = s5vfs->vfs_bsize;
	register int boff;
	daddr_t bn, lastiblock[NIADDR];
	int level;
	long nblocks, blocksreleased = 0;
	register int i;
	struct inode tip;
	extern int s5_putapage();

	ASSERT(RW_WRITE_HELD(&oip->i_contents));
	/*
	 * We only allow truncation of regular files and directories
	 * to arbritary lengths here.  In addition, we allow symbolic
	 * links to be truncated only to zero length.  Other inode
	 * types cannot have their length set here.  Disk blocks are
	 * being dealt with - especially device inodes where
	 * ip->i_oldrdev is actually being stored in ip->i_addr[0]!
	 */
	i = oip->i_mode & IFMT;
	if (i == IFIFO)
		return (0);
	if (i != IFREG && i != IFDIR && !(i == IFLNK && length == 0))
		return (EINVAL);
	if (length == oip->i_size) {
	/* update ctime and mtime to please POSIX tests */
		mutex_enter(&oip->i_tlock);
		oip->i_flag |= ICHG |IUPD;
		mutex_exit(&oip->i_tlock);
		return (0);
	}

	boff = blkoff(s5vfs, length);

	if (length > oip->i_size) {
		int err;

		/*
		 * Trunc up case.  BMAPALLOC will insure that the right blocks
		 * are allocated.  This includes doing any work needed for
		 * allocating the last block.
		 */
		if (boff == 0)
			err = BMAPALLOC(oip, length - 1, (int)bsize, cr);
		else
			err = BMAPALLOC(oip, length - 1, boff, cr);

		if (err == 0) {
			/*
			 * Make sure we zero out the remaining bytes of
			 * the page in case a mmap scribbled on it. We
			 * can't prevent a mmap from writing beyond EOF
			 * on the last page of a file.
			 *
			 * Don't hold i_contents in case pvn_vpzero calls
			 * s5_getpage.
			 */
			if ((boff = blkoff(s5vfs, oip->i_size)) != 0) {
				rw_exit(&oip->i_contents);
				pvn_vpzero(ITOV(oip), (offset_t)oip->i_size,
				    (uint_t)(bsize - boff));
				rw_enter(&oip->i_contents, RW_WRITER);
			}
			oip->i_size = length;
			mutex_enter(&oip->i_tlock);
			oip->i_flag |= ICHG;
			ITIMES_NOLOCK(oip);
			mutex_exit(&oip->i_tlock);
		}

		return (err);
	}

	/*
	 * Update the pages of the file.  If the file is not being
	 * truncated to a block boundary, the contents of the
	 * pages following the end of the file must be zero'ed
	 * in case it ever become accessable again because
	 * of subsequent file growth.
	 */
	if (boff == 0) {
		(void) pvn_vplist_dirty(ITOV(oip), (offset_t)length,
		s5_putapage, B_INVAL | B_TRUNC, CRED());
	} else {
		int err;

		/*
		 * Make sure that the last block is properly allocated.
		 * We only really have to do this if the last block is
		 * actually allocated.  Just to be sure, we do it now
		 * independent of current allocation.
		 */
		err = BMAPALLOC(oip, length - 1, boff, cr);
		if (err)
			return (err);
		/*
		 * Don't hold i_contents here since we may end up calling
		 * s5_getpage for the zeroed pages.
		 */
		rw_exit(&oip->i_contents);
		pvn_vpzero(ITOV(oip), (offset_t)length, (uint_t)(bsize - boff));
		rw_enter(&oip->i_contents, RW_WRITER);

		(void) pvn_vplist_dirty(ITOV(oip), (offset_t)length,
		s5_putapage, B_INVAL | B_TRUNC, CRED());
	}

	/*
	 * Calculate index into inode's block list of
	 * last direct and indirect blocks (if any)
	 * which we want to keep.  Lastblock is -1 when
	 * the file is truncated to 0.
	 */
	lastblock = lblkno(s5vfs, length + bsize - 1) - 1;
	lastiblock[SINGLE] = lastblock - NDADDR;
	lastiblock[DOUBLE] = lastiblock[SINGLE] - NINDIR(s5vfs);
	lastiblock[TRIPLE] = lastiblock[DOUBLE] - NINDIR(s5vfs)*NINDIR(s5vfs);
	nblocks = btodb(bsize);

	/*
	 * Update file and block pointers
	 * on disk before we start freeing blocks.
	 * If we crash before free'ing blocks below,
	 * the blocks will be returned to the free list.
	 * lastiblock values are also normalized to -1
	 * for calls to indirtrunc below.
	 */
	tip = *oip;			/* structure copy */
	ip = &tip;

	for (level = TRIPLE; level >= SINGLE; level--)
		if (lastiblock[level] < 0) {
			oip->i_addr[NDADDR+SINGLE+level] = 0;
			lastiblock[level] = -1;
		}
	for (i = NDADDR - 1; i > lastblock; i--)
		oip->i_addr[i] = 0;

	oip->i_size = length;
	mutex_enter(&oip->i_tlock);
	oip->i_flag |= ICHG|IUPD;
	mutex_exit(&oip->i_tlock);
	s5_iupdat(oip, 1);			/* do sync inode update */

	/*
	 * Indirect blocks first.
	 */
	for (level = TRIPLE; level >= SINGLE; level--) {
		bn = ip->i_addr[NDADDR+SINGLE+level];
		if (bn != 0) {
			blocksreleased +=
			    indirtrunc(ip, bn, lastiblock[level], level);
			if (lastiblock[level] < 0) {
				ip->i_addr[NDADDR+SINGLE+level] = 0;
				(void) s5_free(ip, bn, bsize);
				blocksreleased += nblocks;
			}
		}
		if (lastiblock[level] >= 0)
			goto done;
	}

	/*
	 * Direct blocks.
	 */
	for (i = NDADDR - 1; i > lastblock; i--) {
		bn = ip->i_addr[i];
		if (bn == 0)
			continue;
		ip->i_addr[i] = 0;
		(void) s5_free(ip, bn, bsize);
		blocksreleased += nblocks;
	}

done:
/* BEGIN PARANOIA */
	for (level = 0; level < NADDR; level++)
		if (ip->i_addr[level] != oip->i_addr[level])
			cmn_err(CE_PANIC, "s5_itrunc: inconsistent block");
/* END PARANOIA */

	oip->i_blocks++;	/* indicate change */

	mutex_enter(&oip->i_tlock);
	oip->i_flag |= ICHG;
	mutex_exit(&oip->i_tlock);
	return (0);
}

/*
 * Remove any inodes in the inode cache belonging to dev
 *
 * There should not be any active ones, return error if any are found but
 * still invalidate others (N.B.: this is a user error, not a system error).
 *
 * Also, count the references to dev by block devices - this really
 * has nothing to do with the object of the procedure, but as we have
 * to scan the inode table here anyway, we might as well get the
 * extra benefit.
 *
 * This is called from umount1()/s5_vfsops.c when dev is being unmounted.
 */
s5_iflush(vfsp)
	struct vfs *vfsp;
{
	register struct inode *ip;
	register struct inode *next;
	register int open = 0;
	struct vnode *vp, *rvp;
	register union  ihead *ih;
	dev_t dev = vfsp->vfs_dev;

	rvp = ((struct s5vfs *)(vfsp->vfs_data))->vfs_root;
	/*
	 * We may remove inodes from the cache so hold write version
	 * of icache_lock.
	 */
	rw_enter(&icache_lock, RW_WRITER);
	for (ih = ihead; ih < &ihead[INOHSZ]; ih++) {

		next = ih->ih_chain[0];
		while (next != (struct inode *)ih) {
			ip = next;
			next = ip->i_forw;
			if (ip->i_dev != dev)
				continue;
			vp = ITOV(ip);
			/*
			 * root inode is processed by the caller
			 */
			if (vp == rvp) {
				if (vp->v_count > 1)
					open = -1;
				continue;
			}
			if (ip->i_flag & IREF) {
				/*
				 * Set error indicator for return value,
				 * but continue invalidating other
				 * inodes.
				 */
				open = -1;
				continue;
			}
			/*
			 * Shouldn't be locked since it's not IREF
			 */
			rw_enter(&ip->i_contents, RW_WRITER);
			remque(ip);
			ip->i_forw = ip;
			ip->i_back = ip;
			/*
			 * Hold the vnode since its not done
			 * in VOP_PUTPAGE anymore.
			 */
			VN_HOLD(vp);
			/*
			 * XXX Synchronous write holding
			 * cache lock
			 */
			(void) s5_syncip(ip, B_INVAL, I_SYNC);
			rw_exit(&ip->i_contents);
			VN_RELE(vp);
		}
	}
	rw_exit(&icache_lock);
	return (open);
}


/*
 * Check mode permission on inode.  Mode is READ, WRITE or EXEC.
 * In the case of WRITE, the read-only status of the file system
 * is checked.  The mode is shifted to select the owner/group/other
 * fields.  The super user is granted all permissions except
 * writing to read-only file systems.
 */
int
s5_iaccess(ip, mode, cr)
	register struct inode *ip;
	register int mode;
	register struct cred *cr;
{
	if (mode & IWRITE) {
		/*
		 * Disallow write attempts on read-only
		 * file systems, unless the file is a block
		 * or character device or a FIFO.
		 */
		if (ITOF(ip)->s_ronly != 0) {
			if ((ip->i_mode & IFMT) != IFCHR &&
			    (ip->i_mode & IFMT) != IFBLK &&
			    (ip->i_mode & IFMT) != IFIFO) {
				return (EROFS);
			}
		}
	}
	/*
	 * If you're the super-user,
	 * you always get access.
	 */
	if (cr->cr_uid == 0)
		return (0);
	/*
	 * Access check is based on only
	 * one of owner, group, public.
	 * If not owner, then check group.
	 * If not a member of the group, then
	 * check public access.
	 */
	if (cr->cr_uid != ip->i_uid) {
		mode >>= 3;
		if (!groupmember((uid_t)ip->i_gid, cr))
			mode >>= 3;
	}
	if ((ip->i_mode & mode) == mode)
		return (0);
	return (EACCES);
}

/*
 * remove an inode from the free list
 */
void
s5_irmfree(ip)
struct inode *ip;
{
	struct inode *iq;

	mutex_enter(&ifree_lock);
	iq = ip->i_freef;
	if (iq)
		iq->i_freeb = ip->i_freeb;
	else
		ifreet = ip->i_freeb;
	*ip->i_freeb = iq;
	ip->i_freef = NULL;
	ip->i_freeb = NULL;
	mutex_exit(&ifree_lock);
}


/*
 * Add an inode to the front of the free list.  If it exceeds high
 * water mark, kmem_free it.
 * NOTE: Caller must make sure that there are no pages left in the inode.
 */
void
s5_iaddfree(ip)
struct inode *ip;
{
	struct vnode *vp;

	ASSERT(RW_WRITE_HELD(&icache_lock));

	vp = ITOV(ip);

	if (vp->v_count == 0) {
		/*
		 * Over the high water mark, proceed to free it
		 */
		if (ino_new >= s5_ninode) {
			ino_new--;
			kmem_free(ip, sizeof (*ip));
			return;
		}
	}

	/*
	 * Put the inode back to the free chain
	 */
	mutex_enter(&ifree_lock);
	if (ifreeh) {
		ip->i_freef = ifreeh;
		ifreeh->i_freeb = &ip->i_freef;
	} else {
		ip->i_freef = NULL;
		ifreet = &ip->i_freef;
	}
	ifreeh = ip;
	ip->i_freeb = &ifreeh;
	mutex_exit(&ifree_lock);
	s5ins.in_frfront++;
}

/*
 * Mark inode with the current time
 */
void
s5_imark(ip)
	struct inode *ip;
{
	time_t hrtime = hrestime.tv_sec;

	ASSERT(MUTEX_HELD(&(ip)->i_tlock));

	if ((ip)->i_flag & IACC)
		(ip)->i_atime = hrtime;
	if ((ip)->i_flag & IUPD) {
		(ip)->i_mtime = hrtime;
		(ip)->i_flag |= IMODTIME;
	}
	if ((ip)->i_flag & ICHG) {
		(ip)->i_ctime = hrtime;
	}
}

/*
 * Update timestamps in inode
 */
void
s5_itimes_nolock(ip)
	struct inode *ip;
{
	if ((ip)->i_flag & (IUPD|IACC|ICHG)) {
		if (((ip)->i_flag & (IUPD|IACC|ICHG)) == IACC)
			(ip)->i_flag |= IMODACC;
		else
			(ip)->i_flag |= IMOD;

		s5_imark(ip);

		(ip)->i_flag &= ~(IACC|IUPD|ICHG);
	}
}

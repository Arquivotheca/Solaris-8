/*
 * Copyright (c) 1986-1990,1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

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
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma ident	"@(#)ufs_vnops.c	2.311	99/11/19 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/ksynch.h>
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
#include <sys/dnlc.h>
#include <sys/conf.h>
#include <sys/mman.h>
#include <sys/pathname.h>
#include <sys/debug.h>
#include <sys/vmsystm.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/filio.h>

#include <sys/fs/ufs_fs.h>
#include <sys/fs/ufs_lockfs.h>
#include <sys/fs/ufs_filio.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_fsdir.h>
#include <sys/fs/ufs_quota.h>
#include <sys/fs/ufs_log.h>
#include <sys/fs/ufs_trans.h>
#include <sys/fs/ufs_panic.h>
#include <sys/fs/ufs_bio.h>
#include <sys/dirent.h>		/* must be AFTER <sys/fs/fsdir.h>! */
#include <sys/errno.h>

#include <sys/filio.h>		/* _FIOIO */

#include <vm/hat.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/seg_kmem.h>
#include <vm/rm.h>
#include <sys/swap.h>

#include <fs/fs_subr.h>

#define	ISVDEV(t) \
	((t) == VCHR || (t) == VBLK || (t) == VFIFO)

static struct instats ins;

static 	int ufs_getpage_ra(struct vnode *, u_offset_t, struct seg *, caddr_t);
static	int ufs_getpage_miss(struct vnode *, u_offset_t, size_t, struct seg *,
		caddr_t, struct page **, size_t, enum seg_rw, int);
static	int ufs_open(struct vnode **, int, struct cred *);
static	int ufs_close(struct vnode *, int, int, offset_t, struct cred *);
static	int ufs_read(struct vnode *, struct uio *, int, struct cred *);
static	int ufs_write(struct vnode *, struct uio *, int, struct cred *);
static	int ufs_ioctl(struct vnode *, int, intptr_t, int, struct cred *, int *);
static	int ufs_getattr(struct vnode *, struct vattr *, int, struct cred *);
static	int ufs_setattr(struct vnode *, struct vattr *, int, struct cred *);
static	int ufs_access(struct vnode *, int, int, struct cred *);
static	int ufs_lookup(struct vnode *, char *, struct vnode **,
		struct pathname *, int, struct vnode *, struct cred *);
static	int ufs_create(struct vnode *, char *, struct vattr *, enum vcexcl,
			int, struct vnode **, struct cred *, int);
static	int ufs_remove(struct vnode *, char *, struct cred *);
static	int ufs_link(struct vnode *, struct vnode *, char *, struct cred *);
static	int ufs_rename(struct vnode *, char *, struct vnode *, char *,
			struct cred *);
static	int ufs_mkdir(struct vnode *, char *, struct vattr *, struct vnode **,
			struct cred *);
static	int ufs_rmdir(struct vnode *, char *, struct vnode *, struct cred *);
static	int ufs_readdir(struct vnode *, struct uio *, struct cred *, int *);
static	int ufs_symlink(struct vnode *, char *, struct vattr *, char *,
			struct cred *);
static	int ufs_readlink(struct vnode *, struct uio *, struct cred *);
static	int ufs_fsync(struct vnode *, int, struct cred *);
static	void ufs_inactive(struct vnode *, struct cred *);
static	int ufs_fid(struct vnode *, struct fid *);
static	void ufs_rwlock(struct vnode *, int);
static	void ufs_rwunlock(struct vnode *, int);
static	int ufs_seek(struct vnode *, offset_t, offset_t *);
static	int ufs_frlock(struct vnode *, int, struct flock64 *, int, offset_t,
			struct cred *);
static  int ufs_space(struct vnode *, int, struct flock64 *, int, offset_t,
		struct cred *);
static	int ufs_getpage(struct vnode *, offset_t, size_t, uint_t *,
		struct page **, size_t, struct seg *, caddr_t,
		enum seg_rw, struct cred *);
static	int ufs_putpage(struct vnode *, offset_t, size_t, int, struct cred *);
static	int ufs_putpages(struct vnode *, offset_t, size_t, int, struct cred *);
static	int ufs_map(struct vnode *, offset_t, struct as *, caddr_t *, size_t,
			uchar_t, uchar_t, uint_t, struct cred *);
static	int ufs_addmap(struct vnode *, offset_t, struct as *, caddr_t,  size_t,
			uchar_t, uchar_t, uint_t, struct cred *);
static	int ufs_delmap(struct vnode *, offset_t, struct as *, caddr_t,  size_t,
			uint_t, uint_t, uint_t, struct cred *);
static	int ufs_poll(vnode_t *, short, int, short *, struct pollhead **);
static	int ufs_dump(vnode_t *, caddr_t, int, int);
static	int ufs_l_pathconf(struct vnode *, int, ulong_t *, struct cred *);
static	int ufs_pageio(struct vnode *, struct page *, u_offset_t, size_t, int,
			struct cred *);
static	int ufs_dump(vnode_t *, caddr_t, int, int);
static	int ufs_dumpctl(vnode_t *, int, int *);
static	daddr32_t *save_dblks(struct inode *, daddr32_t *,
			daddr32_t *, int, int);
static	int ufs_getsecattr(struct vnode *, vsecattr_t *, int, struct cred *);
static	int ufs_setsecattr(struct vnode *, vsecattr_t *, int, struct cred *);

/*
 * For lockfs: ulockfs begin/end is now inlined in the ufs_xxx functions.
 *
 * XXX - ULOCKFS in fs_pathconf and ufs_ioctl is not inlined yet.
 */
struct vnodeops ufs_vnodeops = {
	ufs_open,	/* will not be blocked by lockfs */
	ufs_close,	/* will not be blocked by lockfs */
	ufs_read,
	ufs_write,
	ufs_ioctl,
	fs_setfl,
	ufs_getattr,
	ufs_setattr,
	ufs_access,
	ufs_lookup,
	ufs_create,
	ufs_remove,
	ufs_link,
	ufs_rename,
	ufs_mkdir,
	ufs_rmdir,
	ufs_readdir,
	ufs_symlink,
	ufs_readlink,
	ufs_fsync,
	ufs_inactive,	/* will not be blocked by lockfs */
	ufs_fid,
	ufs_rwlock,	/* will not be blocked by lockfs */
	ufs_rwunlock,	/* will not be blocked by lockfs */
	ufs_seek,
	fs_cmp,
	ufs_frlock,
	ufs_space,
	fs_nosys,	/* realvp */
	ufs_getpage,
	ufs_putpage,
	ufs_map,
	ufs_addmap,	/* will not be blocked by lockfs */
	ufs_delmap,	/* will not be blocked by lockfs */
	ufs_poll,	/* will not be blocked by lockfs */
	ufs_dump,	/* dump */
	ufs_l_pathconf,
	ufs_pageio,
	ufs_dumpctl,
	fs_dispose,
	ufs_setsecattr,
	ufs_getsecattr,
	fs_shrlock	/* shrlock */
};

/*
 * Created by ufs_dumpctl() to store a file's disk block info into memory.
 * Used by ufs_dump() to dump data to disk directly.
 */
struct dump {
	struct inode	*ip;		/* the file we contain */
	daddr_t		fsbs;		/* number of blocks stored */
	struct timeval32 time;		/* time stamp for the struct */
	daddr32_t 	dblk[1];	/* place holder for block info */
};

static struct dump *dump_info = NULL;

/*
 * Previously there was no special action required for ordinary files.
 * (Devices are handled through the device file system.)
 * Now we support Large Files and Large File API requires open to
 * fail if file is large.
 * We could take care to prevent data corruption
 * by doing an atomic check of size and truncate if file is opened with
 * FTRUNC flag set but traditionally this is being done by the vfs/vnode
 * layers. So taking care of truncation here is a change in the existing
 * semantics of VOP_OPEN and therefore we chose not to implement any thing
 * here. The check for the size of the file > 2GB is being done at the
 * vfs layer in routine vn_open().
 */

/* ARGSUSED */
static int
ufs_open(vpp, flag, cr)
	struct vnode **vpp;
	int flag;
	struct cred *cr;
{
	TRACE_1(TR_FAC_UFS, TR_UFS_OPEN, "ufs_open:vpp %p", *vpp);
	return (0);
}

/*ARGSUSED*/
static int
ufs_close(vp, flag, count, offset, cr)
	struct vnode *vp;
	int flag;
	int count;
	offset_t offset;
	struct cred *cr;
{
	TRACE_1(TR_FAC_UFS, TR_UFS_CLOSE, "ufs_close:vp %p", vp);

	cleanlocks(vp, ttoproc(curthread)->p_pid, 0);
	cleanshares(vp, ttoproc(curthread)->p_pid);

	/*
	 * Push partially filled cluster at last close.
	 * ``last close'' is approximated because the dnlc
	 * may have a hold on the vnode.
	 */
	if (vp->v_count <= 2 && vp->v_type != VBAD) {
		struct inode *ip = VTOI(vp);
		if (ip->i_delaylen) {
			ins.in_poc.value.ul++;
			(void) ufs_putpages(vp, ip->i_delayoff, ip->i_delaylen,
					B_ASYNC | B_FREE, cr);
			ip->i_delaylen = 0;
		}
	}

	return (0);
}

/*ARGSUSED*/
static int
ufs_read(vp, uiop, ioflag, cr)
	struct vnode *vp;
	struct uio *uiop;
	int ioflag;
	struct cred *cr;
{
	struct inode *ip = VTOI(vp);
	int error;
	struct ufsvfs *ufsvfsp = ip->i_ufsvfs;
	struct ulockfs *ulp;

	ASSERT(RW_READ_HELD(&ip->i_rwlock));
	TRACE_3(TR_FAC_UFS, TR_UFS_READ_START,
		"ufs_read_start:vp %p uiop %p ioflag %x",
		vp, uiop, ioflag);

	/*
	 * Mandatory locking needs to be done before ufs_lockfs_begin()
	 * and TRANS_BEGIN_SYNC() calls since mandatory locks can sleep.
	 */
	if (MANDLOCK(vp, ip->i_mode)) {
		/*
		 * ufs_getattr ends up being called by chklock
		 */
		error = chklock(vp, FREAD,
			uiop->uio_loffset, uiop->uio_resid, uiop->uio_fmode);
		if (error)
			goto out;
	}

	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_READ_MASK);
	if (error)
		goto out;

	/*
	 * Only transact reads to files opened for sync-read and
	 * sync-write on a file system that is not write locked.
	 *
	 * The ``not write locked'' check prevents problems with
	 * enabling/disabling logging on a busy file system.  E.g.,
	 * logging exists at the beginning of the read but does not
	 * at the end.
	 *
	 */
	if (ulp && (ioflag & FRSYNC) && (ioflag & (FSYNC | FDSYNC))) {
		TRANS_BEGIN_SYNC(ufsvfsp, TOP_READ_SYNC, TOP_READ_SIZE);
	}

	rw_enter(&ip->i_contents, RW_READER);
	error = rdip(ip, uiop, ioflag, cr);
	rw_exit(&ip->i_contents);

	if (ulp) {
		if ((ioflag & FRSYNC) && (ioflag & (FSYNC | FDSYNC))) {
			TRANS_END_SYNC(ufsvfsp, error, TOP_READ_SYNC,
					TOP_READ_SIZE);
		}
		ufs_lockfs_end(ulp);
	}
out:
	TRACE_2(TR_FAC_UFS, TR_UFS_READ_END,
		"ufs_read_end:vp %p error %d", vp, error);
	return (error);
}

int	ufs_WRITES = 1;		/* XXX - enable/disable */
int	ufs_HW = 384 * 1024;	/* high water mark */
int	ufs_LW = 256 * 1024;	/* low water mark */
int	ufs_throttles = 0;	/* throttling count */

/*ARGSUSED*/
static int
ufs_write(struct vnode *vp, struct uio *uiop, int ioflag, cred_t *cr)
{
	int error, resv, resid;
	struct inode *ip = VTOI(vp);
	struct ufsvfs *ufsvfsp = ip->i_ufsvfs;
	struct ulockfs *ulp;

	TRACE_3(TR_FAC_UFS, TR_UFS_WRITE_START,
		"ufs_write_start:vp %p uiop %p ioflag %x",
		vp, uiop, ioflag);

	ASSERT(RW_WRITE_HELD(&ip->i_rwlock));

	/*
	 * Mandatory locking needs to be done before ufs_lockfs_begin()
	 * and TRANS_BEGIN_[A]SYNC() calls since mandatory locks can sleep.
	 */
	if (MANDLOCK(vp, ip->i_mode)) {
		/*
		 * ufs_getattr ends up being called by chklock
		 */
		error = chklock(vp, FWRITE,
			uiop->uio_loffset, uiop->uio_resid, uiop->uio_fmode);
		if (error)
			goto out;
	}

	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_WRITE_MASK);
	if (error)
		goto out;

	/*
	 * Amount of log space needed for this write
	 */
	TRANS_WRITE_RESV(ip, uiop, ulp, &resv, &resid);

	/*
	 * Enter Transaction
	 */
	if (ioflag & (FSYNC|FDSYNC)) {
		if (ulp)
			TRANS_BEGIN_SYNC(ufsvfsp, TOP_WRITE_SYNC, resv);
	} else {
		if (ulp)
			TRANS_BEGIN_ASYNC(ufsvfsp, TOP_WRITE, resv);
	}

	/*
	 * Throttle writes.
	 */
	if (ufs_WRITES && (ip->i_writes > ufs_HW)) {
		mutex_enter(&ip->i_tlock);
		while (ip->i_writes > ufs_HW) {
			ufs_throttles++;
			cv_wait(&ip->i_wrcv, &ip->i_tlock);
		}
		mutex_exit(&ip->i_tlock);
	}

	/*
	 * Write the file
	 */
	rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER);
	rw_enter(&ip->i_contents, RW_WRITER);
	if ((ioflag & FAPPEND) != 0 && (ip->i_mode & IFMT) == IFREG) {
		/*
		 * In append mode start at end of file.
		 */
		uiop->uio_loffset = ip->i_size;
	}

	/*
	 * Mild optimisation, dont call ufs_trans_write() unless we have to
	 */
	if (resid) {
		TRANS_WRITE(ip, uiop, ioflag, error, ulp, cr, resv, resid);
	} else {
		error = wrip(ip, uiop, ioflag, cr);
	}
	rw_exit(&ip->i_contents);
	rw_exit(&ufsvfsp->vfs_dqrwlock);

	/*
	 * Leave Transaction
	 */
	if (ulp) {
		if (ioflag & (FSYNC|FDSYNC)) {
			TRANS_END_SYNC(ufsvfsp, error, TOP_WRITE_SYNC, resv);
		} else {
			TRANS_END_ASYNC(ufsvfsp, TOP_WRITE, resv);
		}
		ufs_lockfs_end(ulp);
	}
out:
	TRACE_2(TR_FAC_UFS, TR_UFS_WRITE_END,
		"ufs_write_end:vp %p error %d", vp, error);
	return (error);
}

/*
 * Don't cache write blocks to files with the sticky bit set.
 * Used to keep swap files from blowing the page cache on a server.
 */
int stickyhack = 1;

/*
 * Free behind hacks.  The pager is busted.
 * XXX - need to pass the information down to writedone() in a flag like B_SEQ
 * or B_FREE_IF_TIGHT_ON_MEMORY.
 */
int	freebehind = 1;
int	smallfile = 32 * 1024;

/*
 * wrip does the real work of write requests for ufs.
 */
int
wrip(struct inode *ip, struct uio *uio, int ioflag, struct cred *cr)
{
	u_offset_t off;
	caddr_t base;
	int n, on, mapon;
	struct fs *fs;
	struct vnode *vp;
	struct ufsvfs *ufsvfsp;
	int error, pagecreate;
	o_mode_t type;
	int newpage;
	rlim64_t limit = uio->uio_llimit;
	uint_t flags;
	int iupdat_flag, directio_status;
	long start_resid = uio->uio_resid;	/* save starting resid */
	long premove_resid;			/* resid before uiomove() */

	/*
	 * ip->i_size is incremented before the uiomove
	 * is done on a write.  If the move fails (bad user
	 * address) reset ip->i_size.
	 * The better way would be to increment ip->i_size
	 * only if the uiomove succeeds.
	 */
	int i_size_changed = 0;
	u_offset_t old_i_size;

	vp = ITOV(ip);

	/*
	 * check for forced unmount
	 */
	if (ip->i_ufsvfs == NULL)
		return (EIO);
	ufsvfsp = ip->i_ufsvfs;
	fs = ip->i_fs;

	TRACE_1(TR_FAC_UFS, TR_UFS_RWIP_START,
		"ufs_wrip_start:vp %p", vp);

	ASSERT(RW_LOCK_HELD(&ip->i_contents));

	/* check for valid filetype */
	type = ip->i_mode & IFMT;
	if ((type != IFREG) && (type != IFDIR) &&
	    (type != IFLNK) && (type != IFSHAD)) {
		return (EIO);
	}

	/*
	 * the actual limit of UFS file size
	 * is UFS_MAXOFFSET_T
	 */
	if (limit == RLIM64_INFINITY || limit > MAXOFFSET_T)
		limit = MAXOFFSET_T;

	if (uio->uio_loffset >= limit) {
		TRACE_2(TR_FAC_UFS, TR_UFS_RWIP_END,
			"ufs_wrip_end:vp %p error %d", vp, EINVAL);
		psignal(ttoproc(curthread), SIGXFSZ);
		return (EFBIG);
	}


	/*
	 * if largefiles are disallowed, the limit is
	 * the pre-largefiles value of 2GB
	 */
	if (ufsvfsp->vfs_lfflags & UFS_LARGEFILES)
		limit = MIN(UFS_MAXOFFSET_T, limit);
	else
		limit = MIN(MAXOFF32_T, limit);

	if (uio->uio_loffset < (offset_t)0) {
		TRACE_2(TR_FAC_UFS, TR_UFS_RWIP_END,
			"ufs_wrip_end:vp %p error %d", vp, EINVAL);
		return (EINVAL);
	}
	if (uio->uio_resid == 0) {
		TRACE_2(TR_FAC_UFS, TR_UFS_RWIP_END,
			"ufs_wrip_end:vp %p error %d", vp, 0);
		return (0);
	}

	if (uio->uio_loffset >= limit)
		return (EFBIG);

	ip->i_flag |= INOACC;	/* don't update ref time in getpage */

	if (ioflag & (FSYNC|FDSYNC)) {
		ip->i_flag |= ISYNC;
		iupdat_flag = 1;
	}
	/*
	 * Try to go direct
	 */
	if (ip->i_flag & IDIRECTIO || ufsvfsp->vfs_forcedirectio) {
		uio->uio_llimit = limit;
		error = ufs_directio_write(ip, uio, cr, &directio_status);
		if (directio_status == DIRECTIO_SUCCESS)
			goto out;
	}

	/*
	 * Large Files: We cast MAXBMASK to offset_t
	 * inorder to mask out the higher bits. Since offset_t
	 * is a signed value, the high order bit set in MAXBMASK
	 * value makes it do the right thing by having all bits 1
	 * in the higher word. May be removed for _SOLARIS64_.
	 */

	fs = ip->i_fs;
	do {
		u_offset_t uoff = uio->uio_loffset;
		off = uoff & (offset_t)MAXBMASK;
		mapon = (int)(uoff & (offset_t)MAXBOFFSET);
		on = (int)blkoff(fs, uoff);
		n = (int)MIN(fs->fs_bsize - on, uio->uio_resid);

		if (type == IFREG && uoff + n >= limit) {
			if (uoff >= limit) {
				error = EFBIG;
				goto out;
			}
			/*
			 * since uoff + n >= limit,
			 * therefore n >= limit - uoff, and n is an int
			 * so it is safe to cast it to an int
			 */
			n = (int)(limit - (rlim64_t)uoff);
		}
		if (uoff + n > ip->i_size) {
			/*
			 * We are extending the length of the file.
			 * bmap is used so that we are sure that
			 * if we need to allocate new blocks, that it
			 * is done here before we up the file size.
			 */
			error = bmap_write(ip, uoff, (int)(on + n),
							mapon == 0, cr);
			if (error)
				break;
			i_size_changed = 1;
			old_i_size = ip->i_size;
			UFS_SET_ISIZE(uoff + n, ip);
			TRANS_INODE(ip->i_ufsvfs, ip);
			/*
			 * file has grown larger than 2GB. Set flag
			 * in superblock to indicate this, if it
			 * is not already set.
			 */
			if ((ip->i_size > MAXOFF32_T) &&
			    !(fs->fs_flags & FSLARGEFILES)) {
				ASSERT(ufsvfsp->vfs_lfflags & UFS_LARGEFILES);
				mutex_enter(&ufsvfsp->vfs_lock);
				fs->fs_flags |= FSLARGEFILES;
				ufs_sbwrite(ufsvfsp);
				mutex_exit(&ufsvfsp->vfs_lock);
			}
			/*
			 * If we are writing from the beginning of
			 * the mapping, we can just create the
			 * pages without having to read them.
			 */
			pagecreate = (mapon == 0);
		} else if (n == MAXBSIZE) {
			/*
			 * Going to do a whole mappings worth,
			 * so we can just create the pages w/o
			 * having to read them in.  But before
			 * we do that, we need to make sure any
			 * needed blocks are allocated first.
			 */
			error = bmap_write(ip, uoff, (int)(on + n), 1, cr);
			if (error)
				break;
			pagecreate = 1;
		} else {
			pagecreate = 0;
			/*
			 * In sync mode flush the indirect blocks which
			 * may have been allocated and not written on
			 * disk. In above cases bmap_write will allocate
			 * in sync mode.
			 */
			if (ioflag & (FSYNC|FDSYNC)) {
				error = ufs_indirblk_sync(ip, uoff);
				if (error)
					break;
			}
		}

		/*
		 * At this point we can enter ufs_getpage() in one
		 * of two ways:
		 * 1) segmap_getmapflt() calls ufs_getpage() when the
		 *    forcefault parameter is true (pagecreate == 0)
		 * 2) uiomove() causes a page fault.
		 *
		 * We have to drop the contents lock to prevent the VM
		 * system from trying to reaquire it in ufs_getpage()
		 * should the uiomove cause a pagefault.
		 * Note that this allows a bug to happen where an mmap()'ed
		 * process can see stale data because of the i_size update
		 * above (See bug 4060416).  Unfortunately, holding the lock
		 * has even worse consequences (i.e. deadlock).
		 */
		rw_exit(&ip->i_contents);

		base = segmap_getmapflt(segkmap, vp, (off + mapon),
					(uint_t)n, !pagecreate, S_WRITE);

		/*
		 * segmap_pagecreate() returns 1 if it calls
		 * page_create_va() to allocate any pages.
		 */
		newpage = 0;

		if (pagecreate)
			newpage = segmap_pagecreate(segkmap, base,
			    (size_t)n, 0);

		premove_resid = uio->uio_resid;
		error = uiomove(base + mapon, (long)n, UIO_WRITE, uio);

		if (pagecreate &&
		    uio->uio_loffset < roundup(off + mapon + n, PAGESIZE)) {
			/*
			 * We created pages w/o initializing them completely,
			 * thus we need to zero the part that wasn't set up.
			 * This happens on most EOF write cases and if
			 * we had some sort of error during the uiomove.
			 */
			int nzero, nmoved;

			nmoved = (int)(uio->uio_loffset - (off + mapon));
			ASSERT(nmoved >= 0 && nmoved <= n);
			nzero = roundup(on + n, PAGESIZE) - nmoved;
			ASSERT(nzero > 0 && mapon + nmoved + nzero <= MAXBSIZE);
			(void) kzero(base + mapon + nmoved, (uint_t)nzero);
		}

		/*
		 * Unlock the pages allocated by page_create_va()
		 * in segmap_pagecreate()
		 */
		if (newpage)
			segmap_pageunlock(segkmap, base, (size_t)n, S_WRITE);

		if (error) {
			/*
			 * If we failed on a write, we may have already
			 * allocated file blocks as well as pages.  It's
			 * hard to undo the block allocation, but we must
			 * be sure to invalidate any pages that may have
			 * been allocated.
			 */
			(void) segmap_release(segkmap, base, SM_INVAL);
		} else {
			flags = 0;
			/*
			 * Force write back for synchronous write cases.
			 */
			if ((ioflag & (FSYNC|FDSYNC)) || type == IFDIR) {
				/*
				 * If the sticky bit is set but the
				 * execute bit is not set, we do a
				 * synchronous write back and free
				 * the page when done.  We set up swap
				 * files to be handled this way to
				 * prevent servers from keeping around
				 * the client's swap pages too long.
				 * XXX - there ought to be a better way.
				 */
				if (IS_SWAPVP(vp)) {
					flags = SM_WRITE | SM_FREE |
					    SM_DONTNEED;
					iupdat_flag = 0;
				} else {
					flags = SM_WRITE;
				}
			} else if (n + on == MAXBSIZE || IS_SWAPVP(vp)) {
				/*
				 * Have written a whole block.
				 * Start an asynchronous write and
				 * mark the buffer to indicate that
				 * it won't be needed again soon.
				 */
				flags = SM_WRITE | SM_ASYNC | SM_DONTNEED;
			}
			error = segmap_release(segkmap, base, flags);
			/*
			 * If the operation failed and is synchronous,
			 * then we need to unwind what uiomove() last
			 * did so we can potentially return an error to
			 * the caller.  If this write operation was
			 * done in two pieces and the first succeeded,
			 * then we won't return an error for the second
			 * piece that failed.  However, we only want to
			 * return a resid value that reflects what was
			 * really done.
			 *
			 * Failures for non-synchronous operations can
			 * be ignored since the page subsystem will
			 * retry the operation until it succeeds or the
			 * file system is unmounted.
			 */
			if (error) {
				if ((ioflag & (FSYNC | FDSYNC)) ||
				    type == IFDIR) {
					uio->uio_resid = premove_resid;
				} else {
					error = 0;
				}
			}
		}

		/*
		 * Re-acquire contents lock.
		 */
		rw_enter(&ip->i_contents, RW_WRITER);
		/*
		 * If the uiomove() failed or if a synchronous
		 * page push failed, fix up i_size.
		 */
		if (error) {
			if (i_size_changed) {
				/*
				 * The uiomove failed, and we
				 * allocated blocks,so get rid
				 * of them.
				 */
				(void) ufs_itrunc(ip, old_i_size, 0, cr);
			}
		} else {
			/*
			 * XXX - Can this be out of the loop?
			 */
			ip->i_flag |= IUPD | ICHG;
			if (i_size_changed)
				ip->i_flag |= IATTCHG;
			if (cr->cr_uid != 0 &&
			    (ip->i_mode & (IEXEC | (IEXEC >> 3) |
			    (IEXEC >> 6))) != 0) {
				/*
				 * Clear Set-UID & Set-GID bits on
				 * successful write if not super-user
				 * and at least one of the execute bits
				 * is set.  If we always clear Set-GID,
				 * mandatory file and record locking is
				 * unuseable.
				 */
				ip->i_mode &= ~(ISUID | ISGID);
			}
		}
		TRANS_INODE(ip->i_ufsvfs, ip);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);

out:
	/*
	 * Inode is updated according to this table -
	 *
	 *   FSYNC	  FDSYNC(posix.4)
	 *   --------------------------
	 *   always@	  IATTCHG|IBDWRITE
	 *
	 * @ - 	If we are doing synchronous write the only time we should
	 *	not be sync'ing the ip here is if we have the stickyhack
	 *	activated, the file is marked with the sticky bit and
	 *	no exec bit, the file length has not been changed and
	 *	no new blocks have been allocated during this write.
	 */

	if ((ip->i_flag & ISYNC) != 0) {
		/*
		 * we have eliminated nosync
		 */
		if ((ip->i_flag & (IATTCHG|IBDWRITE)) ||
			((ioflag & FSYNC) && iupdat_flag)) {
			ufs_iupdat(ip, 1);
		}
	}

	/*
	 * If we've already done a partial-write, terminate
	 * the write but return no error.
	 */
	if (start_resid != uio->uio_resid)
		error = 0;

	ip->i_flag &= ~(INOACC | ISYNC);
	ITIMES_NOLOCK(ip);
	TRACE_2(TR_FAC_UFS, TR_UFS_RWIP_END,
		"ufs_wrip_end:vp %p error %d", vp, error);
	return (error);
}

/*
 * rdip does the real work of read requests for ufs.
 */
int
rdip(struct inode *ip, struct uio *uio, int ioflag, cred_t *cr)
{
	u_offset_t off;
	caddr_t base;
	int n, on, mapon;
	struct fs *fs;
	struct ufsvfs *ufsvfsp;
	struct vnode *vp;
	int error;
	uint_t flags;
	o_mode_t type;
	long oresid = uio->uio_resid;
	int dofree, directio_status;
	krw_t rwtype;

	vp = ITOV(ip);

	TRACE_1(TR_FAC_UFS, TR_UFS_RWIP_START,
		"ufs_rdip_start:vp %p", vp);

	ASSERT(RW_LOCK_HELD(&ip->i_contents));

	/* check for valid filetype */
	type = ip->i_mode & IFMT;
	if ((type != IFREG) && (type != IFDIR) &&
	    (type != IFLNK) && (type != IFSHAD)) {
		return (EIO);
	}

	if (uio->uio_loffset > UFS_MAXOFFSET_T) {
		TRACE_2(TR_FAC_UFS, TR_UFS_RWIP_END,
			"ufs_rdip_end:vp %p error %d", vp, EINVAL);
		error = 0;
		goto out;
	}
	if (uio->uio_loffset < (offset_t)0) {
		TRACE_2(TR_FAC_UFS, TR_UFS_RWIP_END,
			"ufs_rdip_end:vp %p error %d", vp, EINVAL);
		return (EINVAL);
	}
	if (uio->uio_resid == 0) {
		TRACE_2(TR_FAC_UFS, TR_UFS_RWIP_END,
			"ufs_rdip_end:vp %p error %d", vp, 0);
		return (0);
	}

	ufsvfsp = ip->i_ufsvfs;
	if (ufsvfsp == NULL)
		return (EIO);
	fs = ufsvfsp->vfs_fs;

	if (!ULOCKFS_IS_NOIACC(ITOUL(ip)) && (fs->fs_ronly == 0) &&
		(!(ip)->i_ufsvfs->vfs_noatime)) {
		mutex_enter(&ip->i_tlock);
		ip->i_flag |= IACC;
		mutex_exit(&ip->i_tlock);
	}
	/*
	 * Try to go direct
	 */
	if (ip->i_flag & IDIRECTIO || ufsvfsp->vfs_forcedirectio) {
		error = ufs_directio_read(ip, uio, cr, &directio_status);
		if (directio_status == DIRECTIO_SUCCESS)
			goto out;
	}

	rwtype = (rw_write_held(&ip->i_contents)?RW_WRITER:RW_READER);
	do {
		offset_t diff;
		u_offset_t uoff = uio->uio_loffset;
		off = uoff & (offset_t)MAXBMASK;
		mapon = (int)(uoff & (offset_t)MAXBOFFSET);
		on = (int)blkoff(fs, uoff);
		n = (int)MIN(fs->fs_bsize - on, uio->uio_resid);

		diff = ip->i_size - uoff;

		if (diff <= (offset_t)0) {
			error = 0;
			goto out;
		}
		if (diff < (offset_t)n)
			n = (int)diff;
		dofree = freebehind &&
		    ip->i_nextr == (off & PAGEMASK) && off > smallfile;

		/*
		 * At this point we can enter ufs_getpage() in one of two
		 * ways:
		 * 1) segmap_getmapflt() calls ufs_getpage() when the
		 *    forcefault parameter is true (value of 1 is passed)
		 * 2) uiomove() causes a page fault.
		 *
		 * We cannot hold onto an i_contents reader lock without
		 * risking deadlock in ufs_getpage() so drop a reader lock.
		 * The ufs_getpage() dolock logic already allows for a
		 * thread holding i_contents as writer to work properly
		 * so we keep a writer lock.
		 */
		if (rwtype == RW_READER)
			rw_exit(&ip->i_contents);
		base = segmap_getmapflt(segkmap, vp, (off + mapon),
					(uint_t)n, 1, S_READ);

		error = uiomove(base + mapon, (long)n, UIO_READ, uio);

		flags = 0;
		if (!error) {
			/*
			 * If reading sequential we won't need
			 * this buffer again soon.
			 */
			if (freebehind && dofree) {
				flags = SM_FREE | SM_DONTNEED |SM_ASYNC;
			}
			/*
			 * In POSIX SYNC (FSYNC and FDSYNC) read mode,
			 * we want to make sure that the page which has
			 * been read, is written on disk if it is dirty.
			 * And corresponding indirect blocks should also
			 * be flushed out.
			 */
			if ((ioflag & FRSYNC) && (ioflag & (FSYNC|FDSYNC))) {
				flags &= ~SM_ASYNC;
				flags |= SM_WRITE;
			}
			error = segmap_release(segkmap, base, flags);
		} else
			(void) segmap_release(segkmap, base, flags);

		if (rwtype == RW_READER)
			rw_enter(&ip->i_contents, rwtype);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);
out:
	/*
	 * Inode is updated according to this table if FRSYNC is set.
	 *
	 *   FSYNC	  FDSYNC(posix.4)
	 *   --------------------------
	 *   always	  IATTCHG|IBDWRITE
	 */

	if (ioflag & FRSYNC) {
		if ((ioflag & FSYNC) ||
		((ioflag & FDSYNC) && (ip->i_flag & (IATTCHG|IBDWRITE)))) {
			rw_exit(&ip->i_contents);
			rw_enter(&ip->i_contents, RW_WRITER);
			ufs_iupdat(ip, 1);
		}
	}
	/*
	 * If we've already done a partial read, terminate
	 * the read but return no error.
	 */
	if (oresid != uio->uio_resid)
		error = 0;
	ITIMES(ip);

	TRACE_2(TR_FAC_UFS, TR_UFS_RWIP_END,
		"ufs_rdip_end:vp %p error %d", vp, error);
	return (error);
}

/* ARGSUSED */
static int
ufs_ioctl(
	struct vnode	*vp,
	int		cmd,
	intptr_t	arg,
	int		flag,
	struct cred	*cr,
	int		*rvalp)
{
	struct lockfs	lockfs, lockfs_out;
	char		*comment, *original_comment;
	int		error;
	struct ufsvfs	*ufsvfsp = VTOI(vp)->i_ufsvfs;
	struct fs	*fs;
	struct ulockfs	*ulp;
	int		issync;
	int		trans_size;

	/*
	 * forcibly unmounted
	 */
	if (ufsvfsp == NULL)
		return (EIO);
	fs = ufsvfsp->vfs_fs;

	if (cmd == Q_QUOTACTL) {
		error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_QUOTA_MASK);
		if (error)
			return (error);

		if (ulp) {
			TRANS_BEGIN_ASYNC(ufsvfsp, TOP_QUOTA,
						TOP_SETQUOTA_SIZE(fs));
		}

		error = quotactl(vp, arg, flag, cr);

		if (ulp) {
			TRANS_END_ASYNC(ufsvfsp, TOP_QUOTA,
						TOP_SETQUOTA_SIZE(fs));
			ufs_lockfs_end(ulp);
		}
		return (error);
	}

	switch (cmd) {
		case _FIOLFS:
			/*
			 * file system locking
			 */
			if (!suser(cr))
				return (EPERM);

			if ((flag & DATAMODEL_MASK) == DATAMODEL_NATIVE) {
				if (copyin((caddr_t)arg, &lockfs,
						sizeof (struct lockfs)))
					return (EFAULT);
			}
#ifdef _SYSCALL32_IMPL
			else {
				struct lockfs32	lockfs32;
				/* Translate ILP32 lockfs to LP64 lockfs */
				if (copyin((caddr_t)arg, &lockfs32,
				    sizeof (struct lockfs32)))
					return (EFAULT);
				lockfs.lf_lock = (ulong_t)lockfs32.lf_lock;
				lockfs.lf_flags = (ulong_t)lockfs32.lf_flags;
				lockfs.lf_key = (ulong_t)lockfs32.lf_key;
				lockfs.lf_comlen = (ulong_t)lockfs32.lf_comlen;
				lockfs.lf_comment =
						(caddr_t)lockfs32.lf_comment;
			}
#endif /* _SYSCALL32_IMPL */

			if (lockfs.lf_comlen) {
				if (lockfs.lf_comlen > LOCKFS_MAXCOMMENTLEN)
					return (ENAMETOOLONG);
				comment = kmem_alloc(lockfs.lf_comlen,
						KM_SLEEP);
				if (copyin(lockfs.lf_comment, comment,
					lockfs.lf_comlen)) {
					kmem_free(comment, lockfs.lf_comlen);
					return (EFAULT);
				}
				original_comment = lockfs.lf_comment;
				lockfs.lf_comment = comment;
			}
			if ((error = ufs_fiolfs(vp, &lockfs, 0)) == 0) {
				lockfs.lf_comment = original_comment;

				if ((flag & DATAMODEL_MASK) ==
				    DATAMODEL_NATIVE) {
					(void) copyout(&lockfs, (caddr_t)arg,
					    sizeof (struct lockfs));
				}
#ifdef _SYSCALL32_IMPL
				else {
					struct lockfs32	lockfs32;
					/* Translate LP64 to ILP32 lockfs */
					lockfs32.lf_lock =
					    (uint32_t)lockfs.lf_lock;
					lockfs32.lf_flags =
					    (uint32_t)lockfs.lf_flags;
					lockfs32.lf_key =
					    (uint32_t)lockfs.lf_key;
					lockfs32.lf_comlen =
					    (uint32_t)lockfs.lf_comlen;
					lockfs32.lf_comment =
					    (uint32_t)lockfs.lf_comment;
					(void) copyout(&lockfs32, (caddr_t)arg,
					    sizeof (struct lockfs32));
				}
#endif /* _SYSCALL32_IMPL */

			}
			return (error);

		case _FIOLFSS:
			/*
			 * get file system locking status
			 */

			if ((flag & DATAMODEL_MASK) == DATAMODEL_NATIVE) {
				if (copyin((caddr_t)arg, &lockfs,
						sizeof (struct lockfs)))
					return (EFAULT);
			}
#ifdef _SYSCALL32_IMPL
			else {
				struct lockfs32	lockfs32;
				/* Translate ILP32 lockfs to LP64 lockfs */
				if (copyin((caddr_t)arg, &lockfs32,
						sizeof (struct lockfs32)))
					return (EFAULT);
				lockfs.lf_lock = (ulong_t)lockfs32.lf_lock;
				lockfs.lf_flags = (ulong_t)lockfs32.lf_flags;
				lockfs.lf_key = (ulong_t)lockfs32.lf_key;
				lockfs.lf_comlen = (ulong_t)lockfs32.lf_comlen;
				lockfs.lf_comment =
					(caddr_t)lockfs32.lf_comment;
			}
#endif /* _SYSCALL32_IMPL */

			if (error =  ufs_fiolfss(vp, &lockfs_out))
				return (error);
			lockfs.lf_lock = lockfs_out.lf_lock;
			lockfs.lf_key = lockfs_out.lf_key;
			lockfs.lf_flags = lockfs_out.lf_flags;
			lockfs.lf_comlen = MIN(lockfs.lf_comlen,
				lockfs_out.lf_comlen);

			if ((flag & DATAMODEL_MASK) == DATAMODEL_NATIVE) {
				if (copyout(&lockfs, (caddr_t)arg,
						sizeof (struct lockfs)))
					return (EFAULT);
			}
#ifdef _SYSCALL32_IMPL
			else {
				/* Translate LP64 to ILP32 lockfs */
				struct lockfs32	lockfs32;
				lockfs32.lf_lock = (uint32_t)lockfs.lf_lock;
				lockfs32.lf_flags = (uint32_t)lockfs.lf_flags;
				lockfs32.lf_key = (uint32_t)lockfs.lf_key;
				lockfs32.lf_comlen = (uint32_t)lockfs.lf_comlen;
				lockfs32.lf_comment =
					(uint32_t)lockfs.lf_comment;
				if (copyout(&lockfs32, (caddr_t)arg,
					    sizeof (struct lockfs32)))
					return (EFAULT);
			}
#endif /* _SYSCALL32_IMPL */

			if (lockfs.lf_comlen &&
			    lockfs.lf_comment && lockfs_out.lf_comment)
				if (copyout(lockfs_out.lf_comment,
					lockfs.lf_comment,
					lockfs.lf_comlen))
					return (EFAULT);
			return (0);

		case _FIOSATIME:
			/*
			 * set access time
			 */

			/*
			 * if mounted w/o atime, return quietly.
			 * I briefly thought about returning ENOSYS, but
			 * figured that most apps would consider this fatal
			 * but the idea is to make this as seamless as poss.
			 */
			if (VTOI(vp)->i_ufsvfs->vfs_noatime)
				return (0);

			error = ufs_lockfs_begin(ufsvfsp, &ulp,
					ULOCKFS_SETATTR_MASK);
			if (error)
				return (error);

			if (ulp) {
				trans_size = (int)TOP_SETATTR_SIZE(VTOI(vp));
				TRANS_BEGIN_CSYNC(ufsvfsp, issync,
						TOP_SETATTR, trans_size);
			}

			error = ufs_fiosatime(vp, (struct timeval *)arg, cr);

			if (ulp) {
				TRANS_END_CSYNC(ufsvfsp, error, issync,
						TOP_SETATTR, trans_size);
				ufs_lockfs_end(ulp);
			}
			return (error);

		case _FIOSDIO:
			/*
			 * set delayed-io
			 */
			return (ufs_fiosdio(vp, (uint_t *)arg, flag, cr));

		case _FIOGDIO:
			/*
			 * get delayed-io
			 */
			return (ufs_fiogdio(vp, (uint_t *)arg, flag, cr));

		case _FIOIO:
			/*
			 * inode open
			 */
			error = ufs_lockfs_begin(ufsvfsp, &ulp,
					ULOCKFS_VGET_MASK);
			if (error)
				return (error);

			error = ufs_fioio(vp, (struct fioio *)arg, cr);

			if (ulp) {
				ufs_lockfs_end(ulp);
			}
			return (error);

		case _FIOFFS:
			/*
			 * file system flush (push w/invalidate)
			 */
			if ((caddr_t)arg != NULL)
				return (EINVAL);
			return (ufs_fioffs(vp, NULL, cr));

		case _FIOISBUSY:
			/*
			 * Contract-private interface for Legato
			 * Purge this vnode from the DNLC and decide
			 * if this vnode is busy (*arg == 1) or not
			 * (*arg == 0)
			 */
			if (!suser(cr))
				return (EPERM);
			error = ufs_fioisbusy(vp, (int *)arg, cr);
			return (error);

		case _FIODIRECTIO:
			return (ufs_fiodirectio(vp, (int)arg, cr));

		case _FIOTUNE:
			/*
			 * Tune the file system (aka setting fs attributes)
			 */
			error = ufs_lockfs_begin(ufsvfsp, &ulp,
					ULOCKFS_SETATTR_MASK);
			if (error)
				return (error);

			error = ufs_fiotune(vp, (struct fiotune *)arg, cr);

			if (ulp)
				ufs_lockfs_end(ulp);
			return (error);

		case _FIOLOGENABLE:
			if (!suser(cr))
				return (EPERM);
			return (ufs_fiologenable(vp, (void *)arg, cr));

		case _FIOLOGDISABLE:
			if (!suser(cr))
				return (EPERM);
			return (ufs_fiologdisable(vp, (void *)arg, cr));

		case _FIOISLOG:
			return (ufs_fioislog(vp, (void *)arg, cr));

		default:
			return (ENOTTY);
	}
}

/* ARGSUSED */
static int
ufs_getattr(vp, vap, flags, cr)
	struct vnode *vp;
	register struct vattr *vap;
	int flags;
	struct cred *cr;
{
	register struct inode *ip = VTOI(vp);
	struct ufsvfs *ufsvfsp;
	int err;

	TRACE_2(TR_FAC_UFS, TR_UFS_GETATTR_START,
		"ufs_getattr_start:vp %p flags %x", vp, flags);

	if (vap->va_mask == AT_SIZE) {
		/*
		 * for performance, if only the size is requested don't bother
		 * with anything else.
		 */
		UFS_GET_ISIZE(&vap->va_size, ip);
		TRACE_1(TR_FAC_UFS, TR_UFS_GETATTR_END,
			"ufs_getattr_end:vp %p", vp);
		return (0);
	}

	/*
	 * inlined lockfs checks
	 */
	ufsvfsp = ip->i_ufsvfs;
	if ((ufsvfsp == NULL) || ULOCKFS_IS_HLOCK(&ufsvfsp->vfs_ulockfs)) {
		err = EIO;
		goto out;
	}

	rw_enter(&ip->i_contents, RW_READER);
	/*
	 * Return all the attributes.  This should be refined so
	 * that it only returns what's asked for.
	 */

	/*
	 * Copy from inode table.
	 */
	vap->va_type = vp->v_type;
	vap->va_mode = ip->i_mode & MODEMASK;
	/*
	 * If there is an ACL and there is a mask entry, then do the
	 * extra work that completes the equivalent of an acltomode(3)
	 * call plus keep the mode bits.  Note that the mask must be
	 * ANDed with the group bits to get the effective group
	 * permissions (see bug 4091822).
	 *
	 * - start with the original permission and mode bits (from above)
	 * - clear the group owner bits
	 * - add in the mask bits intersected with the object group bits
	 */
	if (ip->i_ufs_acl && ip->i_ufs_acl->aclass.acl_ismask) {
		vap->va_mode &= ~((VREAD | VWRITE | VEXEC) >> 3);
		vap->va_mode |= MASK2MODE(ip->i_ufs_acl);
	}
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	vap->va_fsid = ip->i_dev;
	vap->va_nodeid = (ino64_t)ip->i_number;
	vap->va_nlink = ip->i_nlink;
	vap->va_size = ip->i_size;
	vap->va_vcode = ip->i_vcode;
	if (vp->v_type == VCHR || vp->v_type == VBLK)
		vap->va_rdev = ip->i_rdev;
	else
		vap->va_rdev = 0;	/* not a b/c spec. */
	mutex_enter(&ip->i_tlock);
	ITIMES_NOLOCK(ip);	/* mark correct time in inode */
	vap->va_atime.tv_sec = (time_t)ip->i_atime.tv_sec;
	vap->va_atime.tv_nsec = ip->i_atime.tv_usec*1000;
	vap->va_mtime.tv_sec = (time_t)ip->i_mtime.tv_sec;
	vap->va_mtime.tv_nsec = ip->i_mtime.tv_usec*1000;
	vap->va_ctime.tv_sec = (time_t)ip->i_ctime.tv_sec;
	vap->va_ctime.tv_nsec = ip->i_ctime.tv_usec*1000;
	mutex_exit(&ip->i_tlock);

	switch (ip->i_mode & IFMT) {

	case IFBLK:
		vap->va_blksize = MAXBSIZE;		/* was BLKDEV_IOSIZE */
		break;

	case IFCHR:
		vap->va_blksize = MAXBSIZE;
		break;

	default:
		vap->va_blksize = ip->i_fs->fs_bsize;
		break;
	}
	vap->va_nblocks = (fsblkcnt64_t)ip->i_blocks;
	rw_exit(&ip->i_contents);
	err = 0;

out:
	TRACE_1(TR_FAC_UFS, TR_UFS_GETATTR_END, "ufs_getattr_end:vp %p", vp);

	return (err);
}

static int
ufs_setattr(vp, vap, flags, cr)
	register struct vnode *vp;
	register struct vattr *vap;
	int flags;
	struct cred *cr;
{
	int error = 0;
	register long int mask = vap->va_mask;
	register struct inode *ip = VTOI(vp);
	struct ufsvfs *ufsvfsp = ip->i_ufsvfs;
	struct ulockfs *ulp;
	int issync;
	int trans_size;
	int dotrans = 0;
	int dorwlock = 0;
	long blocks;
	int owner_change;
	char *errmsg1 = NULL;
	char *errmsg2 = NULL;
	size_t len1;
	int dodqlock = 0;
	size_t len2;

	TRACE_2(TR_FAC_UFS, TR_UFS_SETATTR_START,
		"ufs_setattr_start:vp %p flags %x", vp, flags);

	/*
	 * Cannot set these attributes.
	 */
	if (mask & AT_NOSET) {
		error = EINVAL;
		goto out;
	}

	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_SETATTR_MASK);
	if (error)
		goto out;

	/*
	 * Acquire i_rwlock before TRANS_BEGIN_CSYNC() if this is a file.
	 * This follows the protocol for read()/write().
	 */
	if (vp->v_type != VDIR) {
		rw_enter(&ip->i_rwlock, RW_WRITER);
		dorwlock = 1;
	}

	/*
	 * Truncate file.  Must have write permission and not be a directory.
	 */
	if (mask & AT_SIZE) {
		rw_enter(&ip->i_contents, RW_WRITER);
		if (vp->v_type == VDIR) {
			error = EISDIR;
			goto update_inode;
		}
		if (error = ufs_iaccess(ip, IWRITE, cr))
			goto update_inode;

		rw_exit(&ip->i_contents);
		error = TRANS_ITRUNC(ip, vap->va_size, 0, cr);
		if (error) {
			rw_enter(&ip->i_contents, RW_WRITER);
			goto update_inode;
		}
	}

	if (ulp) {
		trans_size = (int)TOP_SETATTR_SIZE(ip);
		TRANS_BEGIN_CSYNC(ufsvfsp, issync, TOP_SETATTR, trans_size);
		++dotrans;
	}

	/*
	 * Acquire i_rwlock after TRANS_BEGIN_CSYNC() if this is a directory.
	 * This follows the protocol established by
	 * ufs_link/create/remove/rename/mkdir/rmdir/symlink.
	 */
	if (vp->v_type == VDIR) {
		rw_enter(&ip->i_rwlock, RW_WRITER);
		dorwlock = 1;
	}

	/*
	 * Grab quota lock if we are changing the file's owner.
	 */
	if (mask & AT_UID) {
		rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER);
		dodqlock = 1;
	}
	rw_enter(&ip->i_contents, RW_WRITER);

	/*
	 * Change file access modes.  Must be owner or super-user.
	 */
	if (mask & AT_MODE) {
		if (cr->cr_uid != ip->i_uid && !suser(cr)) {
			error = EPERM;
			goto update_inode;
		}
		ip->i_mode = (ip->i_mode & IFMT) | (vap->va_mode & ~IFMT);
		TRANS_INODE(ip->i_ufsvfs, ip);
		if (cr->cr_uid != 0) {
			/*
			 * A non-privileged user can set the sticky bit
			 * on a directory.
			 */
			if (vp->v_type != VDIR)
				ip->i_mode &= ~ISVTX;
			if (!groupmember((uid_t)ip->i_gid, cr))
				ip->i_mode &= ~ISGID;
		}
		ip->i_flag |= ICHG;
		if (stickyhack) {
			mutex_enter(&vp->v_lock);
			if ((ip->i_mode & (ISVTX | IEXEC | IFDIR)) == ISVTX)
				vp->v_flag |= VSWAPLIKE;
			else
				vp->v_flag &= ~VSWAPLIKE;
			mutex_exit(&vp->v_lock);
		}
	}
	if (mask & (AT_UID|AT_GID)) {
		int checksu = 0;

		/*
		 * To change file ownership, a process not running as
		 * super-user must be running as the owner of the file.
		 */
		if (cr->cr_uid != ip->i_uid)
			checksu = 1;
		else {
			if (rstchown) {
				/*
				 * "chown" is restricted.  A process not
				 * running as super-user cannot change the
				 * owner, and can only change the group to a
				 * group of which it's currently a member.
				 */
				if (((mask & AT_UID) &&
				    vap->va_uid != ip->i_uid) ||
				    ((mask & AT_GID) &&
				    !groupmember(vap->va_gid, cr)))
					checksu = 1;
			}
		}

		if (checksu && !suser(cr)) {
			error = EPERM;
			goto update_inode;
		}

		if (cr->cr_uid != 0) {
			ip->i_mode &= ~(ISUID|ISGID);
		}
		if (mask & AT_UID) {
			/*
			 * Don't change ownership of the quota inode.
			 */
			if (ufsvfsp->vfs_qinod == ip) {
				ASSERT(ufsvfsp->vfs_qflags & MQ_ENABLED);
				error = EINVAL;
				goto update_inode;
			}

			/*
			 * No real ownership change.
			 */
			if (ip->i_uid == vap->va_uid) {
				blocks = 0;
				owner_change = 0;
			}
			/*
			 * Remove the blocks and the file, from the old user's
			 * quota.
			 */
			else {
				blocks = ip->i_blocks;
				owner_change = 1;

				(void) chkdq(ip, -blocks, /* force */ 1, cr,
						(char **)NULL, (size_t *)NULL);
				(void) chkiq(ufsvfsp, /* change */ -1, ip,
						(uid_t)ip->i_uid,
						/* force */ 1, cr,
						(char **)NULL, (size_t *)NULL);
				dqrele(ip->i_dquot);
			}

			ip->i_uid = vap->va_uid;

			/*
			 * There is a real ownership change.
			 */
			if (owner_change) {
				/*
				 * Add the blocks and the file to the new
				 * user's quota.
				 */
				ip->i_dquot = getinoquota(ip);
				(void) chkdq(ip, blocks, /* force */ 1, cr,
						&errmsg1, &len1);
				(void) chkiq(ufsvfsp, /* change */ 1,
						(struct inode *)NULL,
						(uid_t)ip->i_uid,
						/* force */ 1, cr,
						&errmsg2, &len2);
			}
		}
		if (mask & AT_GID) {
			ip->i_gid = vap->va_gid;
		}
		TRANS_INODE(ip->i_ufsvfs, ip);
		ip->i_flag |= ICHG;
	}
	/*
	 * Change file access or modified times.
	 */
	if (mask & (AT_ATIME|AT_MTIME)) {
		if (cr->cr_uid != ip->i_uid && cr->cr_uid != 0) {
			if (flags & ATTR_UTIME)
				error = EPERM;
			else
				error = ufs_iaccess(ip, IWRITE, cr);
			if (error) {
				goto update_inode;
			}
		}

		/* Check that the time value is within ufs range */
		if (((mask & AT_ATIME) && TIMESPEC_OVERFLOW(&vap->va_atime)) ||
		    ((mask & AT_MTIME) && TIMESPEC_OVERFLOW(&vap->va_mtime))) {
			error = EOVERFLOW;
			goto update_inode;
		}

		/*
		 * if the "noaccess" mount option is set and only atime
		 * update is requested, do nothing. No error is returned.
		 */
		if ((VTOI(vp)->i_ufsvfs->vfs_noatime) &&
		    ((mask & (AT_ATIME|AT_MTIME)) == AT_ATIME))
			goto skip_atime;

		mutex_enter(&ip->i_tlock);
		if (mask & AT_ATIME) {
			ip->i_atime.tv_sec = vap->va_atime.tv_sec;
			ip->i_atime.tv_usec = vap->va_atime.tv_nsec / 1000;
			ip->i_flag &= ~IACC;
		}
		if (mask & AT_MTIME) {
			ip->i_mtime.tv_sec = vap->va_mtime.tv_sec;
			ip->i_mtime.tv_usec = vap->va_mtime.tv_nsec / 1000;
			if (hrestime.tv_sec > TIME32_MAX) {
				/*
				 * In 2038, ctime sticks forever..
				 */
				ip->i_ctime.tv_sec = TIME32_MAX;
				ip->i_ctime.tv_usec = 0;
			} else {
				ip->i_ctime.tv_sec = hrestime.tv_sec;
				ip->i_ctime.tv_usec = hrestime.tv_nsec/1000;
			}
			ip->i_flag &= ~(IUPD|ICHG);
			ip->i_flag |= IMODTIME;
		}
		TRANS_INODE(ip->i_ufsvfs, ip);
		ip->i_flag |= IMOD;
		mutex_exit(&ip->i_tlock);
	}

skip_atime:
	/*
	 * The presence of a shadow inode may indicate an ACL, but does
	 * not imply an ACL.  Future FSD types should be handled here too
	 * and check for the presence of the attribute-specific data
	 * before referencing it.
	 */
	if (ip->i_shadow) {
		/*
		 * XXX if ufs_iupdat is changed to sandbagged write fix
		 * ufs_acl_setattr to push ip to keep acls consistent
		 */
		error = ufs_acl_setattr(ip, vap, cr);
	}

update_inode:
	/*
	 * if nfsd and not logging; push synchronously
	 */
	if ((curthread->t_flag & T_DONTPEND) && !TRANS_ISTRANS(ufsvfsp)) {
		ufs_iupdat(ip, 1);
	} else {
		ITIMES_NOLOCK(ip);
	}

	rw_exit(&ip->i_contents);
	if (dodqlock) {
		rw_exit(&ufsvfsp->vfs_dqrwlock);
	}
	if (dorwlock)
		rw_exit(&ip->i_rwlock);

	if (ulp) {
		if (dotrans) {
			TRANS_END_CSYNC(ufsvfsp, error, issync, TOP_SETATTR,
			    trans_size);
		}
		ufs_lockfs_end(ulp);
	}
out:
	TRACE_2(TR_FAC_UFS, TR_UFS_SETATTR_END,
		"ufs_setattr_end:vp %p error %d", vp, error);
	if (errmsg1 != NULL) {
		uprintf(errmsg1);
		kmem_free(errmsg1, len1);
	}
	if (errmsg2 != NULL) {
		uprintf(errmsg2);
		kmem_free(errmsg2, len2);
	}
	return (error);
}

/*ARGSUSED*/
static int
ufs_access(vp, mode, flags, cr)
	struct vnode *vp;
	int mode;
	int flags;
	struct cred *cr;
{
	register struct inode *ip = VTOI(vp);
	int error;

	TRACE_3(TR_FAC_UFS, TR_UFS_ACCESS_START,
		"ufs_access_start:vp %p mode %x flags %x", vp, mode, flags);

	if (ip->i_ufsvfs == NULL)
		return (EIO);

	error = ufs_iaccess(ip, mode, cr);

	TRACE_2(TR_FAC_UFS, TR_UFS_ACCESS_END,
		"ufs_access_end:vp %p error %d", vp, error);
	return (error);
}

/* ARGSUSED */
static int
ufs_readlink(vp, uiop, cr)
	struct vnode *vp;
	struct uio *uiop;
	struct cred *cr;
{
	register struct inode *ip = VTOI(vp);
	register int error;
	struct ufsvfs *ufsvfsp;
	struct ulockfs *ulp;
	int fastsymlink;

	TRACE_2(TR_FAC_UFS, TR_UFS_READLINK_START,
		"ufs_readlink_start:vp %p uiop %p", uiop, vp);

	if (vp->v_type != VLNK) {
		error = EINVAL;
		goto nolockout;
	}

	ufsvfsp = ip->i_ufsvfs;
	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_READLINK_MASK);
	if (error)
		goto nolockout;
	/*
	 * The ip->i_rwlock protects the data blocks used for FASTSYMLINK
	 */
again:
	fastsymlink = 0;
	if (ip->i_flag & IFASTSYMLNK) {
		rw_enter(&ip->i_rwlock, RW_READER);
		rw_enter(&ip->i_contents, RW_READER);
		if (ip->i_flag & IFASTSYMLNK) {
			if (!ULOCKFS_IS_NOIACC(ITOUL(ip)) &&
			    (ip->i_fs->fs_ronly == 0) &&
			    (!(ip)->i_ufsvfs->vfs_noatime)) {
				mutex_enter(&ip->i_tlock);
				ip->i_flag |= IACC;
				mutex_exit(&ip->i_tlock);
			}
			error = uiomove((caddr_t)&ip->i_db[1],
				MIN(ip->i_size, uiop->uio_resid),
				UIO_READ, uiop);
			ITIMES(ip);
			++fastsymlink;
		}
		rw_exit(&ip->i_contents);
		rw_exit(&ip->i_rwlock);
	}
	if (!fastsymlink) {
		ssize_t size;	/* number of bytes read  */
		caddr_t basep;	/* pointer to input data */
		ino_t ino;
		long  igen;

		ino = ip->i_number;
		igen = ip->i_gen;
		size = uiop->uio_resid;
		basep = uiop->uio_iov->iov_base;

		rw_enter(&ip->i_rwlock, RW_WRITER);
		rw_enter(&ip->i_contents, RW_WRITER);
		if (ip->i_flag & IFASTSYMLNK) {
			rw_exit(&ip->i_contents);
			rw_exit(&ip->i_rwlock);
			goto again;
		}
		error = rdip(ip, uiop, 0, cr);
		if (!(error == 0 && ip->i_number == ino && ip->i_gen == igen)) {
			rw_exit(&ip->i_contents);
			rw_exit(&ip->i_rwlock);
			goto out;
		}
		size -= uiop->uio_resid;

		if (ip->i_size <= FSL_SIZE && ip->i_size == size) {
			if (uiop->uio_segflg == UIO_USERSPACE ||
			    uiop->uio_segflg == UIO_USERISPACE)
				error = (copyin(basep, &ip->i_db[1],
				    ip->i_size) != 0) ? EFAULT : 0;
			else
				error = kcopy(basep, &ip->i_db[1], ip->i_size);
			if (error == 0) {
				ip->i_flag |= IFASTSYMLNK;
				/*
				 * free page
				 */
				(void) VOP_PUTPAGE(ITOV(ip),
				    (offset_t)0, PAGESIZE,
				    (B_DONTNEED | B_FREE | B_FORCE | B_ASYNC),
				    cr);
			} else {
				int i;
				/* error, clear garbage left behind */
				for (i = 1; i < NDADDR; i++)
					ip->i_db[i] = 0;
				for (i = 0; i < NIADDR; i++)
					ip->i_ib[i] = 0;
			}
		}
		rw_exit(&ip->i_contents);
		rw_exit(&ip->i_rwlock);
	}
out:
	if (ulp) {
		ufs_lockfs_end(ulp);
	}
nolockout:
	TRACE_2(TR_FAC_UFS, TR_UFS_READLINK_END,
		"ufs_readlink_end:vp %p error %d", vp, error);

	return (error);
}

/* ARGSUSED */
static int
ufs_fsync(vp, syncflag, cr)
	struct vnode *vp;
	int syncflag;
	struct cred *cr;
{
	register struct inode *ip = VTOI(vp);
	int error = 0;
	struct ufsvfs *ufsvfsp = ip->i_ufsvfs;
	struct ulockfs *ulp;

	TRACE_1(TR_FAC_UFS, TR_UFS_FSYNC_START,
		"ufs_fsync_start:vp %p", vp);

	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_FSYNC_MASK);
	if (error)
		goto out;

	if (ulp)
		TRANS_BEGIN_SYNC(ufsvfsp, TOP_FSYNC, TOP_FSYNC_SIZE);

	if (!(IS_SWAPVP(vp)))
		if (syncflag & FNODSYNC) {
			/* Just update the inode only */
			TRANS_IUPDAT(ip, 1);
			error = 0;
		} else if (syncflag & FDSYNC)
			/* Do data-synchronous writes */
			error = TRANS_SYNCIP(ip, 0, I_DSYNC, TOP_FSYNC);
		else
			/* Do synchronous writes */
			error = TRANS_SYNCIP(ip, 0, I_SYNC, TOP_FSYNC);

	rw_enter(&ip->i_contents, RW_WRITER);
	if (!error)
		error = ufs_sync_indir(ip);
	rw_exit(&ip->i_contents);

	if (ulp) {
		TRANS_END_SYNC(ufsvfsp, error, TOP_FSYNC, TOP_FSYNC_SIZE);
		ufs_lockfs_end(ulp);
	}
out:
	TRACE_2(TR_FAC_UFS, TR_UFS_FSYNC_END,
		"ufs_fsync_end:vp %p error %d", vp, error);
	return (error);
}

/*ARGSUSED*/
static void
ufs_inactive(vp, cr)
	struct vnode *vp;
	struct cred *cr;
{

	ufs_iinactive(VTOI(vp));
}

/*
 * Unix file system operations having to do with directory manipulation.
 */
int ufs_lookup_idle_count = 2;	/* Number of inodes to idle each time */
/* ARGSUSED */
static int
ufs_lookup(dvp, nm, vpp, pnp, flags, rdir, cr)
	struct vnode *dvp;
	char *nm;
	struct vnode **vpp;
	struct pathname *pnp;
	int flags;
	struct vnode *rdir;
	struct cred *cr;
{
	register struct inode *ip;
	struct inode *xip;
	register int error;
	struct ufsvfs *ufsvfsp;
	struct ulockfs *ulp;
	struct vnode *vp;

	TRACE_2(TR_FAC_UFS, TR_UFS_LOOKUP_START,
		"ufs_lookup_start:dvp %p name %s", dvp, nm);

	/*
	 * Null component name is a synonym for directory being searched.
	 */
	if (*nm == '\0') {
		VN_HOLD(dvp);
		*vpp = dvp;
		error = 0;
		goto out;
	}

	/*
	 * Fast path: Check the directory name lookup cache.
	 */
	ip = VTOI(dvp);
	if (vp = dnlc_lookup(dvp, nm, NOCRED)) {
		/*
		 * Check accessibility of directory.
		 */
		if ((error = ufs_iaccess(ip, IEXEC, cr)) != 0) {
			VN_RELE(vp);
		}
		ulp = NULL;
		xip = VTOI(vp);
		goto fastpath;
	}

	/*
	 * Keep the idle queue from getting too long by
	 * idling two inodes before attempting to allocate another.
	 *    This operation must be performed before entering
	 *    lockfs or a transaction.
	 */
	if (ufs_idle_q.uq_ne > ufs_idle_q.uq_hiwat)
		if ((curthread->t_flag & T_DONTBLOCK) == 0) {
			ins.in_lidles.value.ul += ufs_lookup_idle_count;
			ufs_idle_some(ufs_lookup_idle_count);
		}

	ufsvfsp = ip->i_ufsvfs;
	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_LOOKUP_MASK);
	if (error)
		goto out;

	error = ufs_dirlook(ip, nm, &xip, cr, 1);

fastpath:
	if (error == 0) {
		ip = xip;
		*vpp = ITOV(ip);
		/*
		 * If vnode is a device return special vnode instead.
		 */
		if (ISVDEV((*vpp)->v_type)) {
			struct vnode *newvp;

			newvp = specvp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type,
			    cr);
			VN_RELE(*vpp);
			if (newvp == NULL)
				error = ENOSYS;
			else
				*vpp = newvp;
		}
	}
	if (ulp) {
		ufs_lockfs_end(ulp);
	}
out:
	TRACE_3(TR_FAC_UFS, TR_UFS_LOOKUP_END,
		"ufs_lookup_end:dvp %p name %s error %d", *vpp, nm, error);
	return (error);
}

static int
ufs_create(dvp, name, vap, excl, mode, vpp, cr, flag)
	struct vnode *dvp;
	char *name;
	struct vattr *vap;
	enum vcexcl excl;
	int mode;
	struct vnode **vpp;
	struct cred *cr;
	int flag;
{
	int error;
	register struct inode *ip = VTOI(dvp);
	struct inode *xip;
	struct vnode *xvp;
	struct ufsvfs *ufsvfsp = ip->i_ufsvfs;
	struct ulockfs *ulp;
	int issync;
	int truncflag = 0;
	int trans_size;

	TRACE_1(TR_FAC_UFS, TR_UFS_CREATE_START,
		"ufs_create_start:dvp %p", dvp);

	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_CREATE_MASK);
	if (error)
		goto out;

	if (ulp) {
		trans_size = (int)TOP_CREATE_SIZE(ip);
		TRANS_BEGIN_CSYNC(ufsvfsp, issync, TOP_CREATE, trans_size);
	}

	/* must be super-user to set sticky bit */
	if (cr->cr_uid != 0)
		vap->va_mode &= ~VSVTX;

	if (*name == '\0') {
		/*
		 * Null component name refers to the directory itself.
		 */
		VN_HOLD(dvp);
		/*
		 * Even though this is an error case, we need to grab the
		 * quota lock since the error handling code below is common.
		 */
		rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER);
		rw_enter(&ip->i_contents, RW_WRITER);
		error = EEXIST;
	} else {
		xip = NULL;
		if (xvp = dnlc_lookup(dvp, name, NOCRED)) {
			if (error = ufs_iaccess(ip, IEXEC, cr)) {
				VN_RELE(xvp);
			} else {
				error = EEXIST;
				xip = VTOI(xvp);
			}
		} else {
			rw_enter(&ip->i_rwlock, RW_WRITER);
			error = ufs_direnter(ip, name, DE_CREATE,
				(struct inode *)0, (struct inode *)0,
				vap, &xip, cr);
			rw_exit(&ip->i_rwlock);
		}
		ip = xip;
		if (ip != NULL) {
			rw_enter(&ip->i_ufsvfs->vfs_dqrwlock, RW_READER);
			rw_enter(&ip->i_contents, RW_WRITER);
		}
	}

	/*
	 * If the file already exists and this is a non-exclusive create,
	 * check permissions and allow access for non-directories.
	 * Read-only create of an existing directory is also allowed.
	 * We fail an exclusive create of anything which already exists.
	 */
	if (error == EEXIST) {
		if (excl == NONEXCL) {
			if (((ip->i_mode & IFMT) == IFDIR) && (mode & IWRITE))
				error = EISDIR;
			else if (mode)
				error = ufs_iaccess(ip, mode, cr);
			else
				error = 0;
		}
		if (error) {
			rw_exit(&ip->i_contents);
			rw_exit(&ufsvfsp->vfs_dqrwlock);
			VN_RELE(ITOV(ip));
			goto unlock;
		} else if (((ip->i_mode & IFMT) == IFREG) &&
		    (vap->va_mask & AT_SIZE) && vap->va_size == 0) {
			/*
			 * Truncate regular files, if requested by caller.
			 * Grab i_rwlock to make sure no one else is
			 * currently writing to the file (we promised
			 * bmap we would do this).
			 * Must get the locks in the correct order.
			 */
			if (ip->i_size == 0) {
				ip->i_flag |= ICHG | IUPD;
				TRANS_INODE(ufsvfsp, ip);
			} else {
				/*
				 * Large Files: Why this check here?
				 * Though we do it in vn_create() we really
				 * want to guarantee that we do not destroy
				 * Large file data by atomically checking
				 * the size while holding the contents
				 * lock.
				 */
				if (flag && !(flag & FOFFMAX) &&
				    ((ip->i_mode & IFMT) == IFREG) &&
				    (ip->i_size > (offset_t)MAXOFF32_T)) {
					rw_exit(&ip->i_contents);
					rw_exit(&ufsvfsp->vfs_dqrwlock);
					error = EOVERFLOW;
					goto unlock;
				}
				if (TRANS_ISTRANS(ufsvfsp))
					truncflag++;
				else {
					rw_exit(&ip->i_contents);
					rw_exit(&ufsvfsp->vfs_dqrwlock);
					rw_enter(&ip->i_rwlock, RW_WRITER);
					rw_enter(&ufsvfsp->vfs_dqrwlock,
							RW_READER);
					rw_enter(&ip->i_contents, RW_WRITER);
					(void) ufs_itrunc(ip, (u_offset_t)0, 0,
								cr);
					rw_exit(&ip->i_rwlock);
				}
			}
		}
	}
	if (error) {
		if (ip != NULL) {
			rw_exit(&ufsvfsp->vfs_dqrwlock);
			rw_exit(&ip->i_contents);
		}
		goto unlock;
	}
	*vpp = ITOV(ip);
	ITIMES(ip);
	rw_exit(&ip->i_contents);
	rw_exit(&ufsvfsp->vfs_dqrwlock);
	/*
	 * If vnode is a device return special vnode instead.
	 */
	if (!error && ISVDEV((*vpp)->v_type)) {
		struct vnode *newvp;

		newvp = specvp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type, cr);
		VN_RELE(*vpp);
		if (newvp == NULL) {
			error = ENOSYS;
			goto unlock;
		}
		truncflag = 0;
		*vpp = newvp;
	}
unlock:

	if (ulp)
		TRANS_END_CSYNC(ufsvfsp, error, issync, TOP_CREATE,
		    trans_size);

	if (!error && truncflag) {
		rw_enter(&ip->i_rwlock, RW_WRITER);
		(void) TRANS_ITRUNC(ip, (u_offset_t)0, 0, cr);
		rw_exit(&ip->i_rwlock);
	}

	if (ulp)
		ufs_lockfs_end(ulp);

out:
	TRACE_3(TR_FAC_UFS, TR_UFS_CREATE_END,
		"ufs_create_end:dvp %p name %s error %d", *vpp, name, error);
	return (error);
}

extern int ufs_idle_max;
/*ARGSUSED*/
static int
ufs_remove(vp, nm, cr)
	struct vnode *vp;
	char *nm;
	struct cred *cr;
{
	register struct inode *ip = VTOI(vp);
	int error;
	struct ufsvfs *ufsvfsp	= ip->i_ufsvfs;
	struct ulockfs *ulp;
	int issync;
	int trans_size;

	TRACE_1(TR_FAC_UFS, TR_UFS_REMOVE_START,
		"ufs_remove_start:vp %p", vp);

	/*
	 * don't let the delete queue get too long
	 */
	if (ufsvfsp->vfs_delete.uq_ne > ufs_idle_max)
		ufs_delete_drain(vp->v_vfsp, 1, 1);

	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_REMOVE_MASK);
	if (error)
		goto out;

	if (ulp)
		TRANS_BEGIN_CSYNC(ufsvfsp, issync, TOP_REMOVE,
		    trans_size = (int)TOP_REMOVE_SIZE(VTOI(vp)));

	rw_enter(&ip->i_rwlock, RW_WRITER);
	error = ufs_dirremove(ip, nm, (struct inode *)0, (struct vnode *)0,
	    DR_REMOVE, cr);
	rw_exit(&ip->i_rwlock);

	if (ulp) {
		TRANS_END_CSYNC(ufsvfsp, error, issync, TOP_REMOVE, trans_size);
		ufs_lockfs_end(ulp);
	}
out:
	TRACE_3(TR_FAC_UFS, TR_UFS_REMOVE_END,
		"ufs_remove_end:vp %p name %s error %d", vp, nm, error);
	return (error);
}

/*
 * Link a file or a directory.  Only the super-user is allowed to make a
 * link to a directory.
 */
static int
ufs_link(tdvp, svp, tnm, cr)
	register struct vnode *tdvp;
	struct vnode *svp;
	char *tnm;
	struct cred *cr;
{
	register struct inode *sip;
	register struct inode *tdp = VTOI(tdvp);
	int error;
	struct vnode *realvp;
	struct ufsvfs *ufsvfsp = tdp->i_ufsvfs;
	struct ulockfs *ulp;
	int issync;
	int trans_size;

	TRACE_1(TR_FAC_UFS, TR_UFS_LINK_START,
		"ufs_link_start:tdvp %p", tdvp);

	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_LINK_MASK);
	if (error)
		goto out;

	if (ulp)
		TRANS_BEGIN_CSYNC(ufsvfsp, issync, TOP_LINK,
		    trans_size = (int)TOP_LINK_SIZE(VTOI(tdvp)));

	if (VOP_REALVP(svp, &realvp) == 0)
		svp = realvp;
	if (svp->v_type == VDIR && !suser(cr)) {
		error = EPERM;
		goto unlock;
	}
	sip = VTOI(svp);
	rw_enter(&tdp->i_rwlock, RW_WRITER);
	error = ufs_direnter(tdp, tnm, DE_LINK, (struct inode *)0,
	    sip, (struct vattr *)0, (struct inode **)0, cr);
	rw_exit(&tdp->i_rwlock);

unlock:
	if (ulp) {
		TRANS_END_CSYNC(ufsvfsp, error, issync, TOP_LINK, trans_size);
		ufs_lockfs_end(ulp);
	}
out:
	TRACE_2(TR_FAC_UFS, TR_UFS_LINK_END,
		"ufs_link_end:tdvp %p error %d", tdvp, error);
	return (error);
}

/*
 * Rename a file or directory.
 * We are given the vnode and entry string of the source and the
 * vnode and entry string of the place we want to move the source
 * to (the target). The essential operation is:
 *	unlink(target);
 *	link(source, target);
 *	unlink(source);
 * but "atomically".  Can't do full commit without saving state in
 * the inode on disk, which isn't feasible at this time.  Best we
 * can do is always guarantee that the TARGET exists.
 */
/*ARGSUSED*/
static int
ufs_rename(sdvp, snm, tdvp, tnm, cr)
	struct vnode *sdvp;		/* old (source) parent vnode */
	char *snm;			/* old (source) entry name */
	struct vnode *tdvp;		/* new (target) parent vnode */
	char *tnm;			/* new (target) entry name */
	struct cred *cr;
{
	struct inode *sip;		/* source inode */
	register struct inode *sdp;	/* old (source) parent inode */
	register struct inode *tdp;	/* new (target) parent inode */
	int error;
	struct vnode *realvp;
	struct ufsvfs *ufsvfsp;
	struct ulockfs *ulp;
	int issync;
	int trans_size;

	TRACE_1(TR_FAC_UFS, TR_UFS_RENAME_START,
		"ufs_rename_start:sdvp %p", sdvp);

	sdp = VTOI(sdvp);
	ufsvfsp = sdp->i_ufsvfs;
	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_RENAME_MASK);
	if (error)
		goto out;

	if (ulp)
		TRANS_BEGIN_CSYNC(ufsvfsp, issync, TOP_RENAME,
		    trans_size = (int)TOP_RENAME_SIZE(sdp));

	if (VOP_REALVP(tdvp, &realvp) == 0)
		tdvp = realvp;

	tdp = VTOI(tdvp);

	mutex_enter(&ufsvfsp->vfs_rename_lock);
	/*
	 * Look up inode of file we're supposed to rename.
	 */
	if (error = ufs_dirlook(sdp, snm, &sip, cr, 0)) {
		mutex_exit(&ufsvfsp->vfs_rename_lock);
		goto unlock;
	}
	/*
	 * Make sure we can delete the source entry.  This requires
	 * write permission on the containing directory.
	 * Check for sticky directories.
	 */
	rw_enter(&sdp->i_contents, RW_READER);
	rw_enter(&sip->i_contents, RW_READER);
	if ((error = ufs_iaccess(sdp, IWRITE, cr)) != 0 ||
	    (error = ufs_sticky_remove_access(sdp, sip, cr)) != 0) {
		rw_exit(&sip->i_contents);
		rw_exit(&sdp->i_contents);
		VN_RELE(ITOV(sip));
		mutex_exit(&ufsvfsp->vfs_rename_lock);
		goto unlock;
	}

	/*
	 * Check for renaming '.' or '..' or alias of '.'
	 */
	if (strcmp(snm, ".") == 0 || strcmp(snm, "..") == 0 || sdp == sip) {
		error = EINVAL;
		rw_exit(&sip->i_contents);
		rw_exit(&sdp->i_contents);
		goto errout;
	}
	rw_exit(&sip->i_contents);
	rw_exit(&sdp->i_contents);

	/*
	 * Link source to the target.
	 */
	rw_enter(&tdp->i_rwlock, RW_WRITER);
	if (error = ufs_direnter(tdp, tnm, DE_RENAME, sdp, sip,
	    (struct vattr *)0, (struct inode **)0, cr)) {
		/*
		 * ESAME isn't really an error; it indicates that the
		 * operation should not be done because the source and target
		 * are the same file, but that no error should be reported.
		 */
		if (error == ESAME)
			error = 0;
		rw_exit(&tdp->i_rwlock);
		goto errout;
	}
	rw_exit(&tdp->i_rwlock);

	rw_enter(&sdp->i_rwlock, RW_WRITER);
	/*
	 * Unlink the source.
	 * Remove the source entry.  ufs_dirremove() checks that the entry
	 * still reflects sip, and returns an error if it doesn't.
	 * If the entry has changed just forget about it.  Release
	 * the source inode.
	 */
	if ((error = ufs_dirremove(sdp, snm, sip, (struct vnode *)0,
	    DR_RENAME, cr)) == ENOENT)
		error = 0;
	rw_exit(&sdp->i_rwlock);

errout:
	VN_RELE(ITOV(sip));
	mutex_exit(&ufsvfsp->vfs_rename_lock);

unlock:
	if (ulp) {
		TRANS_END_CSYNC(ufsvfsp, error, issync, TOP_RENAME, trans_size);
		ufs_lockfs_end(ulp);
	}
out:
	TRACE_5(TR_FAC_UFS, TR_UFS_RENAME_END,
		"ufs_rename_end:sdvp %p snm %s tdvp %p tnm %s error %d",
			sdvp, snm, tdvp, tnm, error);
	return (error);
}

/*ARGSUSED*/
static int
ufs_mkdir(dvp, dirname, vap, vpp, cr)
	struct vnode *dvp;
	char *dirname;
	register struct vattr *vap;
	struct vnode **vpp;
	struct cred *cr;
{
	register struct inode *ip;
	struct inode *xip;
	int error;
	struct ufsvfs *ufsvfsp;
	struct ulockfs *ulp;
	int issync;
	int trans_size;

	ASSERT((vap->va_mask & (AT_TYPE|AT_MODE)) == (AT_TYPE|AT_MODE));

	TRACE_1(TR_FAC_UFS, TR_UFS_MKDIR_START,
		"ufs_mkdir_start:dvp %p", dvp);

	ip = VTOI(dvp);
	ufsvfsp = ip->i_ufsvfs;
	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_MKDIR_MASK);
	if (error)
		goto out;
	if (ulp)
		TRANS_BEGIN_CSYNC(ufsvfsp, issync, TOP_MKDIR,
		    trans_size = (int)TOP_MKDIR_SIZE(ip));

	rw_enter(&ip->i_rwlock, RW_WRITER);
	error = ufs_direnter(ip, dirname, DE_MKDIR, (struct inode *)0,
	    (struct inode *)0, vap, &xip, cr);
	rw_exit(&ip->i_rwlock);
	if (error == 0) {
		ip = xip;
		*vpp = ITOV(ip);
	} else if (error == EEXIST)
		VN_RELE(ITOV(xip));

	if (ulp) {
		TRANS_END_CSYNC(ufsvfsp, error, issync, TOP_MKDIR, trans_size);
		ufs_lockfs_end(ulp);
	}
out:
	TRACE_2(TR_FAC_UFS, TR_UFS_MKDIR_END,
		"ufs_mkdir_end:dvp %p error %d", dvp, error);
	return (error);
}

/*ARGSUSED*/
static int
ufs_rmdir(vp, nm, cdir, cr)
	struct vnode *vp;
	char *nm;
	struct vnode *cdir;
	struct cred *cr;
{
	register struct inode *ip = VTOI(vp);
	int error;
	struct ufsvfs *ufsvfsp = ip->i_ufsvfs;
	struct ulockfs *ulp;
	int issync;

	TRACE_1(TR_FAC_UFS, TR_UFS_RMDIR_START,
		"ufs_rmdir_start:vp %p", vp);

	/*
	 * don't let the delete queue get too long
	 */
	if (ufsvfsp->vfs_delete.uq_ne > ufs_idle_max)
		ufs_delete_drain(vp->v_vfsp, 1, 1);

	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_RMDIR_MASK);
	if (error)
		goto out;

	if (ulp)
		TRANS_BEGIN_CSYNC(ufsvfsp, issync, TOP_RMDIR, TOP_RMDIR_SIZE);

	rw_enter(&ip->i_rwlock, RW_WRITER);
	error = ufs_dirremove(ip, nm, (struct inode *)0, cdir, DR_RMDIR, cr);
	rw_exit(&ip->i_rwlock);

	if (ulp) {
		TRANS_END_CSYNC(ufsvfsp, error, issync, TOP_RMDIR,
				TOP_RMDIR_SIZE);
		ufs_lockfs_end(ulp);
	}
out:
	TRACE_2(TR_FAC_UFS, TR_UFS_RMDIR_END,
		"ufs_rmdir_end:vp %p error %d", vp, error);

	return (error);
}

/* ARGSUSED */
static int
ufs_readdir(vp, uiop, cr, eofp)
	struct vnode *vp;
	struct uio *uiop;
	struct cred *cr;
	int *eofp;
{
	register struct iovec *iovp;
	register struct inode *ip;
	register struct direct *idp;
	register struct dirent64 *odp;
	register uint_t offset;
	register int incount = 0;
	register int outcount = 0;
	register uint_t bytes_wanted, total_bytes_wanted;
	caddr_t outbuf;
	size_t bufsize;
	int error;
	struct fbuf *fbp;
	struct ufsvfs *ufsvfsp;
	struct ulockfs *ulp;

	ip = VTOI(vp);
	ASSERT(RW_READ_HELD(&ip->i_rwlock));

	TRACE_2(TR_FAC_UFS, TR_UFS_READDIR_START,
		"ufs_readdir_start:vp %p uiop %p", vp, uiop);

	if (uiop->uio_loffset >= MAXOFF32_T) {
		if (eofp)
			*eofp = 1;
		return (0);
	}

	/*
	 * Large Files: When we come here we are guaranteed that
	 * uio_offset can be used safely. The high word is zero.
	 */

	ufsvfsp = ip->i_ufsvfs;
	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_READDIR_MASK);
	if (error)
		goto out;

	iovp = uiop->uio_iov;
	total_bytes_wanted = iovp->iov_len;

	/* Large Files: directory files should not be "large" */

	ASSERT(ip->i_size <= MAXOFF32_T);

	/* Force offset to be valid (to guard against bogus lseek() values) */
	offset = (uint_t)uiop->uio_offset & ~(DIRBLKSIZ - 1);

	/* Quit if at end of file or link count of zero (posix) */
	if (offset >= (uint_t)ip->i_size || ip->i_nlink <= 0) {
		if (eofp)
			*eofp = 1;
		error = 0;
		goto unlock;
	}

	/*
	 * Get space to change directory entries into fs independent format.
	 * Do fast alloc for the most commonly used-request size (filesystem
	 * block size).
	 */
	if (uiop->uio_segflg != UIO_SYSSPACE || uiop->uio_iovcnt != 1) {
		/*
		 * The user level readdir() routines actually make
		 * getdent requests in units of 1048 instead of
		 * MAXBSIZE.  This seems a performance loss...
		 */
		bufsize = total_bytes_wanted;
		outbuf = kmem_alloc(bufsize, KM_SLEEP);
		odp = (struct dirent64 *)outbuf;
	} else {
		bufsize = total_bytes_wanted;
		odp = (struct dirent64 *)iovp->iov_base;
	}

nextblk:
	bytes_wanted = total_bytes_wanted;

	/* Truncate request to file size */
	if (offset + bytes_wanted > (int)ip->i_size)
		bytes_wanted = (int)(ip->i_size - offset);

	/* Comply with MAXBSIZE boundary restrictions of fbread() */
	if ((offset & MAXBOFFSET) + bytes_wanted > MAXBSIZE)
		bytes_wanted = MAXBSIZE - (offset & MAXBOFFSET);

	/*
	 * Read in the next chunk.
	 * We are still holding the i_rwlock.
	 */
	error = fbread(vp, (offset_t)offset, bytes_wanted, S_OTHER, &fbp);

	if (error)
		goto update_inode;
	if (!ULOCKFS_IS_NOIACC(ITOUL(ip)) && (ip->i_fs->fs_ronly == 0) &&
	    (!(ip)->i_ufsvfs->vfs_noatime)) {
		ip->i_flag |= IACC;
	}
	incount = 0;
	idp = (struct direct *)fbp->fb_addr;
	if (idp->d_ino == 0 && idp->d_reclen == 0 &&
		idp->d_namlen == 0) {
		cmn_err(CE_WARN, "ufs_readir: bad dir, inumber = %d, fs = %s\n",
			(int)ip->i_number, ufsvfsp->vfs_fs->fs_fsmnt);
		fbrelse(fbp, S_OTHER);
		error = ENXIO;
		goto update_inode;
	}
	/* Transform to file-system independent format */
	while (incount < bytes_wanted) {
		/*
		 * If the current directory entry is mangled, then skip
		 * to the next block.  It would be nice to set the FSBAD
		 * flag in the super-block so that a fsck is forced on
		 * next reboot, but locking is a problem.
		 */
		if (idp->d_reclen & 0x3) {
			offset = (offset + DIRBLKSIZ) & ~(DIRBLKSIZ-1);
			break;
		}

		/* Skip to requested offset and skip empty entries */
		if (idp->d_ino != 0 && offset >= (uint_t)uiop->uio_offset) {
			ushort_t this_reclen =
			    DIRENT64_RECLEN(idp->d_namlen);
			/* Buffer too small for any entries */
			if (!outcount && this_reclen > bufsize) {
				fbrelse(fbp, S_OTHER);
				error = EINVAL;
				goto update_inode;
			}
			/* If would overrun the buffer, quit */
			if (outcount + this_reclen > bufsize) {
				break;
			}
			/* Take this entry */
			odp->d_ino = (ino64_t)idp->d_ino;
			odp->d_reclen = (ushort_t)this_reclen;
			odp->d_off = (offset_t)(offset + idp->d_reclen);
			(void) strcpy(odp->d_name, idp->d_name);
			outcount += odp->d_reclen;
			odp = (struct dirent64 *)((intptr_t)odp +
				    odp->d_reclen);
			ASSERT(outcount <= bufsize);
		}
		if (idp->d_reclen) {
			incount += idp->d_reclen;
			offset += idp->d_reclen;
			idp = (struct direct *)((intptr_t)idp + idp->d_reclen);
		} else {
			offset = (offset + DIRBLKSIZ) & ~(DIRBLKSIZ-1);
			break;
		}
	}
	/* Release the chunk */
	fbrelse(fbp, S_OTHER);

	/* Read whole block, but got no entries, read another if not eof */

	/*
	 * Large Files: casting i_size to int here is not a problem
	 * because directory sizes are always less than MAXOFF32_T.
	 * See assertion above.
	 */

	if (offset < (int)ip->i_size && !outcount)
		goto nextblk;

	/* Copy out the entry data */
	if (uiop->uio_segflg == UIO_SYSSPACE && uiop->uio_iovcnt == 1) {
		iovp->iov_base += outcount;
		iovp->iov_len -= outcount;
		uiop->uio_resid -= outcount;
		uiop->uio_offset = offset;
	} else if ((error = uiomove(outbuf, (long)outcount, UIO_READ,
				    uiop)) == 0)
		uiop->uio_offset = offset;
update_inode:
	ITIMES(ip);
	if (uiop->uio_segflg != UIO_SYSSPACE || uiop->uio_iovcnt != 1)
		kmem_free(outbuf, bufsize);

	if (eofp && error == 0)
		*eofp = (uiop->uio_offset >= (int)ip->i_size);
unlock:
	if (ulp) {
		ufs_lockfs_end(ulp);
	}
out:
	TRACE_2(TR_FAC_UFS, TR_UFS_READDIR_END,
		"ufs_readdir_end:vp %p error %d", vp, error);
	return (error);
}

/*ARGSUSED*/
static int
ufs_symlink(dvp, linkname, vap, target, cr)
	register struct vnode *dvp;	/* ptr to parent dir vnode */
	char *linkname;			/* name of symbolic link */
	struct vattr *vap;		/* attributes */
	char *target;			/* target path */
	struct cred *cr;		/* user credentials */
{
	struct inode *ip, *dip = VTOI(dvp);
	int error;
	struct ufsvfs *ufsvfsp = dip->i_ufsvfs;
	struct ulockfs *ulp;
	int issync;
	int trans_size;
	int leftover = 0;	/* needed to prevent EIO from ufs_rdrwi() */
	int ioflag;

	ip = (struct inode *)0;
	vap->va_type = VLNK;
	vap->va_rdev = 0;

	TRACE_1(TR_FAC_UFS, TR_UFS_SYMLINK_START,
		"ufs_symlink_start:dvp %p", dvp);

	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_SYMLINK_MASK);
	if (error)
		goto out;

	if (ulp)
		TRANS_BEGIN_CSYNC(ufsvfsp, issync, TOP_SYMLINK,
		    trans_size = (int)TOP_SYMLINK_SIZE(dip));

	rw_enter(&dip->i_rwlock, RW_WRITER);
	error = ufs_direnter(dip, linkname, DE_CREATE,
	    (struct inode *)0, (struct inode *)0, vap, &ip, cr);
	rw_exit(&dip->i_rwlock);
	if (error == 0) {
		/*
		 * If we are syncdir or called from nfs
		 * (curthread->t_flag & T_DONTPEND) then synchronously
		 * write out the symlink (ie file data).
		 */
		ioflag = FWRITE;
		if ((ufsvfsp->vfs_syncdir) ||
		    (curthread->t_flag & T_DONTPEND)) {
			ioflag |= FDSYNC;
		}

		rw_enter(&ip->i_rwlock, RW_WRITER);
		rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER);
		rw_enter(&ip->i_contents, RW_WRITER);
		/*
		 * We need to pass in a pointer to "leftover" for "aresid"
		 * otherwise ufs_rdwri() will always return EIO if it can't
		 * write the buffer ("target"). Even if the problem was ENOSPC
		 * or EDQUOT.
		 */
		error = ufs_rdwri(UIO_WRITE, ioflag, ip, target, strlen(target),
		    (offset_t)0, UIO_SYSSPACE, &leftover, cr);
		if (error) {
			rw_exit(&ip->i_contents);
			rw_exit(&ufsvfsp->vfs_dqrwlock);
			rw_exit(&ip->i_rwlock);
			VN_RELE(ITOV(ip));
			rw_enter(&dip->i_rwlock, RW_WRITER);
			/*
			 * We've already got one error set, let's not bother
			 * with another since any possible problems should
			 * have been caught in ufs_direnter()
			 */
			(void) ufs_dirremove(dip, linkname, (struct inode *)0,
			    (struct vnode *)0, DR_REMOVE, cr);
			rw_exit(&dip->i_rwlock);
			goto unlock;
		}
		if (ip->i_size <= FSL_SIZE) {
			/* create a fast symbolic link */
			if (kcopy(target, &ip->i_db[1], ip->i_size) == 0) {
				ip->i_flag |= IFASTSYMLNK;
			} else {
				int i;
				/* error, clear garbage left behind */
				for (i = 1; i < NDADDR; i++)
					ip->i_db[i] = 0;
				for (i = 0; i < NIADDR; i++)
					ip->i_ib[i] = 0;
			}
		/*
		 * nice to free the page to the front of the free inode queue,
		 * but don't bother because symbolic links are seldom created
		 * and leaving at the end of the queue is inexpensive.
		 */
		}
		rw_exit(&ip->i_contents);
		rw_exit(&ufsvfsp->vfs_dqrwlock);
		rw_exit(&ip->i_rwlock);
	}

	if (error == 0 || error == EEXIST)
		VN_RELE(ITOV(ip));
unlock:
	if (ulp) {
		TRANS_END_CSYNC(ufsvfsp, error, issync, TOP_SYMLINK,
				trans_size);
		ufs_lockfs_end(ulp);
	}
out:
	TRACE_2(TR_FAC_UFS, TR_UFS_SYMLINK_END,
		"ufs_symlink_end:dvp %p error %d", dvp, error);
	return (error);
}

/*
 * Ufs specific routine used to do ufs io.
 */
int
ufs_rdwri(rw, ioflag, ip, base, len, offset, seg, aresid, cr)
	enum uio_rw rw;
	int ioflag;
	struct inode *ip;
	caddr_t base;
	ssize_t len;
	offset_t offset;
	enum uio_seg seg;
	int *aresid;
	struct cred *cr;
{
	struct uio auio;
	struct iovec aiov;
	register int error;

	ASSERT(RW_LOCK_HELD(&ip->i_contents));

	bzero((caddr_t)&auio, sizeof (uio_t));
	bzero((caddr_t)&aiov, sizeof (iovec_t));

	aiov.iov_base = base;
	aiov.iov_len = len;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_loffset = offset;
	auio.uio_segflg = (short)seg;
	auio.uio_resid = len;
	auio.uio_llimit = P_CURLIMIT(curproc, RLIMIT_FSIZE);

	if (rw == UIO_WRITE) {
		auio.uio_fmode = FWRITE;
		error = wrip(ip, &auio, ioflag, cr);
	} else {
		auio.uio_fmode = FREAD;
		error = rdip(ip, &auio, ioflag, cr);
	}

	if (aresid) {
		*aresid = auio.uio_resid;
	} else if (auio.uio_resid) {
		error = EIO;
	}
	return (error);
}

static int
ufs_fid(vp, fidp)
	struct vnode *vp;
	struct fid *fidp;
{
	register struct ufid *ufid;
	register struct inode *ip = VTOI(vp);

	if (ip->i_ufsvfs == NULL)
		return (EIO);

	if (fidp->fid_len < (sizeof (struct ufid) - sizeof (ushort_t))) {
		fidp->fid_len = sizeof (struct ufid) - sizeof (ushort_t);
		return (ENOSPC);
	}

	ufid = (struct ufid *)fidp;
	bzero((char *)ufid, sizeof (struct ufid));
	ufid->ufid_len = sizeof (struct ufid) - sizeof (ushort_t);
	ufid->ufid_ino = ip->i_number;
	ufid->ufid_gen = ip->i_gen;

	return (0);
}

static void
ufs_rwlock(vp, write_lock)
	struct vnode *vp;
	int write_lock;
{
	struct inode	*ip = VTOI(vp);

	if (write_lock)
		rw_enter(&ip->i_rwlock, RW_WRITER);
	else
		rw_enter(&ip->i_rwlock, RW_READER);
}

/*ARGSUSED*/
static void
ufs_rwunlock(vp, write_lock)
	struct vnode *vp;
	int write_lock;
{
	struct inode	*ip = VTOI(vp);

	rw_exit(&ip->i_rwlock);
}

/* ARGSUSED */
static int
ufs_seek(vp, ooff, noffp)
	struct vnode *vp;
	offset_t ooff;
	offset_t *noffp;
{
	return ((*noffp < 0 || *noffp > MAXOFFSET_T) ? EINVAL : 0);
}

/* ARGSUSED */
static int
ufs_frlock(vp, cmd, bfp, flag, offset, cr)
	register struct vnode *vp;
	int cmd;
	struct flock64 *bfp;
	int flag;
	offset_t offset;
	cred_t *cr;
{
	register struct inode *ip = VTOI(vp);

	if (ip->i_ufsvfs == NULL)
		return (EIO);

	/*
	 * If file is being mapped, disallow frlock.
	 * XXX I am not holding tlock while checking i_mapcnt because the
	 * current locking strategy drops all locks before calling fs_frlock.
	 * So, mapcnt could change before we enter fs_frlock making is
	 * meaningless to have held tlock in the first place.
	 */
	if (ip->i_mapcnt > 0 && MANDLOCK(vp, ip->i_mode))
		return (EAGAIN);
	return (fs_frlock(vp, cmd, bfp, flag, offset, cr));
}

/* ARGSUSED */
static int
ufs_space(vp, cmd, bfp, flag, offset, cr)
	struct vnode *vp;
	int cmd;
	struct flock64 *bfp;
	int flag;
	offset_t offset;
	struct cred *cr;
{
	int error;
	struct ufsvfs *ufsvfsp	= VTOI(vp)->i_ufsvfs;
	struct ulockfs *ulp;

	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_SPACE_MASK);
	if (error)
		return (error);


	if (cmd != F_FREESP)
		error =  EINVAL;
	else if ((error = convoff(vp, bfp, 0, offset)) == 0)
		error = ufs_freesp(vp, bfp, flag, cr);

	if (ulp)
		ufs_lockfs_end(ulp);
	return (error);
}

/*
 * For read purposes, this has to be (bsize * maxcontig).
 * For write purposes, this can be larger.
 *
 * XXX - if you make it larger, change findextent() to match.
 */
#define	RD_CLUSTSZ(ip)		((ip)->i_ufsvfs->vfs_rdclustsz)
#define	WR_CLUSTSZ(ip)		((ip)->i_ufsvfs->vfs_wrclustsz)

/*
 * A faster version of ufs_getpage.
 *
 * We optimize by inlining the pvn_getpages iterator, eliminating
 * calls to bmap_read if file doesn't have UFS holes, and avoiding
 * the overhead of page_exists().
 *
 * When files has UFS_HOLES and ufs_getpage is called with S_READ,
 * we set *protp to PROT_READ to avoid calling bmap_read. This approach
 * victimizes performance when a file with UFS holes is faulted
 * first in the S_READ mode, and then in the S_WRITE mode. We will get
 * two MMU faults in this case.
 *
 * XXX - the inode fields which control the sequential mode are not
 *	 protected by any mutex. The read ahead will act wild if
 *	 multiple processes will access the file concurrently and
 *	 some of them in sequential mode. One particulary bad case
 *	 is if another thread will change the value of i_nextrio between
 *	 the time this thread tests the i_nextrio value and then reads it
 *	 again to use it as the offset for the read ahead.
 */
static int
ufs_getpage(vp, off, len, protp, plarr, plsz, seg, addr, rw, cr)
	struct vnode *vp;
	offset_t off;
	size_t len;
	uint_t *protp;
	page_t *plarr[];
	size_t plsz;
	struct seg *seg;
	caddr_t addr;
	enum seg_rw rw;
	struct cred *cr;
{
	struct inode 	*ip = VTOI(vp);
	struct fs 	*fs;
	struct ufsvfs	*ufsvfsp = ip->i_ufsvfs;
	struct ulockfs	*ulp;
	u_offset_t	uoff = (u_offset_t)off; /* type conversion */
	int 		err;
	page_t		**pl;
	int		has_holes;
	int		beyond_eof;
	int		seqmode;
	u_offset_t	pgoff;
	u_offset_t	eoff;
	caddr_t		pgaddr;
	int		pgsize = PAGESIZE;
	krw_t		rwtype;
	int		dolock;
	int		do_qlock;
	int		trans_size;

	TRACE_1(TR_FAC_UFS, TR_UFS_GETPAGE_START,
		"ufs_getpage_start:vp %p", vp);

	ASSERT((uoff & PAGEOFFSET) == 0);

	if (protp)
		*protp = PROT_ALL;

	/*
	 * Obey the lockfs protocol
	 */
	err = ufs_lockfs_begin_getpage(ufsvfsp, &ulp, seg,
			rw == S_READ || rw == S_EXEC, protp);
	if (err)
		goto out;

	fs = ufsvfsp->vfs_fs;

	if (ulp && (rw == S_CREATE || rw == S_WRITE) &&
		!(vp->v_flag & VISSWAP)) {
		/*
		 * Try to start a transaction, will return if blocking is
		 * expected to occur and the address space is not the
		 * kernel address space.
		 */
		trans_size = TOP_GETPAGE_SIZE(ip);
		if (seg->s_as != &kas) {
			TRANS_TRY_BEGIN_ASYNC(ufsvfsp, TOP_GETPAGE,
				trans_size, err)
			if (err == EWOULDBLOCK) {
				/*
				 * Use EDEADLK here because the VM code
				 * can normally never see this error.
				 */
				err = EDEADLK;
				ufs_lockfs_end(ulp);
				goto out;
			}
		} else {
			TRANS_BEGIN_ASYNC(ufsvfsp, TOP_GETPAGE, trans_size);
		}
	}

	if (vp->v_flag & VNOMAP) {
		err = ENOSYS;
		goto unlock;
	}

	seqmode = ip->i_nextr == uoff && rw != S_CREATE;

	rwtype = RW_READER;		/* start as a reader */
	dolock = (rw_owner(&ip->i_contents) != curthread);
	/*
	 * If this thread owns the lock, i.e., this thread grabbed it
	 * as writer somewhere above, then we don't need to grab the
	 * lock as reader in this routine.
	 */
	do_qlock = (rw_owner(&ufsvfsp->vfs_dqrwlock) != curthread);

retrylock:
	if (dolock) {
		/*
		 * Grab the quota lock if we need to call
		 * bmap_write() below (with i_contents as writer).
		 */
		if (do_qlock && rwtype == RW_WRITER)
			rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER);
		rw_enter(&ip->i_contents, rwtype);
	}

	/*
	 * We may be getting called as a side effect of a bmap using
	 * fbread() when the blocks might be being allocated and the
	 * size has not yet been up'ed.  In this case we want to be
	 * able to return zero pages if we get back UFS_HOLE from
	 * calling bmap for a non write case here.  We also might have
	 * to read some frags from the disk into a page if we are
	 * extending the number of frags for a given lbn in bmap().
	 * Large Files: The read of i_size here is atomic because
	 * i_contents is held here. If dolock is zero, the lock
	 * is held in bmap routines.
	 */
	beyond_eof = uoff + len > ip->i_size + PAGEOFFSET;
	if (beyond_eof && seg != segkmap) {
		if (dolock) {
			rw_exit(&ip->i_contents);
			if (do_qlock && rwtype == RW_WRITER)
				rw_exit(&ufsvfsp->vfs_dqrwlock);
		}
		err = EFAULT;
		goto unlock;
	}

	/*
	 * Must hold i_contents lock throughout the call to pvn_getpages
	 * since locked pages are returned from each call to ufs_getapage.
	 * Must *not* return locked pages and then try for contents lock
	 * due to lock ordering requirements (inode > page)
	 */

	has_holes = bmap_has_holes(ip);

	if ((rw == S_WRITE || rw == S_CREATE) && (has_holes || beyond_eof)) {
		int	blk_size;
		u_offset_t offset;

		/*
		 * We must acquire the RW_WRITER lock in order to
		 * call bmap_write().
		 */
		if (dolock && rwtype == RW_READER) {
			rwtype = RW_WRITER;

			/*
			 * Grab the quota lock before
			 * upgrading i_contents, but if we can't grab it
			 * don't wait here due to lock order:
			 * vfs_dqrwlock > i_contents.
			 */
			if (do_qlock && rw_tryenter(&ufsvfsp->vfs_dqrwlock,
							RW_READER) == 0) {
				rw_exit(&ip->i_contents);
				goto retrylock;
			}
			if (!rw_tryupgrade(&ip->i_contents)) {
				rw_exit(&ip->i_contents);
				if (do_qlock)
					rw_exit(&ufsvfsp->vfs_dqrwlock);
				goto retrylock;
			}
		}

		/*
		 * May be allocating disk blocks for holes here as
		 * a result of mmap faults. write(2) does the bmap_write
		 * in rdip/wrip, not here. We are not dealing with frags
		 * in this case.
		 */
		/*
		 * Large Files: We cast fs_bmask field to offset_t
		 * just as we do for MAXBMASK because uoff is a 64-bit
		 * data type. fs_bmask will still be a 32-bit type
		 * as we cannot change any ondisk data structures.
		 */

		offset = uoff & (offset_t)fs->fs_bmask;
		while (offset < uoff + len) {
			/*
			 * the variable "bnp" is to simplify the expression for
			 * the compiler; * just passing in &bn to bmap_write
			 * causes a compiler "loop"
			 */

			blk_size = (int)blksize(fs, ip, lblkno(fs, offset));
			err = bmap_write(ip, offset, blk_size, 0, cr);
			if (err)
				goto update_inode;
			offset += blk_size; /* XXX - make this contig */
		}
	}

	/*
	 * Can be a reader from now on.
	 */
	if (dolock && rwtype == RW_WRITER) {
		rw_downgrade(&ip->i_contents);
		/*
		 * We can release vfs_dqrwlock early so do it, but make
		 * sure we don't try to release it again at the bottom.
		 */
		if (do_qlock) {
			rw_exit(&ufsvfsp->vfs_dqrwlock);
			do_qlock = 0;
		}
	}

	/*
	 * We remove PROT_WRITE in cases when the file has UFS holes
	 * because we don't  want to call bmap_read() to check each
	 * page if it is backed with a disk block.
	 */
	if (protp && has_holes && rw != S_WRITE && rw != S_CREATE)
		*protp &= ~PROT_WRITE;

	err = 0;

	/*
	 * The loop looks up pages in the range [off, off + len).
	 * For each page, we first check if we should initiate an asynchronous
	 * read ahead before we call page_lookup (we may sleep in page_lookup
	 * for a previously initiated disk read).
	 */
	eoff = (uoff + len);
	for (pgoff = uoff, pgaddr = addr, pl = plarr;
	    pgoff < eoff; /* empty */) {
		page_t	*pp;
		u_offset_t	nextrio;
		se_t	se;

		se = ((rw == S_CREATE || rw == S_OTHER) ? SE_EXCL : SE_SHARED);

		/* Handle async getpage (faultahead) */
		if (plarr == NULL) {
			ip->i_nextrio = pgoff;
			(void) ufs_getpage_ra(vp, pgoff, seg, pgaddr);
			pgoff += pgsize;
			pgaddr += pgsize;
			continue;
		}
		/*
		 * Check if we should initiate read ahead of next cluster.
		 * We call page_exists only when we need to confirm that
		 * we have the current page before we initiate the read ahead.
		 */
		nextrio = ip->i_nextrio;
		if (seqmode &&
		    pgoff + RD_CLUSTSZ(ip) >= nextrio && pgoff <= nextrio &&
		    nextrio < ip->i_size && page_exists(vp, pgoff))
			(void) ufs_getpage_ra(vp, pgoff, seg, pgaddr);

		if ((pp = page_lookup(vp, pgoff, se)) != NULL) {
			/*
			 * We found the page in the page cache.
			 */
			*pl++ = pp;
			pgoff += pgsize;
			pgaddr += pgsize;
			len -= pgsize;
			plsz -= pgsize;
		} else  {
			/*
			 * We have to create the page, or read it from disk.
			 */
			if (err = ufs_getpage_miss(vp, pgoff, len, seg, pgaddr,
			    pl, plsz, rw, seqmode))
				goto error;

			while (*pl != NULL) {
				pl++;
				pgoff += pgsize;
				pgaddr += pgsize;
				len -= pgsize;
				plsz -= pgsize;
			}
		}
	}

	/*
	 * Return pages up to plsz if they are in the page cache.
	 * We cannot return pages if there is a chance that they are
	 * backed with a UFS hole and rw is S_WRITE or S_CREATE.
	 */
	if (plarr && !(has_holes && (rw == S_WRITE || rw == S_CREATE))) {

		ASSERT((protp == NULL) ||
			!(has_holes && (*protp & PROT_WRITE)));

		eoff = pgoff + plsz;
		while (pgoff < eoff) {
			page_t		*pp;

			if ((pp = page_lookup_nowait(vp, pgoff,
			    SE_SHARED)) == NULL)
				break;

			*pl++ = pp;
			pgoff += pgsize;
			plsz -= pgsize;
		}
	}

	if (plarr)
		*pl = NULL;			/* Terminate page list */
	ip->i_nextr = pgoff;

error:
	if (err && plarr) {
		/*
		 * Release any pages we have locked.
		 */
		while (pl > &plarr[0])
			page_unlock(*--pl);

		plarr[0] = NULL;
	}

update_inode:
	/*
	 * If the inode is not already marked for IACC (in rdip() for read)
	 * and the inode is not marked for no access time update (in wrip()
	 * for write) then update the inode access time and mod time now.
	 */
	if ((ip->i_flag & (IACC | INOACC)) == 0) {
		/*
		 * We only want to take "i_tlock" if we absolutely
		 * have to. In order to keep this neat, we use a
		 * local variable (flags_to_set) to keep track of the
		 * flags that need to be set. At the end if there are
		 * any flags to set, we do it all at once.
		 */
		int flags_to_set = 0;
		if ((rw != S_OTHER) && (ip->i_mode & IFMT) != IFDIR)
			if (!ULOCKFS_IS_NOIACC(ITOUL(ip)) &&
			    (fs->fs_ronly == 0) &&
			    (!(ip)->i_ufsvfs->vfs_noatime)) {
				flags_to_set |= IACC;
		}
		if (rw == S_WRITE)
			flags_to_set |= IUPD;
		if (flags_to_set) {
			mutex_enter(&ip->i_tlock);
			ip->i_flag |= flags_to_set;
			ITIMES_NOLOCK(ip);
			mutex_exit(&ip->i_tlock);
		}
	}

	if (dolock) {
		rw_exit(&ip->i_contents);
		if (do_qlock && rwtype == RW_WRITER)
			rw_exit(&ufsvfsp->vfs_dqrwlock);
	}

unlock:
	if (ulp) {
		if ((rw == S_CREATE || rw == S_WRITE) &&
		    !(vp->v_flag & VISSWAP)) {
			TRANS_END_ASYNC(ufsvfsp, TOP_GETPAGE, trans_size);
		}
		ufs_lockfs_end(ulp);
	}
out:
	TRACE_2(TR_FAC_UFS, TR_UFS_GETPAGE_END,
		"ufs_getpage_end:vp %p error %d", vp, err);
	return (err);
}

/*
 * ufs_getpage_miss is called when ufs_getpage missed the page in the page
 * cache. The page is either read from the disk, or it's created.
 * A page is created (without disk read) if rw == S_CREATE, or if
 * the page is not backed with a real disk block (UFS hole).
 */
/* ARGSUSED */
static int
ufs_getpage_miss(vp, off, len, seg, addr, pl, plsz, rw, seq)
	struct vnode 	*vp;
	u_offset_t	off;
	size_t 		len;
	struct seg 	*seg;
	caddr_t 	addr;
	page_t 		*pl[];
	size_t		plsz;
	enum seg_rw 	rw;
	int 		seq;
{
	struct inode	*ip = VTOI(vp);
	int		crpage;
	int		err;
	daddr_t		bn;
	int		contig;
	size_t		io_len;
	page_t		*pp;
	int		bsize = ip->i_fs->fs_bsize;
	klwp_t		*lwp = ttolwp(curthread);

	/*
	 * Figure out whether the page can be created, or must be
	 * must be read from the disk.
	 */
	if (rw == S_CREATE)
		crpage = 1;
	else {
		contig = 0;
		if (err = bmap_read(ip, off, &bn, &contig))
			return (err);
		crpage = (bn == UFS_HOLE);
		if (bn == UFS_HOLE && rw == S_WRITE) {
			return (ufs_fault(vp,
	"ufs_getpage_miss: bn == UFS_HOLE and rw == S_WRITE (vp: %p ufsvfs %p)",
				(void *)vp, (void *)ip->i_ufsvfs));
		}
	}

	if (crpage) {
		if ((pp = page_create_va(vp, off, PAGESIZE, PG_WAIT, seg,
		    addr)) == NULL) {
			return (ufs_fault(vp,
				    "ufs_getpage_miss: page_create == NULL"));
		}

		if (rw != S_CREATE)
			pagezero(pp, 0, PAGESIZE);
		io_len = PAGESIZE;
	} else {
		u_offset_t	io_off;
		uint_t	xlen;
		struct buf	*bp;
		ufsvfs_t	*ufsvfsp = ip->i_ufsvfs;

		/*
		 * If access is not in sequential order, we read from disk
		 * in bsize units.
		 *
		 * We limit the size of the transfer to bsize if we are reading
		 * from the beginning of the file. Note in this situation we
		 * will hedge our bets and initiate an async read ahead of
		 * the second block.
		 */
		if (!seq || off == 0)
			contig = MIN(contig, bsize);

		pp = pvn_read_kluster(vp, off, seg, addr, &io_off,
		    &io_len, off, contig, 0);

		/*
		 * Some other thread has entered the page.
		 * ufs_getpage will retry page_lookup.
		 */
		if (pp == NULL) {
			pl[0] = NULL;
			return (0);
		}

		/*
		 * Zero part of the page which we are not
		 * going to read from the disk.
		 */
		xlen = io_len & PAGEOFFSET;
		if (xlen != 0)
			pagezero(pp->p_prev, xlen, PAGESIZE - xlen);

		bp = pageio_setup(pp, io_len, ip->i_devvp, B_READ);
		bp->b_edev = ip->i_dev;
		bp->b_dev = cmpdev(ip->i_dev);
		bp->b_blkno = bn;
		bp->b_un.b_addr = (caddr_t)0;
		if (ufsvfsp->vfs_log) {
			LUFS_STRATEGY(ufsvfsp->vfs_log, bp);
		} else {
			ufsvfsp->vfs_iotstamp = lbolt;
			ub.ub_getpages.value.ul++;
			(void) bdev_strategy(bp);
			if (lwp != NULL)
				lwp->lwp_ru.inblock++;
		}

		ip->i_nextrio = off + ((io_len + PAGESIZE - 1) & PAGEMASK);

		/*
		 * If the file access is sequential, initiate read ahead
		 * of the next cluster.
		 */
		if (seq && ip->i_nextrio < ip->i_size)
			(void) ufs_getpage_ra(vp, off, seg, addr);
		err = biowait(bp);
		pageio_done(bp);

		if (err) {
			pvn_read_done(pp, B_ERROR);
			return (err);
		}
	}

	pvn_plist_init(pp, pl, plsz, off, io_len, rw);
	return (0);
}

/*
 * Read ahead a cluster from the disk. Returns the length in bytes.
 */
static int
ufs_getpage_ra(vp, off, seg, addr)
	struct vnode 	*vp;
	u_offset_t	off;
	struct seg 	*seg;
	caddr_t 	addr;
{
	struct inode	*ip = VTOI(vp);
	u_offset_t	io_off = ip->i_nextrio;
	caddr_t		addr2 = addr + (io_off - off);
	daddr_t		bn;
	int		contig;
	size_t		io_len;
	page_t		*pp;
	int		xlen;
	int		bsize = ip->i_fs->fs_bsize;
	struct buf	*bp;
	ufsvfs_t	*ufsvfsp;
	klwp_t		*lwp = ttolwp(curthread);

	/*
	 * Is this test needed?
	 */
	if (addr2 >= seg->s_base + seg->s_size)
		return (0);

	contig = 0;
	if (bmap_read(ip, io_off, &bn, &contig) != 0 || bn == UFS_HOLE)
		return (0);

	/*
	 * Limit the transfer size to bsize if this is the 2nd block.
	 */
	if (io_off == (u_offset_t)bsize)
		contig = MIN(contig, bsize);

	if ((pp = pvn_read_kluster(vp, io_off, seg, addr2, &io_off,
	    &io_len, io_off, contig, 1)) == NULL)
		return (0);

	/*
	 * Zero part of page which we are not going to read from disk
	 */
	if ((xlen = (io_len & PAGEOFFSET)) > 0)
		pagezero(pp->p_prev, xlen, PAGESIZE - xlen);

	ip->i_nextrio = (io_off + io_len + PAGESIZE - 1) & PAGEMASK;

	bp = pageio_setup(pp, io_len, ip->i_devvp, B_READ | B_ASYNC);
	bp->b_edev = ip->i_dev;
	bp->b_dev = cmpdev(ip->i_dev);
	bp->b_blkno = bn;
	bp->b_un.b_addr = (caddr_t)0;
	ufsvfsp = ip->i_ufsvfs;
	if (ufsvfsp->vfs_log) {
		LUFS_STRATEGY(ufsvfsp->vfs_log, bp);
	} else {
		ufsvfsp->vfs_iotstamp = lbolt;
		ub.ub_getras.value.ul++;
		(void) bdev_strategy(bp);
		if (lwp != NULL)
			lwp->lwp_ru.inblock++;
	}

	return (io_len);
}

int	ufs_delay = 1;
/*
 * Flags are composed of {B_INVAL, B_FREE, B_DONTNEED, B_FORCE, B_ASYNC}
 *
 * LMXXX - the inode really ought to contain a pointer to one of these
 * async args.  Stuff gunk in there and just hand the whole mess off.
 * This would replace i_delaylen, i_delayoff.
 */
/*ARGSUSED*/
static int
ufs_putpage(vp, off, len, flags, cr)
	register struct vnode *vp;
	offset_t off;
	size_t len;
	int flags;
	struct cred *cr;
{
	register struct inode *ip = VTOI(vp);
	int err = 0;

	if (vp->v_count == 0) {
		return (ufs_fault(vp, "ufs_putpage: bad v_count == 0"));
	}

	TRACE_1(TR_FAC_UFS, TR_UFS_PUTPAGE_START,
		"ufs_putpage_start:vp %p", vp);

	/*
	 * XXX - Why should this check be made here?
	 */
	if (vp->v_flag & VNOMAP) {
		err = ENOSYS;
		goto errout;
	}

	if (ip->i_ufsvfs == NULL) {
		err = EIO;
		goto errout;
	}

	if (flags & B_ASYNC) {
		if (ufs_delay && len &&
		    (flags & ~(B_ASYNC|B_DONTNEED|B_FREE)) == 0) {
			mutex_enter(&ip->i_tlock);
			/*
			 * If nobody stalled, start a new cluster.
			 */
			if (ip->i_delaylen == 0) {
				ip->i_delayoff = off;
				ip->i_delaylen = len;
				mutex_exit(&ip->i_tlock);
				goto errout;
			}
			/*
			 * If we have a full cluster or they are not contig,
			 * then push last cluster and start over.
			 */
			if (ip->i_delaylen >= WR_CLUSTSZ(ip) ||
			    ip->i_delayoff + ip->i_delaylen != off) {
				u_offset_t doff;
				size_t dlen;

				doff = ip->i_delayoff;
				dlen = ip->i_delaylen;
				ip->i_delayoff = off;
				ip->i_delaylen = len;
				mutex_exit(&ip->i_tlock);
				err = ufs_putpages(vp, doff, dlen,
				    flags, cr);
				/* LMXXX - flags are new val, not old */
				goto errout;
			}
			/*
			 * There is something there, it's not full, and
			 * it is contig.
			 */
			ip->i_delaylen += len;
			mutex_exit(&ip->i_tlock);
			goto errout;
		}
		/*
		 * Must have weird flags or we are not clustering.
		 */
	}

	err = ufs_putpages(vp, off, len, flags, cr);

errout:
	TRACE_2(TR_FAC_UFS, TR_UFS_PUTPAGE_END,
		"ufs_putpage_end:vp %p error %d", vp, err);
	return (err);
}

/*
 * If len == 0, do from off to EOF.
 *
 * The normal cases should be len == 0 & off == 0 (entire vp list),
 * len == MAXBSIZE (from segmap_release actions), and len == PAGESIZE
 * (from pageout).
 */
/*ARGSUSED*/
static int
ufs_putpages(
	register struct vnode *vp,
	offset_t off,
	size_t len,
	int flags,
	struct cred *cr)
{
	register struct inode *ip = VTOI(vp);
	register page_t *pp;
	u_offset_t io_off;
	size_t io_len;
	register u_offset_t eoff;
	int err = 0;
	int dolock;

	if (vp->v_count == 0)
		return (ufs_fault(vp, "ufs_putpages: v_count == 0"));
	/*
	 * Acquire the readers/write inode lock before locking
	 * any pages in this inode.
	 * The inode lock is held during i/o.
	 */
	if (len == 0) {
		mutex_enter(&ip->i_tlock);
		ip->i_delayoff = ip->i_delaylen = 0;
		mutex_exit(&ip->i_tlock);
	}
	dolock = (rw_owner(&ip->i_contents) != curthread);
	if (dolock)
		rw_enter(&ip->i_contents, RW_READER);

	if (vp->v_pages == NULL) {
		if (dolock)
			rw_exit(&ip->i_contents);
		return (0);
	}

	if (len == 0) {
		/*
		 * Search the entire vp list for pages >= off.
		 */
		err = pvn_vplist_dirty(vp, (u_offset_t)off, ufs_putapage,
					flags, cr);
	} else {
		/*
		 * Loop over all offsets in the range looking for
		 * pages to deal with.
		 */
		if ((eoff = blkroundup(ip->i_fs, ip->i_size)) != 0)
			eoff = MIN(off + len, eoff);
		else
			eoff = off + len;

		for (io_off = off; io_off < eoff; io_off += io_len) {
			/*
			 * If we are not invalidating, synchronously
			 * freeing or writing pages, use the routine
			 * page_lookup_nowait() to prevent reclaiming
			 * them from the free list.
			 */
			if ((flags & B_INVAL) || ((flags & B_ASYNC) == 0)) {
				pp = page_lookup(vp, io_off,
					(flags & (B_INVAL | B_FREE)) ?
					    SE_EXCL : SE_SHARED);
			} else {
				pp = page_lookup_nowait(vp, io_off,
					(flags & B_FREE) ? SE_EXCL : SE_SHARED);
			}

			if (pp == NULL || pvn_getdirty(pp, flags) == 0)
				io_len = PAGESIZE;
			else {
				u_offset_t *io_offp = &io_off;

				err = ufs_putapage(vp, pp, io_offp, &io_len,
				    flags, cr);
				if (err != 0)
					break;
				/*
				 * "io_off" and "io_len" are returned as
				 * the range of pages we actually wrote.
				 * This allows us to skip ahead more quickly
				 * since several pages may've been dealt
				 * with by this iteration of the loop.
				 */
			}
		}
	}
	if (dolock)
		rw_exit(&ip->i_contents);
	return (err);
}

static void
ufs_iodone(buf_t *bp)
{
	struct inode *ip;

	ASSERT((bp->b_pages->p_vnode != NULL) && !(bp->b_flags & B_READ));

	bp->b_iodone = NULL;

	ip = VTOI(bp->b_pages->p_vnode);

	mutex_enter(&ip->i_tlock);
	if (ip->i_writes >= ufs_LW) {
		if ((ip->i_writes -= bp->b_bcount) <= ufs_LW)
			if (ufs_WRITES)
				cv_broadcast(&ip->i_wrcv); /* wake all up */
	} else {
		ip->i_writes -= bp->b_bcount;
	}

	mutex_exit(&ip->i_tlock);
	iodone(bp);
}

/*
 * Disable userdata logging for now. BJF
 */
int ufs_no_userdata = 1;

/*
 * Write out a single page, possibly klustering adjacent
 * dirty pages.  The inode lock must be held.
 *
 * LMXXX - bsize < pagesize not done.
 */
/*ARGSUSED*/
int
ufs_putapage(
	struct vnode *vp,
	page_t *pp,
	u_offset_t *offp,
	size_t *lenp,		/* return values */
	int flags,
	struct cred *cr)
{
	struct inode *ip = VTOI(vp);
	struct fs *fs;
	u_offset_t io_off;
	size_t io_len;
	daddr_t bn;
	int err;
	struct buf *bp;
	u_offset_t off;
	int contig;
	struct ufsvfs *ufsvfsp = ip->i_ufsvfs;
	klwp_t *lwp = ttolwp(curthread);

	ASSERT(RW_LOCK_HELD(&ip->i_contents));

	TRACE_1(TR_FAC_UFS, TR_UFS_PUTAPAGE_START,
		"ufs_putapage_start:vp %p", vp);

	fs = ip->i_fs;
	ASSERT(fs->fs_ronly == 0);

	/*
	 * If the modified time on the inode has not already been
	 * set elsewhere (e.g. for write/setattr) we set the time now.
	 * This gives us approximate modified times for mmap'ed files
	 * which are modified via stores in the user address space.
	 */
	if ((ip->i_flag & IMODTIME) == 0) {
		mutex_enter(&ip->i_tlock);
		ip->i_flag |= IUPD;
		ITIMES_NOLOCK(ip);
		mutex_exit(&ip->i_tlock);
	}

	/*
	 * Align the request to a block boundry (for old file systems),
	 * and go ask bmap() how contiguous things are for this file.
	 */
	off = pp->p_offset & (offset_t)fs->fs_bmask;	/* block align it */
	contig = 0;
	err = bmap_read(ip, off, &bn, &contig);
	if (err)
		goto out;
	if (bn == UFS_HOLE) {			/* putpage never allocates */
		/*
		 * logging device is in error mode; simply return EIO
		 */
		if (TRANS_ISERROR(ufsvfsp)) {
			err = EIO;
			goto out;
		}
		err = ufs_fault(ITOV(ip), "ufs_putapage: bn == UFS_HOLE");
		goto out;
	}

	/*
	 * Take the length (of contiguous bytes) passed back from bmap()
	 * and _try_ and get a set of pages covering that extent.
	 */
	pp = pvn_write_kluster(vp, pp, &io_off, &io_len, off, contig, flags);

	/*
	 * May have run out of memory and not clustered backwards.
	 * off		p_offset
	 * [  pp - 1  ][   pp   ]
	 * [	block		]
	 * We told bmap off, so we have to adjust the bn accordingly.
	 */
	if (io_off > off) {
		bn += btod(io_off - off);
		contig -= (io_off - off);
	}

	/*
	 * bmap was carefull to tell us the right size so use that.
	 * There might be unallocated frags at the end.
	 * LMXXX - bzero the end of the page?  We must be writing after EOF.
	 */
	if (io_len > contig) {
		ASSERT(io_len - contig < fs->fs_bsize);
		io_len -= (io_len - contig);
	}

	/*
	 * Handle the case where we are writing the last page after EOF.
	 *
	 * XXX - just a patch for i-mt3.
	 */
	if (io_len == 0) {
		ASSERT(pp->p_offset >= (u_offset_t)(roundup(ip->i_size,
							    PAGESIZE)));
		io_len = PAGESIZE;
	}

	bp = pageio_setup(pp, io_len, ip->i_devvp, B_WRITE | flags);

	ULOCKFS_SET_MOD(ITOUL(ip));

	bp->b_edev = ip->i_dev;
	bp->b_dev = cmpdev(ip->i_dev);
	bp->b_blkno = bn;
	bp->b_un.b_addr = (caddr_t)0;

	if (TRANS_ISTRANS(ufsvfsp)) {
		if ((ip->i_mode & IFMT) == IFSHAD) {
			TRANS_BUF(ufsvfsp, 0, io_len, bp, DT_SHAD);
		} else if (ufsvfsp->vfs_qinod == ip) {
			TRANS_DELTA(ufsvfsp, ldbtob(bn), bp->b_bcount, DT_QR,
			    0, 0);
		/*
		 * Optionally log synchronously written userdata
		 */
		} else if (((flags & (B_INVAL | B_ASYNC)) == 0) &&
		    (!ufs_no_userdata) && (curthread->t_flag & T_DONTBLOCK) &&
		    ((ip->i_mode & IFMT) != IFDIR)) {
			int islogged;
			TRANS_UD_BUF(ufsvfsp, islogged, 0, io_len, bp, DT_UD);
			if (islogged)
				bp->b_flags |= B_ASYNC;
		}
	}

	/* write throttle */

	ASSERT(bp->b_iodone == NULL);
	bp->b_iodone = (int (*)())ufs_iodone;
	mutex_enter(&ip->i_tlock);
	ip->i_writes += bp->b_bcount;
	mutex_exit(&ip->i_tlock);

	if (bp->b_flags & B_ASYNC) {
		if (ufsvfsp->vfs_log) {
			LUFS_STRATEGY(ufsvfsp->vfs_log, bp);
		} else {
			ufsvfsp->vfs_iotstamp = lbolt;
			ub.ub_putasyncs.value.ul++;
			(void) bdev_strategy(bp);
			if (lwp != NULL)
				lwp->lwp_ru.oublock++;
		}
	} else {
		if (ufsvfsp->vfs_log) {
			LUFS_STRATEGY(ufsvfsp->vfs_log, bp);
		} else {
			ufsvfsp->vfs_iotstamp = lbolt;
			ub.ub_putsyncs.value.ul++;
			(void) bdev_strategy(bp);
			if (lwp != NULL)
				lwp->lwp_ru.oublock++;
		}
		err = biowait(bp);
		pageio_done(bp);
		pvn_write_done(pp, ((err) ? B_ERROR : 0) | B_WRITE | flags);
	}

	pp = NULL;

out:
	if (err != 0 && pp != NULL)
		pvn_write_done(pp, B_ERROR | B_WRITE | flags);

	if (offp)
		*offp = io_off;
	if (lenp)
		*lenp = io_len;
	TRACE_2(TR_FAC_UFS, TR_UFS_PUTAPAGE_END,
		"ufs_putapage_end:vp %p error %d", vp, err);
	return (err);
}

/* ARGSUSED */
static int
ufs_map(struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t *addrp,
	size_t len,
	uchar_t prot,
	uchar_t maxprot,
	uint_t flags,
	struct cred *cr)
{
	struct segvn_crargs vn_a;
	int error;
	struct ufsvfs *ufsvfsp = VTOI(vp)->i_ufsvfs;
	struct ulockfs *ulp;

	TRACE_1(TR_FAC_UFS, TR_UFS_MAP_START,
		"ufs_map_start:vp %p", vp);

	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_MAP_MASK);
	if (error)
		goto out;

	if (vp->v_flag & VNOMAP) {
		error = ENOSYS;
		goto unlock;
	}

	if (off < (offset_t)0 || (offset_t)(off + len) < (offset_t)0) {
		error = EINVAL;
		goto unlock;
	}

	if (vp->v_type != VREG) {
		error = ENODEV;
		goto unlock;
	}

	/*
	 * If file is being locked, disallow mapping.
	 */
	if (vp->v_filocks != NULL && MANDLOCK(vp, VTOI(vp)->i_mode)) {
		error = EAGAIN;
		goto unlock;
	}

	as_rangelock(as);
	if ((flags & MAP_FIXED) == 0) {
		map_addr(addrp, len, off, 1, flags);
		if (*addrp == NULL) {
			as_rangeunlock(as);
			error = ENOMEM;
			goto unlock;
		}
	} else {
		/*
		 * User specified address - blow away any previous mappings
		 */
		(void) as_unmap(as, *addrp, len);
	}

	vn_a.vp = vp;
	vn_a.offset = (u_offset_t)off;
	vn_a.type = flags & MAP_TYPE;
	vn_a.prot = prot;
	vn_a.maxprot = maxprot;
	vn_a.cred = cr;
	vn_a.amp = NULL;
	vn_a.flags = flags & ~MAP_TYPE;

	error = as_map(as, *addrp, len, segvn_create, &vn_a);
	as_rangeunlock(as);

unlock:
	if (ulp) {
		ufs_lockfs_end(ulp);
	}
out:
	TRACE_2(TR_FAC_UFS, TR_UFS_MAP_END,
		"ufs_map_end:vp %p error %d", vp, error);
	return (error);
}

/* ARGSUSED */
static int
ufs_addmap(struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t	len,
	uchar_t  prot,
	uchar_t  maxprot,
	uint_t    flags,
	struct cred *cr)
{
	register struct inode *ip = VTOI(vp);

	if (vp->v_flag & VNOMAP) {
		return (ENOSYS);
	}

	mutex_enter(&ip->i_tlock);
	ip->i_mapcnt += btopr(len);
	mutex_exit(&ip->i_tlock);
	return (0);
}

/*ARGSUSED*/
static int
ufs_delmap(vp, off, as, addr, len, prot, maxprot, flags, cr)
	struct vnode *vp;
	offset_t off;
	struct as *as;
	caddr_t addr;
	size_t len;
	uint_t prot, maxprot;
	uint_t flags;
	struct cred *cr;
{
	register struct inode *ip = VTOI(vp);

	if (vp->v_flag & VNOMAP) {
		return (ENOSYS);
	}

	mutex_enter(&ip->i_tlock);
	ip->i_mapcnt -= btopr(len); 	/* Count released mappings */
	ASSERT(ip->i_mapcnt >= 0);
	mutex_exit(&ip->i_tlock);
	return (0);
}
/*
 * Return the answer requested to poll() for non-device files
 */
struct pollhead ufs_pollhd;

/* ARGSUSED */
int
ufs_poll(vnode_t *vp, short ev, int any, short *revp, struct pollhead **phpp)
{
	struct ufsvfs	*ufsvfsp;

	*revp = 0;
	ufsvfsp = VTOI(vp)->i_ufsvfs;

	if (!ufsvfsp) {
		*revp = POLLHUP;
		goto out;
	}

	if (ULOCKFS_IS_HLOCK(&ufsvfsp->vfs_ulockfs) ||
	    ULOCKFS_IS_ELOCK(&ufsvfsp->vfs_ulockfs)) {
		*revp |= POLLERR;

	} else {
		if ((ev & POLLOUT) && !ufsvfsp->vfs_fs->fs_ronly &&
		    !ULOCKFS_IS_WLOCK(&ufsvfsp->vfs_ulockfs))
			*revp |= POLLOUT;

		if ((ev & POLLWRBAND) && !ufsvfsp->vfs_fs->fs_ronly &&
		    !ULOCKFS_IS_WLOCK(&ufsvfsp->vfs_ulockfs))
			*revp |= POLLWRBAND;

		if (ev & POLLIN)
			*revp |= POLLIN;

		if (ev & POLLRDNORM)
			*revp |= POLLRDNORM;

		if (ev & POLLRDBAND)
			*revp |= POLLRDBAND;
	}

	if ((ev & POLLPRI) && (*revp & (POLLERR|POLLHUP)))
		*revp |= POLLPRI;
out:
	*phpp = !any && !*revp ? &ufs_pollhd : (struct pollhead *)NULL;

	return (0);
}

/* ARGSUSED */
static int
ufs_l_pathconf(vp, cmd, valp, cr)
	struct vnode	*vp;
	int		cmd;
	ulong_t		*valp;
	struct cred	*cr;
{
	int		error;
	struct ufsvfs	*ufsvfsp = VTOI(vp)->i_ufsvfs;
	struct ulockfs	*ulp;

	error = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_PATHCONF_MASK);
	if (error)
		return (error);

	if (cmd == _PC_FILESIZEBITS) {
		if (ufsvfsp->vfs_lfflags & UFS_LARGEFILES)
			*valp = UFS_FILESIZE_BITS;
		else
			*valp = 32;
	} else
		error = fs_pathconf(vp, cmd, valp, cr);

	if (ulp) {
		ufs_lockfs_end(ulp);
	}
	return (error);
}

int ufs_pageio_writes, ufs_pageio_reads;

/*ARGSUSED*/
static int
ufs_pageio(vp, pp, io_off, io_len, flags, cr)
	register struct vnode *vp;
	page_t *pp;
	u_offset_t io_off;
	size_t io_len;
	int flags;
	cred_t *cr;
{
	struct inode *ip = VTOI(vp);
	int err = 0;
	size_t done_len = 0, cur_len = 0;
	int contig = 0;
	page_t *npp = NULL, *opp = NULL, *cpp = pp;
	daddr_t bn;
	struct buf *bp;
	int dolock;
	klwp_t *lwp = ttolwp(curthread);

	if (pp == NULL)
		return (EINVAL);

	dolock = (rw_owner(&ip->i_contents) != curthread);
	/*
	 * We need a better check.  Ideally, we would use another
	 * vnodeops so that hlocked and forcibly unmounted file
	 * systems would return EIO where appropriate and w/o the
	 * need for these checks.
	 */
	if (ip->i_ufsvfs == NULL)
		return (EIO);

	if (dolock)
		rw_enter(&ip->i_contents, RW_READER);
	/*
	 * Break the io request into chunks, one for each contiguous
	 * stretch of disk blocks in the target file.
	 */
	while (done_len < io_len) {
		ASSERT(cpp);
		contig = 0;
		if (err = bmap_read(ip, (u_offset_t)(io_off + done_len),
				    &bn, &contig))
			break;

		if (bn == UFS_HOLE) {	/* No holey swapfiles */
			err = ufs_fault(ITOV(ip), "ufs_pageio: bn == UFS_HOLE");
			break;
		}

		cur_len = MIN(io_len - done_len, contig);
		page_list_break(&cpp, &npp, btop(cur_len));

		bp = pageio_setup(cpp, cur_len, ip->i_devvp, flags);
		ASSERT(bp != NULL);

		bp->b_edev = ip->i_dev;
		bp->b_dev = cmpdev(ip->i_dev);
		bp->b_blkno = bn;
		bp->b_un.b_addr = (caddr_t)0;

		ip->i_ufsvfs->vfs_iotstamp = lbolt;
		ub.ub_pageios.value.ul++;
		(void) bdev_strategy(bp);
		if (flags & B_READ)
			ufs_pageio_reads++;
		else
			ufs_pageio_writes++;
		if (lwp != NULL) {
			if (flags & B_READ)
				lwp->lwp_ru.inblock++;
			else
				lwp->lwp_ru.oublock++;
		}
		/*
		 * If the request is not B_ASYNC, wait for i/o to complete
		 * and re-assemble the page list to return to the caller.
		 * If it is B_ASYNC we leave the page list in pieces and
		 * cleanup() will dispose of them.
		 */
		if ((flags & B_ASYNC) == 0) {
			err = biowait(bp);
			pageio_done(bp);
			if (err)
				break;
			page_list_concat(&opp, &cpp);
		}
		cpp = npp;
		npp = NULL;
		done_len += cur_len;
	}
	ASSERT(err || (cpp == NULL && npp == NULL && done_len == io_len));
	if (err) {
		if (flags & B_ASYNC) {
			/* Cleanup unprocessed parts of list */
			page_list_concat(&cpp, &npp);
			if (flags & B_READ)
				pvn_read_done(cpp, B_ERROR);
			else
				pvn_write_done(cpp, B_ERROR);
		} else {
			/* Re-assemble list and let caller clean up */
			page_list_concat(&opp, &cpp);
			page_list_concat(&opp, &npp);
		}
	}
	if (dolock)
		rw_exit(&ip->i_contents);
	return (err);
}

/*
 * Called when the kernel is in a frozen state to dump data
 * directly to the device. It uses a private dump data structure,
 * set up by dump_ctl, to locate the correct disk block to which to dump.
 */
static int
ufs_dump(vnode_t *vp, caddr_t addr, int ldbn, int dblks)
{
	struct inode    *ip = VTOI(vp);
	struct fs	*fs = ip->i_fs;
	int		disk_blks = fs->fs_bsize >> DEV_BSHIFT;
	int		error = 0;
	daddr_t		dbn, lfsbn;
	int		ndbs, nfsbs;
	u_offset_t	file_size;

	/*
	 * forced unmount case
	 */
	if (ip->i_ufsvfs == NULL)
		return (EIO);
	/*
	 * Validate the inode that it has not been modified since
	 * the dump structure is allocated.
	 */
	mutex_enter(&ip->i_tlock);
	if ((dump_info == NULL) ||
	    (dump_info->ip != ip) ||
	    (dump_info->time.tv_sec != ip->i_mtime.tv_sec) ||
	    (dump_info->time.tv_usec != ip->i_mtime.tv_usec)) {
		mutex_exit(&ip->i_tlock);
		return (-1);
	}
	mutex_exit(&ip->i_tlock);

	/*
	 * See that the file has room for this write
	 */
	UFS_GET_ISIZE(&file_size, ip);

	if (ldbtob((offset_t)(ldbn + dblks)) > file_size)
		return (ENOSPC);

	/*
	 * Find the physical disk block numbers from the dump
	 * private data structure directly and write out the data
	 * in contiguous block lumps
	 */
	while (dblks > 0 && !error) {
		lfsbn = (daddr_t)lblkno(fs, ldbtob((offset_t)ldbn));
		dbn = fsbtodb(fs, dump_info->dblk[lfsbn]) + ldbn % disk_blks;
		nfsbs = 1;
		ndbs = disk_blks - ldbn % disk_blks;
		while (ndbs < dblks && fsbtodb(fs, dump_info->dblk[lfsbn +
		    nfsbs]) == dbn + ndbs) {
			nfsbs++;
			ndbs += disk_blks;
		}
		if (ndbs > dblks)
			ndbs = dblks;
		error = bdev_dump(ip->i_dev, addr, dbn, ndbs);
		addr += ldbtob((offset_t)ndbs);
		dblks -= ndbs;
		ldbn += ndbs;
	}
	return (error);

}

/*
 * Prepare the file system before and after the dump operation.
 *
 * action = DUMP_ALLOC:
 * Preparation before dump, allocate dump private data structure
 * to hold all the direct and indirect block info for dump.
 *
 * action = DUMP_FREE:
 * Clean up after dump, deallocate the dump private data structure.
 *
 * action = DUMP_SCAN:
 * Scan dump_info for *blkp DEV_BSIZE blocks of contig fs space;
 * if found, the starting file-relative DEV_BSIZE lbn is written
 * to *bklp; that lbn is intended for use with VOP_DUMP()
 */
static int
ufs_dumpctl(vnode_t *vp, int action, int *blkp)
{
	struct inode	*ip = VTOI(vp);
	ufsvfs_t	*ufsvfsp = ip->i_ufsvfs;
	struct fs	*fs;
	int		i, entry, entries;
	int		n, ncontig;
	daddr32_t	*dblk, *storeblk;
	daddr32_t	*nextblk, *endblk;
	struct buf	*bp;

	if (action == DUMP_ALLOC) {
		/*
		 * alloc and record dump_info
		 */
		if (dump_info != NULL)
			return (EINVAL);

		/*
		 * check for forced unmount
		 */
		if (ufsvfsp == NULL)
			return (EIO);

		ASSERT(vp->v_type == VREG);
		fs = ufsvfsp->vfs_fs;

		rw_enter(&ip->i_contents, RW_READER);

		if (bmap_has_holes(ip)) {
			rw_exit(&ip->i_contents);
			return (EFAULT);
		}

		/*
		 * calculate and allocate space needed according to i_size
		 */
		entries = (int)lblkno(fs, blkroundup(fs, ip->i_size));
		if ((dump_info = (struct dump *)
		    kmem_alloc(sizeof (struct dump) +
		    (entries - 1) * sizeof (daddr32_t), KM_NOSLEEP)) == NULL) {
			    rw_exit(&ip->i_contents);
			    return (ENOMEM);
		}

		/* Start saving the info */
		dump_info->fsbs = entries;
		dump_info->ip = ip;
		storeblk = &dump_info->dblk[0];

		/* Direct Blocks */
		for (entry = 0; entry < NDADDR && entry < entries; entry++)
			*storeblk++ = ip->i_db[entry];

		/* Indirect Blocks */
		for (i = 0; i < NIADDR; i++) {
			bp = UFS_BREAD(ufsvfsp,
				ip->i_dev, fsbtodb(fs, ip->i_ib[i]),
				fs->fs_bsize);
			dblk = bp->b_un.b_daddr;
			if ((storeblk = save_dblks(ip, storeblk, dblk, i,
			    entries)) == 0) {
				kmem_free(dump_info, sizeof (struct dump) +
				    (entries - 1) * sizeof (daddr32_t));
				brelse(bp);
				rw_exit(&ip->i_contents);
				return (EIO);
			}
			brelse(bp);
		}
		rw_exit(&ip->i_contents);

		/* and time stamp the information */
		mutex_enter(&ip->i_tlock);
		dump_info->time = ip->i_mtime;
		mutex_exit(&ip->i_tlock);
	} else if (action == DUMP_FREE) {
		/*
		 * free dump_info
		 */
		if (dump_info == NULL)
			return (EINVAL);
		entries = dump_info->fsbs - 1;
		kmem_free(dump_info, sizeof (struct dump) +
		    entries * sizeof (daddr32_t));
		dump_info = NULL;
	} else if (action == DUMP_SCAN) {
		/*
		 * scan dump_info
		 */
		if (dump_info == NULL)
			return (EINVAL);

		dblk = dump_info->dblk;
		nextblk = dblk + 1;
		endblk = dblk + dump_info->fsbs - 1;
		fs = ufsvfsp->vfs_fs;
		ncontig = *blkp >> (fs->fs_bshift - DEV_BSHIFT);

		/*
		 * scan dblk[] entries; contig fs space is found when:
		 * ((current blkno + frags per block) == next blkno)
		 */
		n = 0;
		while (n < ncontig && dblk < endblk) {
			if ((*dblk + fs->fs_frag) == *nextblk)
				n++;
			else
				n = 0;
			dblk++;
			nextblk++;
		}

		/*
		 * index is where size bytes of contig space begins;
		 * conversion from index to the file's DEV_BSIZE lbn
		 * is equivalent to:  (index * fs_bsize) / DEV_BSIZE
		 */
		if (n == ncontig) {
			i = (dblk - dump_info->dblk) - ncontig;
			*blkp = i << (fs->fs_bshift - DEV_BSHIFT);
		} else
			return (EFAULT);
	}
	return (0);
}

/*
 * Recursive helper function for ufs_dumpctl().  It follows the indirect file
 * system  blocks until it reaches the the disk block addresses, which are
 * then stored into the given buffer, storeblk.
 */
static daddr32_t *
save_dblks(struct inode *ip, daddr32_t *storeblk, daddr32_t *dblk,
    int level, int entries)
{
	ufsvfs_t	*ufsvfsp = ip->i_ufsvfs;
	struct fs	*fs = ufsvfsp->vfs_fs;
	struct buf	*bp;
	int		i;

	if (level == 0) {
		for (i = 0; i < NINDIR(fs); i++) {
			if (storeblk - dump_info->dblk >= entries)
				break;
			*storeblk++ = dblk[i];
		}
		return (storeblk);
	}
	for (i = 0; i < NINDIR(fs); i++) {
		if (storeblk - dump_info->dblk >= entries)
			break;
		bp = UFS_BREAD(ufsvfsp,
				ip->i_dev, fsbtodb(fs, dblk[i]), fs->fs_bsize);
		if (bp->b_flags & B_ERROR) {
			brelse(bp);
			return (NULL);
		}
		storeblk = save_dblks(ip, storeblk, bp->b_un.b_daddr,
		    level - 1, entries);
		brelse(bp);
	}
	return (storeblk);
}

/* ARGSUSED */
static int
ufs_getsecattr(vp, vsap, flag, cr)
	struct vnode *vp;
	vsecattr_t *vsap;
	int flag;
	struct cred *cr;
{
	struct inode	*ip = VTOI(vp);
	struct ulockfs	*ulp;
	struct ufsvfs *ufsvfsp = ip->i_ufsvfs;
	ulong_t		vsa_mask = vsap->vsa_mask;
	int		err = 0;

	TRACE_3(TR_FAC_UFS, TR_UFS_GETSECATTR_START,
	    "ufs_getsecattr_start:vp %p, vsap %p, flags %x", vp, vsap, flag);

	if (err = ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_GETATTR_MASK))
		return (err);

	vsa_mask &= (VSA_ACL | VSA_ACLCNT | VSA_DFACL | VSA_DFACLCNT);

	rw_enter(&ip->i_contents, RW_READER);
	if (vsa_mask != 0) {
		err = ufs_acl_get(ip, vsap, flag, cr);
	}
out:
	rw_exit(&ip->i_contents);
	if (ulp) {
		ufs_lockfs_end(ulp);
	}
	TRACE_1(TR_FAC_UFS, TR_UFS_GETSECATTR_END,
	    "ufs_getsecattr_end:vp %p", vp);
	return (err);
}

/* ARGSUSED */
static int
ufs_setsecattr(vp, vsap, flag, cr)
	struct vnode *vp;
	vsecattr_t *vsap;
	int flag;
	struct cred *cr;
{
	struct inode	*ip = VTOI(vp);
	struct ulockfs	*ulp;
	struct ufsvfs *ufsvfsp = VTOI(vp)->i_ufsvfs;
	struct fs	*fs;
	ulong_t		vsa_mask = vsap->vsa_mask;
	int		err;
	int		trans_size;

	TRACE_3(TR_FAC_UFS, TR_UFS_SETSECATTR_START,
	    "ufs_setsecattr_start:vp %p, vsap %p, flags %x", vp, vsap, flag);

	/*
	 * check for forced unmount
	 */
	if (ufsvfsp == NULL)
		return (EIO);
	fs = ufsvfsp->vfs_fs;

	if (fs->fs_ronly != 0)
		return (EROFS);

	if (VTOI(vp)->i_ufsvfs->vfs_nosetsec)
		return (ENOSYS);

	if (err = ufs_lockfs_begin(ufsvfsp, &ulp,  ULOCKFS_SETATTR_MASK))
		return (err);
	if (ulp)
		TRANS_BEGIN_ASYNC(ufsvfsp, TOP_SETSECATTR,
		    trans_size = TOP_SETSECATTR_SIZE(VTOI(vp)));

	vsa_mask &= (VSA_ACL | VSA_ACLCNT | VSA_DFACL | VSA_DFACLCNT);

	rw_enter(&ip->i_contents, RW_WRITER);
	if (vsa_mask != 0) {
		err = ufs_acl_set(ip, vsap, flag, cr);
	}
out:
	rw_exit(&ip->i_contents);
	if (ulp) {
		TRANS_END_ASYNC(ufsvfsp, TOP_SETSECATTR, trans_size);
		ufs_lockfs_end(ulp);
	}
	TRACE_1(TR_FAC_UFS, TR_UFS_SETSECATTR_END,
	    "ufs_setsecattr_end:vp %p", vp);
	return (err);
}

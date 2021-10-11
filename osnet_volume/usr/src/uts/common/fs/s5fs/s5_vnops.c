/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
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
 * 	Copyright (c) 1986-1990, 1993, 1996-1997 by Sun Microsystems, Inc.
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma ident	"@(#)s5_vnops.c	1.33	99/06/01 SMI"
/* from ufs_vnops.c 2.176	92/12/02 SMI */

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
#include <sys/vmsystm.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>

#include <sys/fs/s5_fs.h>
#include <sys/fs/s5_inode.h>
#include <sys/fs/s5_fsdir.h>
#include <sys/dirent.h>		/* must be AFTER <sys/fs/fsdir.h>! */
#include <sys/errno.h>

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

#include "fs/fs_subr.h"

#define	ISVDEV(t) \
	((t) == VCHR || (t) == VBLK || (t) == VFIFO)

extern	int convoff();
extern	int chklock();
extern	int kzero();

static int rwip(struct inode *ip, struct uio *uio,
    enum uio_rw rw, int ioflag, struct cred *cr);
int s5_rdwri(enum uio_rw rw, struct inode *ip, caddr_t base, int len,
    offset_t offset, enum uio_seg seg, int *aresid, struct cred *cr);
static int s5_page_fill(struct inode *ip, page_t *pp, uint_t off,
    uint_t bflgs, uint_t *pg_off, struct seg *seg);
static int s5_iodone(struct buf *bp);

static int s5_getpage_ra(struct vnode *vp, uint_t off, struct seg *seg,
    caddr_t addr);
static int s5_getpage_miss(struct vnode *vp, uint_t off, size_t len,
    struct seg *seg, caddr_t addr, page_t *pl[], size_t plsz,
    enum seg_rw rw, int seq);
void s5_setra(int ra);
static int s5_open(struct vnode **vpp, int flag, struct cred *cr);
static int s5_close(struct vnode *vp, int flag, int count, offset_t offset,
    struct cred *cr);
static int s5_read(struct vnode *vp, struct uio *uiop, int ioflag,
    struct cred *cr);
static int s5_write(struct vnode *vp, struct uio *uiop, int ioflag,
    struct cred *cr);
static int s5_getattr(struct vnode *vp, struct vattr *vap, int flags,
    struct cred *cr);
static int s5_setattr(struct vnode *vp, struct vattr *vap,
    int flags, struct cred *cr);
static int s5_access(struct vnode *vp, int mode, int flags, struct cred *cr);
static int s5_lookup(struct vnode *dvp, char *nm, struct vnode **vpp,
    struct pathname *pnp, int flags, struct vnode *rdir, struct cred *cr);
static int s5_create(struct vnode *dvp, char *name, struct vattr *vap,
    enum vcexcl excl, int mode, struct vnode **vpp,
    struct cred *cr, int);
static int s5_remove(struct vnode *vp, char *nm, struct cred *cr);
static int s5_link(struct vnode *tdvp, struct vnode *svp,
    char *tnm, struct cred *cr);
static int s5_rename(struct vnode *sdvp, char *snm, struct vnode *tdvp,
    char *tnm, struct cred *cr);
static int s5_mkdir(struct vnode *dvp, char *dirname, struct vattr *vap,
    struct vnode **vpp, struct cred *cr);
static int s5_rmdir(struct vnode *vp, char *nm, struct vnode *cdir,
    struct cred *cr);
static int s5_readdir(struct vnode *vp, struct uio *uiop, struct cred *cr,
    int *eofp);
static int s5_symlink(struct vnode *dvp, char *linkname,
    struct vattr *vap, char *target, struct cred *cr);
static int s5_readlink(struct vnode *vp, struct uio *uiop, struct cred *cr);
static int s5_fsync(struct vnode *vp, int syncflag, struct cred *cr);
static void s5_inactive(struct vnode *vp, struct cred *cr);
static void s5_rwlock(struct vnode *vp, int write_lock);
static void s5_rwunlock(struct vnode *vp, int write_lock);
static int s5_seek(struct vnode *vp, offset_t ooff, offset_t *noffp);
static int s5_frlock(struct vnode *vp, int cmd, struct flock64 *bfp,
    int flag, offset_t offset, cred_t *cr);
static int s5_space(struct vnode *vp, int cmd, struct flock64 *bfp, int flag,
    offset_t offset, struct cred *cr);
static int s5_getpage(struct vnode *vp, offset_t off, size_t len, uint_t *protp,
    page_t *plarr[], size_t plsz, struct seg *seg, caddr_t addr,
    enum seg_rw rw, struct cred *cr);
static int s5_putpage(struct vnode *vp, offset_t off, size_t len,
    int flags, struct cred *cr);
int s5_putapage(struct vnode *vp, page_t *pp, u_offset_t *offp, size_t *lenp,
    int flags, struct cred *cr);
static int s5_putpages(struct vnode *vp, offset_t off, size_t len,
    int flags, struct cred *cr);
static struct buf *s5_startio(struct inode *ip, daddr_t bn,
    page_t *pp, size_t len, uint_t pgoff, int flags, int io);
static int s5_map(struct vnode *vp, offset_t off, struct as *as,
    caddr_t *addrp, size_t len, uchar_t prot, uchar_t maxprot, uint_t flags,
    struct cred *cr);
static int s5_addmap(struct vnode *vp, offset_t off, struct as *as,
    caddr_t addr, size_t len, uchar_t prot, uchar_t maxprot, uint_t flags,
    struct cred *cr);
static int s5_delmap(struct vnode *vp, offset_t off, struct as *as,
    caddr_t addr, size_t len, uint_t prot, uint_t maxprot, uint_t flags,
    struct cred *cr);

static int s5_l_pathconf(struct vnode *vp, int cmd, ulong_t *valp,
    struct cred *cr);

struct vnodeops s5_vnodeops = {
	s5_open,
	s5_close,
	s5_read,
	s5_write,
	fs_nosys,	/* ioctl */
	fs_setfl,
	s5_getattr,
	s5_setattr,
	s5_access,
	s5_lookup,
	s5_create,
	s5_remove,
	s5_link,
	s5_rename,
	s5_mkdir,
	s5_rmdir,
	s5_readdir,
	s5_symlink,
	s5_readlink,
	s5_fsync,
	s5_inactive,
	fs_nosys,	/* fid */
	s5_rwlock,
	s5_rwunlock,
	s5_seek,
	fs_cmp,
	s5_frlock,
	s5_space,
	fs_nosys,	/* realvp */
	s5_getpage,
	s5_putpage,
	s5_map,
	s5_addmap,
	s5_delmap,
	fs_poll,
	fs_nosys,	/* dump */
	s5_l_pathconf,
	fs_nosys,	/* pageio */
	fs_nosys,	/* dumpctl */
	fs_dispose,
	fs_nosys,
	fs_fab_acl,
	fs_shrlock	/* shrlock */
};

/*
 * No special action required for ordinary files.  (Devices are handled
 * through the device file system.)
 */
/* ARGSUSED */
static int
s5_open(struct vnode **vpp, int flag, struct cred *cr)
{
	TRACE_1(TR_FAC_S5, TR_S5_OPEN, "s5_open:vpp %p", vpp);
	return (0);
}

/*ARGSUSED*/
static int
s5_close(struct vnode *vp, int flag, int count, offset_t offset,
    struct cred *cr)
{
	struct inode *ip = VTOI(vp);

	TRACE_1(TR_FAC_S5, TR_S5_CLOSE, "s5_close:vp %p", vp);

	ITIMES(ip);

	cleanlocks(vp, ttoproc(curthread)->p_pid, 0);
	cleanshares(vp, ttoproc(curthread)->p_pid);

	return (0);
}

/*ARGSUSED*/
static int
s5_read(struct vnode *vp, struct uio *uiop, int ioflag, struct cred *cr)
{
	struct inode *ip = VTOI(vp);
	int error;
	struct ulockfs *ulp;

	ASSERT(RW_READ_HELD(&ip->i_rwlock));
	TRACE_3(TR_FAC_S5, TR_S5_READ_START,
		"s5_read_start:vp %p uiop %p ioflag %x",
		vp, uiop, ioflag);
	if (error = s5_lockfs_vp_begin(vp, &ulp))
		goto out;

	rw_enter(&ip->i_contents, RW_READER);
	error = rwip(ip, uiop, UIO_READ, ioflag, cr);
	ITIMES(ip);
	rw_exit(&ip->i_contents);

	s5_lockfs_end(ulp);
out:

	TRACE_2(TR_FAC_S5, TR_S5_READ_END,
		"s5_read_end:vp %p error %d", vp, error);
	return (error);
}

int	s5_WRITES = 1;		/* XXX - enable/disable */
int	s5_HW = 384 * 1024;	/* high water mark */
int	s5_LW = 256 * 1024;	/* low water mark */
int	s5_throttles = 0;	/* throttling count */

/*ARGSUSED*/
static int
s5_write(struct vnode *vp, struct uio *uiop, int ioflag, struct cred *cr)
{
	int error;
	struct inode *ip;
	struct ulockfs *ulp;

	ip = VTOI(vp);
	TRACE_3(TR_FAC_S5, TR_S5_WRITE_START,
		"s5_write_start:vp %p uiop %p ioflag %x",
		vp, uiop, ioflag);
	ASSERT(RW_WRITE_HELD(&ip->i_rwlock));

	if (error = s5_lockfs_vp_begin(vp, &ulp))
		goto out;

	/*
	 * Throttle writes.
	 *
	 * XXX - this should be a resource limit, per process.
	 */

	if (s5_WRITES && (ip->i_writes > s5_HW)) {
		mutex_enter(&ip->i_tlock);
		while (ip->i_writes > s5_HW) {
			s5_throttles++;
			cv_wait(&ip->i_wrcv, &ip->i_tlock);
		}
		mutex_exit(&ip->i_tlock);
	}

	rw_enter(&ip->i_contents, RW_WRITER);

	if (vp->v_type == VREG && (error = fs_vcode(vp, &ip->i_vcode)))
		goto unlock;

	if ((ioflag & FAPPEND) != 0 && (ip->i_mode & IFMT) == IFREG) {
		/*
		 * In append mode start at end of file.
		 * assign to uio_loffset so that the upper 32 bits can be
		 * cleared to 0.
		 */
		uiop->uio_loffset = ip->i_size;
	}
	error = rwip(ip, uiop, UIO_WRITE, ioflag, cr);
	ITIMES(ip);

unlock:
	rw_exit(&ip->i_contents);
	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_WRITE_END,
		"s5_write_end:vp %p error %d", vp, error);
	return (error);
}

/*
 * Don't cache write blocks to files with the sticky bit set.
 * Used to keep swap files from blowing the page cache on a server.
 */
int s5fs_stickyhack = 1;

/*
 * Free behind hacks.  The pager is busted.
 * XXX - need to pass the information down to writedone() in a flag like B_SEQ
 * or B_FREE_IF_TIGHT_ON_MEMORY.
 */
int	s5fs_freebehind = 1;
int	s5fs_smallfile = 32 * 1024;

/*
 * rwip does the real work of read or write requests for S5.
 */
static int
rwip(struct inode *ip, struct uio *uio,
    enum uio_rw rw, int ioflag, struct cred *cr)
{
	uint_t off;
	caddr_t base;
	int n, on, mapon;
	struct vnode *vp;
	int type, error, pagecreate;
	int newpage;
	rlim_t limit = uio->uio_limit;
	uint_t flags;
	int iupdat_flag;
	long old_blocks;
	long oresid = uio->uio_resid;
	int dofree;
	int bsize = ip->i_s5vfs->vfs_bsize;

	/*
	 * ip->i_size is incremented before the uiomove
	 * is done on a write.  If the move fails (bad user
	 * address) reset ip->i_size.
	 * The better way would be to increment ip->i_size
	 * only if the uiomove succeeds.
	 */
	int i_size_changed = 0;
	int old_i_size;

	ASSERT(rw == UIO_READ || rw == UIO_WRITE);

	vp = ITOV(ip);

	TRACE_1(TR_FAC_S5, TR_S5_RWIP_START,
		"s5_rwip_start:vp %p", vp);

	ASSERT(RW_LOCK_HELD(&ip->i_contents));

	if (MANDLOCK(vp, ip->i_mode)) {
		rw_exit(&ip->i_contents);
		/*
		 * s5_getattr ends up being called by chklock
		 */
		error = chklock(vp, rw == UIO_READ ? FREAD : FWRITE,
			uio->uio_loffset, uio->uio_resid,
			uio->uio_fmode);
		if (rw == UIO_READ)
			rw_enter(&ip->i_contents, RW_READER);
		else
			rw_enter(&ip->i_contents, RW_WRITER);
		if (error != 0) {
			TRACE_2(TR_FAC_S5, TR_S5_RWIP_END,
				"s5_rwip_end:vp %p error %d", vp, error);
			return (error);
		}
	}
	type = ip->i_mode & IFMT;
	ASSERT(type == IFREG || type == IFDIR || type == IFLNK);
	vp = ITOV(ip);

	if (uio->uio_loffset > MAXOFF_T) {
		cmn_err(CE_PANIC, "rwip: bad uio_loffset\n");
	}

	if (uio->uio_offset < 0 || (uio->uio_offset + uio->uio_resid) < 0) {
		TRACE_2(TR_FAC_S5, TR_S5_RWIP_END,
			"s5_rwip_end:vp %p error %d", vp, EINVAL);
		return (EINVAL);
	}
	if (uio->uio_resid == 0) {
		TRACE_2(TR_FAC_S5, TR_S5_RWIP_END,
			"s5_rwip_end:vp %p error %d", vp, 0);
		return (0);
	}

	mutex_enter(&ip->i_tlock);
	if (rw == UIO_WRITE) {
		ip->i_flag |= INOACC;	/* don't update ref time in getpage */
	} else {
		ip->i_flag |= IACC;
	}

	if (ioflag & (FSYNC|FDSYNC)) {
		ip->i_flag |= ISYNC;
		old_blocks = ip->i_blocks;
		iupdat_flag = 0;
	}
	mutex_exit(&ip->i_tlock);

	vp = ITOV(ip);
	do {
		off = uio->uio_offset & MAXBMASK;
		mapon = uio->uio_offset & MAXBOFFSET;
		on = blkoff(ip->i_s5vfs, uio->uio_offset);
		n = MIN(bsize - on, uio->uio_resid);

		if (rw == UIO_READ) {
			int diff = ip->i_size - uio->uio_offset;

			if (diff <= 0) {
				error = 0;
				goto out;
			}
			if (diff < n)
				n = diff;
			dofree = s5fs_freebehind &&
			    ip->i_nextr == (off & PAGEMASK) &&
			    off > s5fs_smallfile;
		}

		if (rw == UIO_WRITE) {
			if (type == IFREG && uio->uio_offset + n >= limit) {
				if (uio->uio_offset >= limit) {
					psignal(ttoproc(curthread), SIGXFSZ);
					error = EFBIG;
					goto out;
				}
				n = limit - uio->uio_offset;
			}
			if (uio->uio_offset + n > ip->i_size) {
				/*
				 * We are extending the length of the file.
				 * bmap is used so that we are sure that
				 * if we need to allocate new blocks, that it
				 * is done here before we up the file size.
				 */
				error = s5_bmap_write(ip, uio->uio_offset,
				    (int)(on + n), mapon == 0, cr);
				if (error) {
#ifdef XXX
	/* XXX jsz: This is in ufs code, but seems useless! */
					old_i_size = ip->i_size;
					ip->i_size = uio->uio_offset + n;
					ip->i_size = old_i_size;
#endif
					break;
				}
				i_size_changed = 1;
				old_i_size = ip->i_size;
				ip->i_size = uio->uio_offset + n;
				iupdat_flag = 1;
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
				error = s5_bmap_write(ip, uio->uio_offset,
				    (int)(on + n), 1, cr);
				if (error)
					break;
				pagecreate = 1;
			} else
				pagecreate = 0;
		} else
			pagecreate = 0;

		/*
		 * We have to drop the contents lock to prevent the VM
		 * system from trying to reaquire it in s5_getpage()
		 * should the uiomove cause a pagefault.
		 */
		rw_exit(&ip->i_contents);

		base = segmap_getmapflt(segkmap, vp,
		    (offset_t)off + mapon, (size_t)n,
		    !pagecreate, rw == UIO_READ ? S_READ : S_WRITE);

		/*
		 * segmap_pagecreate() returns 1 if it calls
		 * page_create_va() to allocate any pages.
		 */
		newpage = 0;
		if (pagecreate)
			newpage = segmap_pagecreate(segkmap, base,
			    (size_t)n, 0);

		error = uiomove(base + mapon, (long)n, rw, uio);

		if (pagecreate &&
		    uio->uio_offset < roundup(off + mapon + n, PAGESIZE)) {
			/*
			 * We created pages w/o initializing them completely,
			 * thus we need to zero the part that wasn't set up.
			 * This happens on most EOF write cases and if
			 * we had some sort of error during the uiomove.
			 */
			long nzero, nmoved;

			nmoved = uio->uio_offset - (off + mapon);
			ASSERT(nmoved >= 0 && nmoved <= n);
			nzero = roundup(on + n, PAGESIZE) - nmoved;
			ASSERT(nzero > 0 && mapon + nmoved + nzero <= MAXBSIZE);
			(void) kzero(base + mapon + nmoved, (size_t)nzero);
		}

		/*
		 * Unlock the pages which have been allocated by
		 * page_create_va() in segmap_pagecreate().
		 */
		if (newpage)
			segmap_pageunlock(segkmap, base, (size_t)n,
				rw == UIO_READ ? S_READ : S_WRITE);

		if (error) {
			/*
			 * If we failed on a write, we may have already
			 * allocated file blocks as well as pages.  It's
			 * hard to undo the block allocation, but we must
			 * be sure to invalidate any pages that may have
			 * been allocated.
			 */
			if (rw == UIO_WRITE)
				(void) segmap_release(segkmap, base, SM_INVAL);
			else
				(void) segmap_release(segkmap, base, 0);
		} else {
			flags = 0;
			if (rw == UIO_WRITE) {
				/*
				 * Force write back for synchronous write cases.
				 */
				if ((ioflag & (FSYNC|FDSYNC)) ||
					type == IFDIR) {
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
					} else {
						iupdat_flag = 1;
						flags = SM_WRITE;
					}
				} else if (n + on == MAXBSIZE ||
				    IS_SWAPVP(vp)) {
					/*
					 * Have written a whole block.
					 * Start an asynchronous write and
					 * mark the buffer to indicate that
					 * it won't be needed again soon.
					 */
					flags = SM_WRITE | SM_ASYNC |
					    SM_DONTNEED;
				}
			} else if (rw == UIO_READ) {
				/*
				 * If read a whole block, or read to eof,
				 * won't need this buffer again soon.
				 */
				if (n + on == MAXBSIZE &&
				    s5fs_freebehind && dofree &&
				    freemem < lotsfree + pages_before_pager) {
					flags = SM_FREE | SM_DONTNEED |SM_ASYNC;
				}
			}
			error = segmap_release(segkmap, base, flags);
		}

		/*
		 * Re-acquire contents lock.
		 */
		if (rw == UIO_WRITE) {
			rw_enter(&ip->i_contents, RW_WRITER);
			/*
			 * If the uiomove failed, fix up i_size.
			 */
			if (error) {
				if (i_size_changed)
					ip->i_size = old_i_size;
			} else {
				/*
				 * XXX - Can this be out of the loop?
				 */
				mutex_enter(&ip->i_tlock);
				ip->i_flag |= IUPD | ICHG;
				mutex_exit(&ip->i_tlock);
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
		} else
			rw_enter(&ip->i_contents, RW_READER);

	} while (error == 0 && uio->uio_resid > 0 && n != 0);

	/*
	 * If we are doing synchronous write the only time we should
	 * not be sync'ing the ip here is if we have the stickyhack
	 * activated, the file is marked with the sticky bit and
	 * no exec bit, the file length has not been changed and
	 * no new blocks have been allocated during this write.
	 */
	if ((ip->i_flag & ISYNC) != 0 && !((rw == UIO_READ) &&
	    ((ioflag & (FSYNC|FDSYNC)) == FSYNC)) &&
	    (iupdat_flag != 0 || old_blocks != ip->i_blocks)) {
		s5_iupdat(ip, 1);
	}

out:
	/*
	 * If we've already done a partial-write, terminate
	 * the write but return no error.
	 */
	if (oresid != uio->uio_resid)
		error = 0;

	mutex_enter(&ip->i_tlock);
	ip->i_flag &= ~(INOACC | ISYNC);
	mutex_exit(&ip->i_tlock);
	TRACE_2(TR_FAC_S5, TR_S5_RWIP_END,
		"s5_rwip_end:vp %p error %d", vp, error);
	return (error);
}

/* ARGSUSED */
static int
s5_getattr(struct vnode *vp, struct vattr *vap, int flags,
    struct cred *cr)
{
	struct inode *ip = VTOI(vp);
	struct ulockfs *ulp;
	int err;

	TRACE_2(TR_FAC_S5, TR_S5_GETATTR_START,
		"s5_getattr_start:vp %p flags %x", vp, flags);

	if (err = s5_lockfs_vp_begin(vp, &ulp))
		goto out;

	if (vap->va_mask == AT_SIZE) {
		/*
		 * for performance, if only the size is requested don't bother
		 * with anything else.
		 */
		TRACE_1(TR_FAC_S5, TR_S5_GETATTR_END,
			"s5_getattr_end:vp %p", vp);
		vap->va_size = (offset_t)ip->i_size;
		err = 0;
		goto unlock;
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
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	vap->va_fsid = ip->i_dev;
	vap->va_nodeid = ip->i_number;
	vap->va_nlink = ip->i_nlink;
	vap->va_size = (offset_t)ip->i_size;
	vap->va_vcode = ip->i_vcode;
	if (vp->v_type == VCHR || vp->v_type == VBLK)
		vap->va_rdev = ip->i_rdev;
	else
		vap->va_rdev = 0;	/* not a b/c spec. */
	mutex_enter(&ip->i_tlock);
	ITIMES_NOLOCK(ip);	/* mark correct time in inode */
	vap->va_atime.tv_sec = ip->i_atime;
	vap->va_atime.tv_nsec = 0;
	vap->va_mtime.tv_sec = ip->i_mtime;
	vap->va_mtime.tv_nsec = 0;
	vap->va_ctime.tv_sec = ip->i_ctime;
	vap->va_ctime.tv_nsec = 0;
	mutex_exit(&ip->i_tlock);

	switch (ip->i_mode & IFMT) {

	case IFBLK:
		vap->va_blksize = MAXBSIZE;		/* was BLKDEV_IOSIZE */
		break;

	case IFCHR:
		vap->va_blksize = MAXBSIZE;		/* was BLKDEV_IOSIZE */
		break;
	default:
		vap->va_blksize = ip->i_s5vfs->vfs_bsize;
		break;
	}

	/*
	 * Below is an estimate assuming no holes and just counting
	 * data blocks, not indirect blocks.  S5 does not maintain
	 * i_blocks on disk, and traipsing through all the indirect
	 * blocks isn't worth the trouble.
	 */
	vap->va_nblocks = btodb(ip->i_size + ip->i_s5vfs->vfs_bmask);
	rw_exit(&ip->i_contents);
	err = 0;

unlock:
	s5_lockfs_end(ulp);
out:
	TRACE_1(TR_FAC_S5, TR_S5_GETATTR_END, "s5_getattr_end:vp %p", vp);

	return (err);
}

static int
s5_setattr(struct vnode *vp, struct vattr *vap,
    int flags, struct cred *cr)
{
	int error = 0;
	long int mask = vap->va_mask;
	struct inode *ip;
	struct ulockfs *ulp;

	TRACE_2(TR_FAC_S5, TR_S5_SETATTR_START,
		"s5_setattr_start:vp %p flags %x", vp, flags);

	if (error = s5_lockfs_vp_begin(vp, &ulp))
		goto out;

	/*
	 * Cannot set these attributes.
	 */
	if (mask & AT_NOSET) {
		error = EINVAL;
		goto unlock;
	}

	ip = VTOI(vp);

	rw_enter(&ip->i_rwlock, RW_WRITER);
	rw_enter(&ip->i_contents, RW_WRITER);

	/*
	 * Change file access modes.  Must be owner or super-user.
	 */
	if (mask & AT_MODE) {
		if (cr->cr_uid != ip->i_uid && !suser(cr)) {
			error = EPERM;
			goto update_inode;
		}
		ip->i_mode &= IFMT;
		ip->i_mode |= vap->va_mode & ~IFMT;
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
		mutex_enter(&ip->i_tlock);
		ip->i_flag |= ICHG;
		mutex_exit(&ip->i_tlock);
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

		if (cr->cr_uid != 0)
			ip->i_mode &= ~(ISUID|ISGID);
		if (mask & AT_UID) {
			/*
			 * Only 16 bits of uid fit in an on-disk inode
			 */
			if ((ulong_t)vap->va_uid > (ulong_t)USHRT_MAX) {
				error = EOVERFLOW;
				goto update_inode;
			}
			ip->i_uid = vap->va_uid;
		}
		if (mask & AT_GID) {
			/*
			 * Only 16 bits of gid fit in an on-disk inode
			 */
			if ((ulong_t)vap->va_gid > (ulong_t)USHRT_MAX) {
				error = EOVERFLOW;
				goto update_inode;
			}
			ip->i_gid = vap->va_gid;
		}
		mutex_enter(&ip->i_tlock);
		ip->i_flag |= ICHG;
		mutex_exit(&ip->i_tlock);
	}
	/*
	 * Truncate file.  Must have write permission and not be a directory.
	 */
	if (mask & AT_SIZE) {
		if (vp->v_type == VDIR) {
			error = EISDIR;
			goto update_inode;
		}
		if (error = s5_iaccess(ip, IWRITE, cr))
			goto update_inode;
		if (vp->v_type == VREG &&
		    (error = fs_vcode(vp, &ip->i_vcode)))
			goto update_inode;
		if (vap->va_size > MAXOFF_T) {
			error = EFBIG;
			goto update_inode;
		}
		if (error = s5_itrunc(ip, (ulong_t)vap->va_size, cr))
			goto update_inode;
	}
	/*
	 * Change file access or modified times.
	 */
	if (mask & (AT_ATIME|AT_MTIME)) {
		if (cr->cr_uid != ip->i_uid && cr->cr_uid != 0) {
			if (flags & ATTR_UTIME)
				error = EPERM;
			else
				error = s5_iaccess(ip, IWRITE, cr);
			if (error)
				goto update_inode;
		}
		mutex_enter(&ip->i_tlock);
		if (mask & AT_ATIME) {
			ip->i_atime = vap->va_atime.tv_sec;
			ip->i_flag &= ~IACC;
		}
		if (mask & AT_MTIME) {
			ip->i_mtime = vap->va_mtime.tv_sec;
			ip->i_ctime = hrestime.tv_sec;
			ip->i_flag &= ~(IUPD|ICHG);
			ip->i_flag |= IMODTIME;
		}
		ip->i_flag |= IMOD;
		mutex_exit(&ip->i_tlock);
	}

update_inode:
	s5_iupdat(ip, 1); /* XXX - should be async for performance */
	rw_exit(&ip->i_contents);
	rw_exit(&ip->i_rwlock);

unlock:
	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_SETATTR_END,
		"s5_setattr_end:vp %p error %d", vp, error);
	return (error);
}

/*ARGSUSED*/
static int
s5_access(struct vnode *vp, int mode, int flags, struct cred *cr)
{
	struct inode *ip = VTOI(vp);
	int error;

	TRACE_3(TR_FAC_S5, TR_S5_ACCESS_START,
		"s5_access_start:vp %p mode %x flags %x", vp, mode, flags);

	if (ip->i_s5vfs == NULL)
		return (EIO);

	error = s5_iaccess(ip, mode, cr);

	TRACE_2(TR_FAC_S5, TR_S5_ACCESS_END,
		"s5_access_end:vp %p error %d", vp, error);
	return (error);
}

/* ARGSUSED */
static int
s5_readlink(struct vnode *vp, struct uio *uiop, struct cred *cr)
{
	struct inode *ip;
	int error;
	struct ulockfs *ulp;

	TRACE_2(TR_FAC_S5, TR_S5_READLINK_START,
		"s5_readlink_start:vp %p uiop %p", uiop, vp);

	if (error = s5_lockfs_vp_begin(vp, &ulp))
		goto out;

	if (vp->v_type != VLNK) {
		error = EINVAL;
		goto unlock;
	}
	ip = VTOI(vp);

	rw_enter(&ip->i_contents, RW_READER);
	(void) rwip(ip, uiop, UIO_READ, 0, cr);
	rw_exit(&ip->i_contents);

	ITIMES(ip);

unlock:
	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_READLINK_END,
		"s5_readlink_end:vp %p error %d", vp, error);

	return (error);
}

/* ARGSUSED */
static int
s5_fsync(struct vnode *vp, int syncflag, struct cred *cr)
{
	struct inode *ip = VTOI(vp);
	int error;
	struct ulockfs *ulp;

	TRACE_1(TR_FAC_S5, TR_S5_FSYNC_START,
		"s5_fsync_start:vp %p", vp);

	if (error = s5_lockfs_vp_begin(vp, &ulp))
		goto out;

	rw_enter(&ip->i_contents, RW_WRITER);
	if (!(IS_SWAPVP(vp)))
		error = s5_syncip(ip, 0, I_SYNC); /* Do synchronous writes */
	if (!error)
		error = s5_sync_indir(ip);
	ITIMES(ip);			/* XXX: is this necessary ??? */
	rw_exit(&ip->i_contents);

	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_FSYNC_END,
		"s5_fsync_end:vp %p error %d", vp, error);
	return (error);
}

/*ARGSUSED*/
static void
s5_inactive(struct vnode *vp, struct cred *cr)
{

	s5_iinactive(VTOI(vp), cr);
}

/*
 * Unix file system operations having to do with directory manipulation.
 */
/* ARGSUSED */
static int
s5_lookup(struct vnode *dvp, char *nm, struct vnode **vpp,
    struct pathname *pnp, int flags, struct vnode *rdir, struct cred *cr)
{
	struct inode *ip;
	struct inode *xip;
	int error;
	struct ulockfs *ulp;

	TRACE_2(TR_FAC_S5, TR_S5_LOOKUP_START,
		"s5_lookup_start:dvp %p name %s", dvp, nm);

	if (error = s5_lockfs_vp_begin(dvp, &ulp))
		goto out;

	/*
	 * Null component name is a synonym for directory being searched.
	 */
	if (*nm == '\0') {
		VN_HOLD(dvp);
		*vpp = dvp;
		error = 0;
		goto unlock;
	}

	ip = VTOI(dvp);
	error = s5_dirlook(ip, nm, &xip, cr);
	ITIMES(ip);
	if (error == 0) {
		ip = xip;
		*vpp = ITOV(ip);
		if ((ip->i_mode & (ISVTX | IEXEC | IFDIR)) == ISVTX &&
		    s5fs_stickyhack) {
			mutex_enter(&(*vpp)->v_lock);
			(*vpp)->v_flag |= VISSWAP;
			mutex_exit(&(*vpp)->v_lock);
		}
		ITIMES(ip);
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

unlock:
	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_LOOKUP_END,
		"s5_lookup_end:dvp %p error %d", dvp, error);
	return (error);
}

/*ARGSUSED7*/
static int
s5_create(struct vnode *dvp, char *name, struct vattr *vap,
    enum vcexcl excl, int mode, struct vnode **vpp,
    struct cred *cr, int flag)
{
	int error;
	struct inode *ip = VTOI(dvp);
	struct inode *xip;
	struct ulockfs *ulp;

	TRACE_1(TR_FAC_S5, TR_S5_CREATE_START,
		"s5_create_start:dvp %p", dvp);

	if (error = s5_lockfs_vp_begin(dvp, &ulp))
		goto out;

	/* must be super-user to set sticky bit */
	if (cr->cr_uid != 0)
		vap->va_mode &= ~VSVTX;

	if (*name == '\0') {
		/*
		 * Null component name refers to the directory itself.
		 */
		VN_HOLD(dvp);
		ITIMES(ip);
		rw_enter(&ip->i_contents, RW_WRITER);
		error = EEXIST;
	} else {
		xip = NULL;
		rw_enter(&ip->i_rwlock, RW_WRITER);
		error = s5_direnter(ip, name, DE_CREATE, (struct inode *)0,
		    (struct inode *)0, vap, &xip, cr);
		rw_exit(&ip->i_rwlock);
		ITIMES(ip);
		ip = xip;
		if (ip != NULL)
			rw_enter(&ip->i_contents, RW_WRITER);
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
				error = s5_iaccess(ip, mode, cr);
			else
				error = 0;
		}
		if (error) {
			rw_exit(&ip->i_contents);
			s5_iput(ip);
			goto unlock;
		} else if (((ip->i_mode & IFMT) == IFREG) &&
		    (vap->va_mask & AT_SIZE) && vap->va_size == (offset_t)0) {
			/*
			 * Truncate regular files, if requested by caller.
			 * Grab i_rwlock to make sure no one else is
			 * currently writing to the file (we promised
			 * bmap we would do this).
			 * Must get the locks in the correct order.
			 */
			rw_exit(&ip->i_contents);
			rw_enter(&ip->i_rwlock, RW_WRITER);
			rw_enter(&ip->i_contents, RW_WRITER);
			(void) s5_itrunc(ip, (ulong_t)0, cr);
			rw_exit(&ip->i_rwlock);
		}
	}
	if (error) {
		if (ip != NULL)
			rw_exit(&ip->i_contents);
		goto unlock;
	}
	*vpp = ITOV(ip);
	ITIMES(ip);
	if (((ip->i_mode & IFMT) == IFREG) && (vap->va_mask & AT_SIZE))
		error = fs_vcode(ITOV(ip), &ip->i_vcode);
	rw_exit(&ip->i_contents);
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
		*vpp = newvp;
	}

unlock:
	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_CREATE_END,
		"s5_create_end:dvp %p error %d", dvp, error);
	return (error);
}

/*ARGSUSED*/
static int
s5_remove(struct vnode *vp, char *nm, struct cred *cr)
{
	struct inode *ip = VTOI(vp);
	int error;
	struct ulockfs *ulp;

	TRACE_1(TR_FAC_S5, TR_S5_REMOVE_START,
		"s5_remove_start:vp %p", vp);

	if (error = s5_lockfs_vp_begin(vp, &ulp))
		goto out;

	rw_enter(&ip->i_rwlock, RW_WRITER);
	error = s5_dirremove(ip, nm, (struct inode *)0, (struct vnode *)0,
	    DR_REMOVE, cr);
	rw_exit(&ip->i_rwlock);
	ITIMES(ip);

	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_REMOVE_END,
		"s5_remove_end:vp %p error %d", vp, error);
	return (error);
}

/*
 * Link a file or a directory.  Only the super-user is allowed to make a
 * link to a directory.
 */
static int
s5_link(struct vnode *tdvp, struct vnode *svp,
    char *tnm, struct cred *cr)
{
	struct inode *sip;
	struct inode *tdp;
	int error;
	struct vnode *realvp;
	struct ulockfs *ulp;

	TRACE_1(TR_FAC_S5, TR_S5_LINK_START,
		"s5_link_start:tdvp %p", tdvp);

	if (error = s5_lockfs_vp_begin(tdvp, &ulp))
		goto out;

	if (VOP_REALVP(svp, &realvp) == 0)
		svp = realvp;
	if (svp->v_type == VDIR && !suser(cr)) {
		error = EPERM;
		goto unlock;
	}
	sip = VTOI(svp);
	tdp = VTOI(tdvp);
	rw_enter(&tdp->i_rwlock, RW_WRITER);
	error = s5_direnter(tdp, tnm, DE_LINK, (struct inode *)0,
	    sip, (struct vattr *)0, (struct inode **)0, cr);
	rw_exit(&tdp->i_rwlock);
	ITIMES(sip);
	ITIMES(tdp);

unlock:
	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_LINK_END,
		"s5_link_end:tdvp %p error %d", tdvp, error);
	return (error);
}

kmutex_t	rename_lock;	/* Serialize all renames in S5 */

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
s5_rename(struct vnode *sdvp, char *snm, struct vnode *tdvp,
    char *tnm, struct cred *cr)
{
	struct inode *sip;		/* source inode */
	struct inode *sdp;	/* old (source) parent inode */
	struct inode *tdp;	/* new (target) parent inode */
	int error;
	struct vnode *realvp;
	struct ulockfs *ulp;

	TRACE_1(TR_FAC_S5, TR_S5_RENAME_START,
		"s5_rename_start:sdvp %p", sdvp);

	if (error = s5_lockfs_vp_begin(sdvp, &ulp))
		goto out;

	if (VOP_REALVP(tdvp, &realvp) == 0)
		tdvp = realvp;

	sdp = VTOI(sdvp);
	tdp = VTOI(tdvp);

	mutex_enter(&rename_lock);
	/*
	 * Look up inode of file we're supposed to rename.
	 */
	if (error = s5_dirlook(sdp, snm, &sip, cr)) {
		mutex_exit(&rename_lock);
		goto unlock;
	}
	/*
	 * be sure this is not a directory with another file system mounted
	 * over it.  If it is just give up the locks, and return with
	 * EBUSY
	*/
	if (ITOV(sip)->v_vfsmountedhere != NULL) {
		VN_RELE(ITOV(sip));
		mutex_exit(&rename_lock);
		error = EBUSY;
		goto unlock;
	}
	/*
	 * Make sure we can delete the source entry.  This requires
	 * write permission on the containing directory.  If that
	 * directory is "sticky" it further requires (except for the
	 * super-user) that the user own the directory or the source
	 * entry, or else have permission to write the source entry.
	 */
	rw_enter(&sdp->i_contents, RW_READER);
	rw_enter(&sip->i_contents, RW_READER);
	if ((error = s5_iaccess(sdp, IWRITE, cr)) > 0 ||
	    ((sdp->i_mode & ISVTX) && cr->cr_uid != 0 &&
	    cr->cr_uid != sdp->i_uid && cr->cr_uid != sip->i_uid &&
	    ((error = s5_iaccess(sip, IWRITE, cr)) > 0))) {
		rw_exit(&sip->i_contents);
		rw_exit(&sdp->i_contents);
		irele(sip);
		mutex_exit(&rename_lock);
		goto unlock;
	}

	/*
	 * Check for renaming '.' or '..' or alias of '.'
	 */
	if (strcmp(snm, ".") == 0 || strcmp(snm, "..") == 0 || sdp == sip) {
		error = EINVAL;
		rw_exit(&sip->i_contents);
		rw_exit(&sdp->i_contents);
		goto update_inode;
	}
	rw_exit(&sip->i_contents);
	rw_exit(&sdp->i_contents);

	/*
	 * Link source to the target.
	 */
	rw_enter(&tdp->i_rwlock, RW_WRITER);
	if (error = s5_direnter(tdp, tnm, DE_RENAME, sdp, sip,
	    (struct vattr *)0, (struct inode **)0, cr)) {
		/*
		 * ESAME isn't really an error; it indicates that the
		 * operation should not be done because the source and target
		 * are the same file, but that no error should be reported.
		 */
		if (error == ESAME)
			error = 0;
		rw_exit(&tdp->i_rwlock);
		goto update_inode;
	}
	rw_exit(&tdp->i_rwlock);

	rw_enter(&sdp->i_rwlock, RW_WRITER);
	/*
	 * Unlink the source.
	 * Remove the source entry.  s5_dirremove() checks that the entry
	 * still reflects sip, and returns an error if it doesn't.
	 * If the entry has changed just forget about it.  Release
	 * the source inode.
	 */
	if ((error = s5_dirremove(sdp, snm, sip, (struct vnode *)0,
	    DR_RENAME, cr)) == ENOENT)
		error = 0;
	rw_exit(&sdp->i_rwlock);
update_inode:
	ITIMES(sdp);
	ITIMES(tdp);
	irele(sip);
	mutex_exit(&rename_lock);

unlock:
	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_RENAME_END,
		"s5_rename_end:sdvp %p error %d", sdvp, error);
	return (error);
}

/*ARGSUSED*/
static int
s5_mkdir(struct vnode *dvp, char *dirname, struct vattr *vap,
    struct vnode **vpp, struct cred *cr)
{
	struct inode *ip;
	struct inode *xip;
	int error;
	struct ulockfs *ulp;

	ASSERT((vap->va_mask & (AT_TYPE|AT_MODE)) == (AT_TYPE|AT_MODE));

	TRACE_1(TR_FAC_S5, TR_S5_MKDIR_START,
		"s5_mkdir_start:dvp %p", dvp);

	if (error = s5_lockfs_vp_begin(dvp, &ulp))
		goto out;

	ip = VTOI(dvp);
	rw_enter(&ip->i_rwlock, RW_WRITER);
	error = s5_direnter(ip, dirname, DE_MKDIR, (struct inode *)0,
	    (struct inode *)0, vap, &xip, cr);
	rw_exit(&ip->i_rwlock);
	ITIMES(ip);
	if (error == 0) {
		ip = xip;
		*vpp = ITOV(ip);
		ITIMES(ip);
	} else if (error == EEXIST)
		s5_iput(xip);

	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_MKDIR_END,
		"s5_mkdir_end:dvp %p error %d", dvp, error);
	return (error);
}

/*ARGSUSED*/
static int
s5_rmdir(struct vnode *vp, char *nm, struct vnode *cdir,
    struct cred *cr)
{
	struct inode *ip = VTOI(vp);
	int error;
	struct ulockfs *ulp;

	TRACE_1(TR_FAC_S5, TR_S5_RMDIR_START,
		"s5_rmdir_start:vp %p", vp);

	if (error = s5_lockfs_vp_begin(vp, &ulp))
		goto out;

	rw_enter(&ip->i_rwlock, RW_WRITER);
	error = s5_dirremove(ip, nm, (struct inode *)0, cdir, DR_RMDIR, cr);
	rw_exit(&ip->i_rwlock);
	ITIMES(ip);

	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_RMDIR_END,
		"s5_rmdir_end:vp %p error %d", vp, error);

	return (error);
}

/* ARGSUSED */
static int
s5_readdir(struct vnode *vp, struct uio *uiop, struct cred *cr,
    int *eofp)
{
	struct iovec *iovp;
	struct inode *ip;
	struct direct *idp;
	struct dirent64 *odp;
	uint_t offset;
	int incount = 0;
	int outcount = 0;
	uint_t bytes_wanted, total_bytes_wanted;
	caddr_t outbuf;
	size_t bufsize;
	int error;
	struct fbuf *fbp;
	struct ulockfs *ulp;

	ip = VTOI(vp);
	ASSERT(RW_READ_HELD(&ip->i_rwlock));

	TRACE_2(TR_FAC_S5, TR_S5_READDIR_START,
		"s5_readdir_start:vp %p uiop %p", vp, uiop);

	if (uiop->uio_loffset >= MAXOFF_T) {
		if (eofp)
			*eofp = 1;
		return (0);
	}
	/*
	 * Large Files: When we come here we are guaranteed that
	 * uio_offset can be used safely. The high word is zero.
	 */

	if (error = s5_lockfs_vp_begin(vp, &ulp))
		goto out;

	iovp = uiop->uio_iov;
	total_bytes_wanted = iovp->iov_len;

	/* Large Files: directory files should not be "large" */

	ASSERT(ip->i_size <= MAXOFF_T);


	/* Force offset to be valid (to guard against bogus lseek() values) */
	offset = uiop->uio_offset & ~(SDSIZ - 1);

	/* Quit if at end of file */
	if (offset >= ip->i_size) {
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
	bufsize = total_bytes_wanted + sizeof (struct dirent64);
	outbuf = kmem_alloc(bufsize, KM_SLEEP);
	odp = (struct dirent64 *)outbuf;

nextblk:
	bytes_wanted = total_bytes_wanted;

	/* Truncate request to file size */
	if (offset + bytes_wanted > ip->i_size)
		bytes_wanted = ip->i_size - offset;

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
	mutex_enter(&ip->i_tlock);
	ip->i_flag |= IACC;
	mutex_exit(&ip->i_tlock);
	incount = 0;
	idp = (struct direct *)fbp->fb_addr;
	if (idp->d_ino == 0 && idp->d_name == '\0') {
		cmn_err(CE_WARN, "s5_readir: bad dir, inumber = %d\n",
			(int)ip->i_number);
		fbrelse(fbp, S_OTHER);
		error = ENXIO;
		goto update_inode;
	}


	/* Transform to file-system independent format */
	while (incount < bytes_wanted) {
		/* Skip to requested offset and skip empty entries */
		if (idp->d_ino != 0 && offset >= uiop->uio_offset) {
			int namelen = (idp->d_name[DIRSIZ-1] ?
			    DIRSIZ : strlen(idp->d_name));

			odp->d_ino = idp->d_ino;
			odp->d_reclen = DIRENT64_RECLEN(namelen);
			/* If this entry yields too many bytes, quit */
			if (outcount + odp->d_reclen > total_bytes_wanted)
				break;
			odp->d_off = offset + sizeof (struct direct);
			(void) strncpy(odp->d_name, idp->d_name, namelen);
			odp->d_name[namelen] = '\0';
			outcount += odp->d_reclen;
			odp = (struct dirent64 *)((int)odp + odp->d_reclen);
			ASSERT(outcount <= bufsize);
		}

		/* Advance to next entry */
		incount += sizeof (struct direct);
		offset += sizeof (struct direct);
		++idp;
	}

	/* Release the chunk */
	fbrelse(fbp, S_OTHER);

	/* Read whole block, but got no entries, read another if not eof */

	/*
	 * Large Files: casting i_size to int here is not a problem
	 * because directory sizes are always less than MAXOFF_T.
	 * See assertion above.
	 */

	if (offset < ip->i_size && !outcount)
		goto nextblk;

	/* Copy out the entry data */
	if ((error = uiomove(outbuf, (long)outcount, UIO_READ, uiop)) == 0)
		uiop->uio_offset = offset;
update_inode:
	ITIMES(ip);
	kmem_free(outbuf, bufsize);
	if (eofp && error == 0)
		*eofp = (uiop->uio_offset >= ip->i_size);

unlock:
	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_READDIR_END,
		"s5_readdir_end:vp %p error %d", vp, error);
	return (error);
}

/*ARGSUSED*/
static int
s5_symlink(struct vnode *dvp, char *linkname,
    struct vattr *vap, char *target, struct cred *cr)
{
	struct inode *ip, *dip = VTOI(dvp);
	int error;
	struct ulockfs *ulp;

	ip = (struct inode *)0;
	vap->va_type = VLNK;
	vap->va_rdev = 0;

	TRACE_1(TR_FAC_S5, TR_S5_SYMLINK_START,
		"s5_symlink_start:dvp %p", dvp);

	if (error = s5_lockfs_vp_begin(dvp, &ulp))
		goto out;

	rw_enter(&dip->i_rwlock, RW_WRITER);
	error = s5_direnter(dip, linkname, DE_CREATE,
	    (struct inode *)0, (struct inode *)0, vap, &ip, cr);
	rw_exit(&dip->i_rwlock);
	if (error == 0) {
		rw_enter(&ip->i_contents, RW_WRITER);
		error = s5_rdwri(UIO_WRITE, ip, target, (int)strlen(target),
		    (offset_t)0, UIO_SYSSPACE, (int *)0, cr);
		if (error) {
			rw_exit(&ip->i_contents);
			idrop(ip);
			rw_enter(&dip->i_rwlock, RW_WRITER);
			error = s5_dirremove(dip, linkname, (struct inode *)0,
			    (struct vnode *)0, DR_REMOVE, cr);
			rw_exit(&dip->i_rwlock);
			goto update_inode;
		}
		rw_exit(&ip->i_contents);
	}

	if (error == 0 || error == EEXIST)
		s5_iput(ip);
update_inode:
	ITIMES(VTOI(dvp));

	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_SYMLINK_END,
		"s5_symlink_end:dvp %p error %d", dvp, error);
	return (error);
}

/*
 * S5 specific routine used to do S5 io.
 */
int
s5_rdwri(enum uio_rw rw, struct inode *ip, caddr_t base, int len,
    offset_t offset, enum uio_seg seg, int *aresid, struct cred *cr)
{
	struct uio auio;
	struct iovec aiov;
	int error;

	aiov.iov_base = base;
	aiov.iov_len = len;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_loffset = offset;
	auio.uio_segflg = (short)seg;
	auio.uio_resid = len;

	error = rwip(ip, &auio, rw, 0, cr);
	if (rw == UIO_WRITE) {
		auio.uio_fmode = FWRITE;
	} else {
		auio.uio_fmode = FREAD;
	}

	if (aresid) {
		*aresid = auio.uio_resid;
	} else if (auio.uio_resid) {
		error = EIO;
	}
	return (error);
}

static void
s5_rwlock(struct vnode *vp, int write_lock)
{
	struct inode	*ip = VTOI(vp);

	if (write_lock)
		rw_enter(&ip->i_rwlock, RW_WRITER);
	else
		rw_enter(&ip->i_rwlock, RW_READER);
}

/*ARGSUSED*/
static void
s5_rwunlock(struct vnode *vp, int write_lock)
{
	struct inode	*ip = VTOI(vp);

	rw_exit(&ip->i_rwlock);
}

/* ARGSUSED */
static int
s5_seek(struct vnode *vp, offset_t ooff, offset_t *noffp)
{
	if (*noffp < 0)
		return (EINVAL);
	else if (*noffp > MAXOFF_T)
		return (EFBIG);
	else
		return (0);
}

/* ARGSUSED */
static int
s5_frlock(struct vnode *vp, int cmd, struct flock64 *bfp,
    int flag, offset_t offset, cred_t *cr)
{
	struct inode *ip = VTOI(vp);

	if (ip->i_s5vfs == NULL)
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

	if (offset > MAXOFF_T)
		return (EFBIG);

	if ((bfp->l_start > MAXOFF_T) || (bfp->l_end > MAXOFF_T) ||
	    (bfp->l_len > MAXOFF_T))
		return (EFBIG);

	return (fs_frlock(vp, cmd, bfp, flag, offset, cr));
}

/* ARGSUSED */
static int
s5_space(struct vnode *vp, int cmd, struct flock64 *bfp, int flag,
    offset_t offset, struct cred *cr)
{
	int error;
	struct ulockfs *ulp;

	if (error = s5_lockfs_vp_begin(vp, &ulp))
		return (error);

	if (cmd != F_FREESP)
		error =  EINVAL;
	else if (offset > MAXOFF_T)
		error = EFBIG;
	else if ((error = convoff(vp, bfp, 0, offset)) == 0)
		error = s5_freesp(vp, bfp, flag, cr);

	s5_lockfs_end(ulp);
	return (error);
}

static int s5_ra = 1;

void
s5_setra(int ra)
{
	s5_ra = ra;
}

/*
 * For read purposes, this has to be (bsize * maxcontig).
 * For write purposes, this can be larger.
 *
 * XXX - if you make it larger, change findextent() to match.
 */
#define	RD_CLUSTSZ(ip)		((ip)->i_s5vfs->vfs_rdclustsz)
#define	WR_CLUSTSZ(ip)		((ip)->i_s5vfs->vfs_wrclustsz)

/*
 * A faster version of s5_getpage.
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
s5_getpage(struct vnode *vp, offset_t off, size_t len, uint_t *protp,
    page_t *plarr[], size_t plsz, struct seg *seg, caddr_t addr,
    enum seg_rw rw, struct cred *cr)
{
	struct inode 	*ip = VTOI(vp);
	uint_t		uoff = (uint_t)off; /* type conversion */
	int 		err;
	page_t		**pl;
	int		beyond_eof;
	int		seqmode;
	uint_t		pgoff;
	uint_t		eoff;
	caddr_t		pgaddr;
	struct ulockfs *ulp;
	int		pgsize = PAGESIZE;
	krw_t		rwtype;

	ASSERT(off <= MAXOFF_T);
	TRACE_1(TR_FAC_S5, TR_S5_GETPAGE_START,
		"s5_getpage_start:vp %p", vp);
	ASSERT((uoff & PAGEOFFSET) == 0);

	if (protp)
		*protp = PROT_ALL;

	/*
	 * Obey the lockfs protocol. Simplified for s5.
	 */
	if (err = s5_lockfs_vp_begin(vp, &ulp))
		goto out;

	if (vp->v_flag & VNOMAP) {
		err = ENOSYS;
		goto unlock;
	}

	seqmode = s5_ra && ip->i_nextr == uoff && rw != S_CREATE;

	rwtype = RW_READER;		/* start as a reader */
retrylock:
	rw_enter(&ip->i_contents, rwtype);

	/*
	 * We may be getting called as a side effect of a bmap using
	 * fbread() when the blocks might be being allocated and the
	 * size has not yet been up'ed.  In this case we want to be
	 * able to return zero pages if we get back S5_HOLE from
	 * calling bmap for a non write case here.  We also might have
	 * to read some frags from the disk into a page if we are
	 * extending the number of frags for a given lbn in bmap().
	 */
	beyond_eof = uoff + len > ip->i_size + PAGEOFFSET;
	if (beyond_eof && seg != segkmap) {
		rw_exit(&ip->i_contents);
		err = EFAULT;
		goto unlock;
	}

	/*
	 * Must hold i_contents lock throughout the call to pvn_getpages
	 * since locked pages are returned from each call to s5_getapage.
	 * Must *not* return locked pages and then try for contents lock
	 * due to lock ordering requirements (inode > page)
	 */

	if (rw == S_WRITE || rw == S_CREATE) {
		int	blk_size;
		uint_t	offset;

		/*
		 * We must acquire the RW_WRITER lock in order to
		 * call s5_bmap_write().
		 */
		if (rwtype == RW_READER && !rw_tryupgrade(&ip->i_contents)) {
			rwtype = RW_WRITER;
			rw_exit(&ip->i_contents);
			goto retrylock;
		}

		/*
		 * May be allocating disk blocks for holes here as
		 * a result of mmap faults. write(2) does the s5_bmap_write
		 * in rwip, not here.
		 */

		offset = uoff;
		while (offset < uoff + len) {
			blk_size = (int)blksize(ip->i_s5vfs);
			err = s5_bmap_write(ip, offset, blk_size, 0, cr);
			if (err) {
				rw_exit(&ip->i_contents);
				goto update_inode;
			}
			offset += blk_size; /* XXX - make this contig */
		}
	}

	/*
	 * Can be a writer from now on.
	 */
	if (rwtype == RW_WRITER)
		rw_downgrade(&ip->i_contents);

	/*
	 * We remove PROT_WRITE
	 * because we don't  want to call s5_bmap_read() to check each
	 * page if it is backed with a disk block.
	 */
	if (protp && rw != S_WRITE && rw != S_CREATE)
		*protp &= ~PROT_WRITE;

	err = 0;

	/*
	 * The loop looks up pages in the range <off, off + len).
	 * For each page, we first check if we should initiate an asynchronous
	 * read ahead before we call page_lookup (we may sleep in page_lookup
	 * for a previously initiated disk read).
	 */
	eoff = uoff + len;
	for (pgoff = uoff, pgaddr = addr, pl = plarr;
	    pgoff < eoff; /* empty */) {
		page_t	*pp;
		uint_t	nextrio;
		se_t se = (rw == S_CREATE ? SE_EXCL : SE_SHARED);

		/* Handle async getpage (faultahead) */
		if (plarr == NULL) {
			ip->i_nextrio = pgoff;
			(void) s5_getpage_ra(vp, pgoff, seg, pgaddr);
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
		    nextrio < ip->i_size && page_exists(vp, (offset_t)pgoff))
			(void) s5_getpage_ra(vp, pgoff, seg, pgaddr);

		if ((pp = page_lookup(vp, (offset_t)pgoff, se)) != NULL) {
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
			if (err = s5_getpage_miss(vp, pgoff, len, seg, pgaddr,
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
	 * backed with a S5 hole and rw is S_WRITE or S_CREATE.
	 */
	if (plarr && !(rw == S_WRITE || rw == S_CREATE)) {

		ASSERT(!(*protp & PROT_WRITE));
		eoff = pgoff + plsz;
		while (pgoff < eoff) {
			page_t		*pp;

			if ((pp = page_lookup_nowait(vp, (offset_t)pgoff,
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

	rw_exit(&ip->i_contents);

update_inode:
	/*
	 * If the inode is not already marked for IACC (in rwip() for read)
	 * and the inode is not marked for no access time update (in rwip()
	 * for write) then update the inode access time and mod time now.
	 */
	mutex_enter(&ip->i_tlock);
	if ((ip->i_flag & (IACC | INOACC)) == 0) {
		if (rw != S_OTHER)
			ip->i_flag |= IACC;
		if (rw == S_WRITE)
			ip->i_flag |= IUPD;
		ITIMES_NOLOCK(ip);
	}
	mutex_exit(&ip->i_tlock);

unlock:
	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_GETPAGE_END,
		"s5_getpage_end:vp %p error %d", vp, err);
	return (err);
}

/*
 * s5_getpage_miss is called when s5_getpage missed the page in the page
 * cache. The page is either read from the disk, or it's created.
 * A page is created (without disk read) if rw == S_CREATE, or if
 * the page is not backed with a real disk block (S5 hole).
 */
static int
s5_getpage_miss(struct vnode *vp, uint_t off, size_t len, struct seg *seg,
    caddr_t addr, page_t *pl[], size_t plsz, enum seg_rw rw, int seq)
{
	struct inode	*ip = VTOI(vp);
	int		err = 0;
	ulong_t		io_len;
	u_offset_t	io_off;
	uint_t		pgoff;
	page_t		*pp;


#ifdef	lint
	len = len;
#endif	/* lint */

	pl[0] = NULL;
	/*
	 * Figure out whether the page can be created, or must be
	 * must be read from the disk.
	 */
	if (rw == S_CREATE) {
		if ((pp = page_create_va(vp, (offset_t)off, PAGESIZE, PG_WAIT,
						seg, addr)) == NULL)
			cmn_err(CE_PANIC, "s5_getpage_miss: page_create");

		io_len = PAGESIZE;
	} else {
		pp = pvn_read_kluster(vp, (offset_t)off, seg, addr, &io_off,
		&io_len, (offset_t)off, PAGESIZE, 0);
		/*
		 * Some other thread has entered the page.
		 * s5_getpage will retry page_lookup.
		 */
		if (pp == NULL) {
			return (0);
		}
		/*
		 * Fill the page with as much data as we can from the file.
		 */
		err = s5_page_fill(ip, pp, off, B_READ, &pgoff, seg);

		if (err) {
			pvn_read_done(pp, B_ERROR);
			return (err);
		}

		/* XXX ??? ufs has io_len instead of pgoff below */
		ip->i_nextrio = off + ((pgoff + PAGESIZE - 1) & PAGEMASK);

		/*
		 * If the file access is sequential, initiate read ahead
		 * of the next cluster.
		 */
		if (seq && ip->i_nextrio < ip->i_size)
			(void) s5_getpage_ra(vp, off, seg, addr);
	}

outmiss:
	pvn_plist_init(pp, pl, plsz, (offset_t)off, io_len, rw);
	return (err);
}

/*
 * Read ahead a cluster from the disk. Returns the length in bytes.
 */
/*ARGSUSED*/
static int
s5_getpage_ra(struct vnode *vp, uint_t off, struct seg *seg,
    caddr_t addr)
{
#ifdef HELL_FREEZES_OVER
	struct inode	*ip = VTOI(vp);
	offset_t	io_off = (offset_t)ip->i_nextrio;
	ulong_t		io_len;
	caddr_t		addr2 = addr + ((uint_t)io_off - off);
	page_t		*pp;
	uint_t		lfoff = ip->i_nextrio;
	uint_t		pgoff;
	int		bsize = FsBSIZE(ITOF(ip)->s_bshift);

	/*
	 * Is this test needed?
	 */
	if (addr2 >= seg->s_base + seg->s_size)
		return (0);
	pp = pvn_read_kluster(vp, (offset_t)lfoff, seg, addr2, &io_off, &io_len,
		(offset_t)(off & (PAGESIZE-1)), PAGESIZE, 1);
	/*
	 * Some other thread has entered the page.
	 * So no read head done here (ie we will have to and wait
	 * for the read when needed).
	 */
	if (pp == NULL) {
		return (0);
	}

	s5_page_fill(ip, pp, lfoff, (B_READ|B_ASYNC), &pgoff, seg);
	ip->i_nextrio =  lfoff + ((pgoff + PAGESIZE - 1) & PAGEMASK);

	return (pgoff);
#else
	return (0);
#endif
}

/*
 * Loop filling the page pp with file data starting at off in file
 *
 */
static int
s5_page_fill(struct inode *ip, page_t *pp, uint_t off,
    uint_t bflgs, uint_t *pg_off, struct seg *seg)
{
	int		err = 0;
	daddr_t		bn;
	int		contig = 0;
	uint_t		lfoff = off;
	int		bsize = FsBSIZE(ITOF(ip)->s_bshift);
	int		pgoff;


	for (pgoff = 0; pgoff < PAGESIZE && lfoff < ip->i_size;
		pgoff += contig, lfoff += contig) {
		struct buf	*bp;
		if (err = s5_bmap_read(ip, lfoff, &bn, &contig))
			goto outmiss;
		contig = MIN(contig, PAGESIZE-pgoff);
		/*
		 * Zero part of the page which we are not
		 * going to read from the disk.
		 */
		if (bn == S5_HOLE) {
			pagezero(pp->p_prev, pgoff, bsize);
			contig = bsize;
			continue;
		}

		bp = s5_startio(ip, bn, pp, contig, pgoff,
		    bflgs, seg == segkmap);

		if ((bflgs & B_ASYNC) == 0) {
			err = biowait(bp);
			pageio_done(bp);
			if (err)
				goto outmiss;
		}
	}

	/*
	 * Zero part of the page which we are not
	 * going to read from the disk.
	 */
	if (pgoff < PAGESIZE) {
		pagezero(pp->p_prev, pgoff, PAGESIZE - pgoff);
	}
outmiss:
	*pg_off = pgoff;
	return (err);
}


int	s5_delay = 1;
/*
 * Flags are composed of {B_INVAL, B_FREE, B_DONTNEED, B_FORCE, B_ASYNC}
 *
 * LMXXX - the inode really ought to contain a pointer to one of these
 * async args.  Stuff gunk in there and just hand the whole mess off.
 * This would replace i_delaylen, i_delayoff.
 */
/*ARGSUSED*/
static int
s5_putpage(struct vnode *vp, offset_t off, size_t len,
    int flags, struct cred *cr)
{
	int err = 0;

	if (vp->v_count == 0)
		cmn_err(CE_PANIC, "s5_putpage: bad v_count\n");

	TRACE_1(TR_FAC_S5, TR_S5_PUTPAGE_START,
		"s5_putpage_start:vp %p", vp);

	/*
	 * XXX - Why should this check be made here?
	 */
	if (vp->v_flag & VNOMAP) {
		TRACE_2(TR_FAC_S5, TR_S5_PUTPAGE_END,
			"s5_putpage_end:vp %p error %d", vp, ENOSYS);
		return (ENOSYS);
	}
	ASSERT(off <= MAXOFF_T);

#ifdef HELL_FREEZES_OVER
	ip = VTOI(vp);

	if (flags & B_ASYNC) {
		if (s5_delay && len &&
		    (flags & ~(B_ASYNC|B_DONTNEED|B_FREE)) == 0) {
			mutex_enter(&ip->i_tlock);
			/*
			 * If nobody stalled, start a new cluster.
			 */
			if (ip->i_delaylen == 0) {
				ip->i_delayoff = off;
				ip->i_delaylen = len;
				mutex_exit(&ip->i_tlock);
				TRACE_2(TR_FAC_S5, TR_S5_PUTPAGE_END,
					"s5_putpage_end:vp %p error %d",
					vp, 0);
				return (0);
			}
			/*
			 * If we have a full cluster or they are not contig,
			 * then push last cluster and start over.
			 */
			if (ip->i_delaylen >= WR_CLUSTSZ(ip) ||
			    ip->i_delayoff + ip->i_delaylen != off) {
				offset_t doff;
				uint_t dlen;

				doff = ip->i_delayoff;
				dlen = ip->i_delaylen;
				ip->i_delayoff = (long)off;
				ip->i_delaylen = (long)len;
				mutex_exit(&ip->i_tlock);
				err = s5_putpages(vp, doff, dlen,
				    flags, cr);
				/* LMXXX - flags are new val, not old */
				TRACE_2(TR_FAC_S5, TR_S5_PUTPAGE_END,
					"s5_putpage_end:vp %p error %d",
					vp, err);
				return (err);
			}
			/*
			 * There is something there, it's not full, and
			 * it is contig.
			 */
			ip->i_delaylen += len;
			mutex_exit(&ip->i_tlock);
			TRACE_2(TR_FAC_S5, TR_S5_PUTPAGE_END,
				"s5_putpage_end:vp %p error %d", vp, 0);
			return (0);
		}
		/*
		 * Must have weird flags or we are not clustering.
		 */
	}
#endif

	err = s5_putpages(vp, off, len, flags, cr);

	TRACE_2(TR_FAC_S5, TR_S5_PUTPAGE_END,
		"s5_putpage_end:vp %p error %d", vp, err);
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
s5_putpages(struct vnode *vp, offset_t off, size_t len,
    int flags, struct cred *cr)
{
	struct inode *ip;
	page_t *pp;
	u_offset_t io_off;
	ulong_t io_len;
	uint_t eoff;
	int err = 0;

	if (vp->v_count == 0)
		cmn_err(CE_PANIC, "s5_putpages: bad v_count");

	ip = VTOI(vp);

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
	rw_enter(&ip->i_contents, RW_READER);

	if (vp->v_pages == NULL) {
		rw_exit(&ip->i_contents);
		return (0);
	}

	if (len == 0) {
		/*
		 * Search the entire vp list for pages >= off.
		 */
		err = pvn_vplist_dirty(vp, (u_offset_t)off, s5_putapage,
				flags, cr);
	} else {
		/*
		 * Loop over all offsets in the range looking for
		 * pages to deal with.
		 */
		if ((eoff = blkroundup(ip->i_s5vfs, ip->i_size)) != 0)
			eoff = MIN((uint_t)off + len, eoff);
		else
			eoff = (uint_t)off + len;
#ifdef	lint
		io_len = 0;
#endif	/* lint */

		for (io_off = off; io_off < eoff; io_off += io_len) {
			/*
			 * If we are not invalidating, synchronously
			 * freeing or writing pages use the routine
			 * page_lookup_nowait() to prevent reclaiming
			 * them from the free list.
			 */
			if ((flags & B_INVAL) || ((flags & B_ASYNC) == 0)) {
				pp = page_lookup(vp, (offset_t)io_off,
					(flags & (B_INVAL | B_FREE)) ?
					    SE_EXCL : SE_SHARED);
			} else {
				pp = page_lookup_nowait(vp, (offset_t)io_off,
					(flags & B_FREE) ? SE_EXCL : SE_SHARED);
			}

			if (pp == NULL || pvn_getdirty(pp, flags) == 0)
				io_len = PAGESIZE;
			else {
				err = s5_putapage(vp, pp, &io_off,
				    &io_len, flags, cr);
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
	if (err == 0 && off == 0 && (len == 0 || len >= ip->i_size)) {
		/*
		 * We have just sync'ed back all the pages on
		 * the inode, turn off the IMODTIME flag.
		 */
		mutex_enter(&ip->i_tlock);
		ip->i_flag &= ~IMODTIME;
		mutex_exit(&ip->i_tlock);
	}
	rw_exit(&ip->i_contents);
	return (err);
}

/*
 * Write out a single page, possibly klustering adjacent
 * dirty pages.  The inode lock must be held.
 *
 * LMXXX - bsize < pagesize not done.
 */
/*ARGSUSED*/
int
s5_putapage(struct vnode *vp, page_t *pp, u_offset_t *offp, size_t *lenp,
    int flags, struct cred *cr)
{
	struct inode *ip;
	struct filsys *fs;
	u_offset_t io_off;
	ulong_t	io_len;
	daddr_t	bn;
	int	err;
	int	off, contig = 0;
	long	bsize;
	uint_t	lfoff;
	int	pgoff;

	flags &= ~B_ASYNC;

	ip = VTOI(vp);
	ASSERT(RW_LOCK_HELD(&ip->i_contents));

	bsize = ip->i_s5vfs->vfs_bsize;

	TRACE_1(TR_FAC_S5, TR_S5_PUTAPAGE_START,
		"s5_putapage_start:vp %p", vp);

	fs = ITOF(ip);
	ASSERT(fs->s_ronly == 0);

	/*
	 * If the modified time on the inode has not already been
	 * set elsewhere (e.g. for write/setattr) and this is not
	 * a call from msync (B_FORCE) we set the time now.
	 * This gives us approximate modified times for mmap'ed files
	 * which are modified via stores in the user address space.
	 */
	if ((ip->i_flag & IMODTIME) == 0 || (flags & B_FORCE)) {
		mutex_enter(&ip->i_tlock);
		ip->i_flag |= IUPD;
		ITIMES_NOLOCK(ip);
		mutex_exit(&ip->i_tlock);

		/*
		 * This is an attempt to clean up loose ends left by
		 * applications that store into mapped files.  It's
		 * insufficient, strictly speaking, for ill-behaved
		 * applications, but about the best we can do.
		 */
		if (vp->v_type == VREG) {
			/* XXX! i_contents must be upgraded to write version */
			err = fs_vcode(vp, &ip->i_vcode);
			if (err) {
				goto out;
			}
		}
	}

	/*
	 * Align the request to a block boundry (for old file systems),
	 * and go ask bmap() how contiguous things are for this file.
	 */
	off = (int)pp->p_offset & ~(bsize - 1);	/* block align it */
	lfoff = off;

	/*
	 * Take the length (of contiguous bytes) passed back from bmap()
	 * and _try_ and get a set of pages covering that extent.
	 */
	pp = pvn_write_kluster(vp, pp, &io_off, &io_len, (u_offset_t)off,
		PAGESIZE, flags);

	if (io_len == 0)
		io_len = PAGESIZE;

#ifdef	WRITE_THROTTLE
		mutex_enter(&ip->i_tlock);
		ip->i_writes += PAGESIZE;
		mutex_exit(&ip->i_tlock);
#endif

	for (pgoff = 0; pgoff < io_len && lfoff < ip->i_size;
		pgoff += contig, lfoff += contig) {
		struct buf	*bp;

		if (err = s5_bmap_read(ip, lfoff, &bn, &contig))
			goto out;
		if (bn == S5_HOLE)		/* putpage never allocates */
			cmn_err(CE_PANIC, "s5_putapage: bn == S5_HOLE");
		contig = MIN(contig, io_len-pgoff);

		bp = s5_startio(ip, bn, pp, contig, pgoff, B_WRITE | flags, 0);

		if ((flags & B_ASYNC) == 0) {
			/*
			 * Wait for i/o to complete.
			 */
			err = biowait(bp);
			pageio_done(bp);
		}


	}
	if ((flags & B_ASYNC) == 0)
		pvn_write_done(pp, ((err) ? B_ERROR : 0) | B_WRITE | flags);

#ifdef	WRITE_THROTTLE
		mutex_enter(&ip->i_tlock);
		ip->i_writes -= PAGESIZE;
		if (ip->i_writes < 0)
			ip->i_writes = 0;
		cv_signal(&ip->i_wrcv);
		mutex_exit(&ip->i_tlock);
#endif


	pp = NULL;

out:
	if (err != 0 && pp != NULL)
		pvn_write_done(pp, B_ERROR | B_WRITE | flags);

	if (pgoff < PAGESIZE)
		io_len -= (PAGESIZE - pgoff);
	if (offp)
		*offp = io_off;
	if (lenp)
		*lenp = io_len;
	TRACE_2(TR_FAC_S5, TR_S5_PUTAPAGE_END,
		"s5_putapage_end:vp %p error %d", vp, err);
	return (err);
}

static int
s5_iodone(struct buf *bp)
{
	struct inode *ip;

	ASSERT((bp->b_pages->p_vnode != NULL) && !(bp->b_flags & B_READ));

	bp->b_iodone = NULL;

	ip = VTOI(bp->b_pages->p_vnode);

	mutex_enter(&ip->i_tlock);
	if (ip->i_writes >= s5_LW) {
		if ((ip->i_writes -= bp->b_bcount) <= s5_LW)
			if (s5_WRITES)
				cv_broadcast(&ip->i_wrcv); /* wake all up */
	} else {
		ip->i_writes -= bp->b_bcount;
	}

	mutex_exit(&ip->i_tlock);
	iodone(bp);
	return (0);
}
/*
 * Flags are composed of {B_READ, B_WRITE, B_ASYNC, B_INVAL, B_FREE, B_DONTNEED}
 */
static struct buf *
s5_startio(struct inode *ip, daddr_t bn, page_t *pp,
    size_t len, uint_t pgoff, int flags, int io)
{
	struct buf *bp;
	klwp_t *lwp = ttolwp(curthread);

#ifdef	lint
	io = io;
#endif	/* lint */

	TRACE_1(TR_FAC_S5, TR_S5_STARTIO_START,
		"s5_startio_start:vp %p", ITOV(ip));

	bp = pageio_setup(pp, len, ip->i_devvp, flags);
	ASSERT(bp != NULL);

	bp->b_edev = ip->i_dev;
	bp->b_dev = cmpdev(ip->i_dev);
	bp->b_blkno = bn;
	bp->b_un.b_addr = (caddr_t)pgoff;
	/* write throttle */

	if (!(bp->b_flags & B_READ)) {
		ASSERT(bp->b_iodone == NULL);
		bp->b_iodone = s5_iodone;
		mutex_enter(&ip->i_tlock);
		ip->i_writes += bp->b_bcount;
		mutex_exit(&ip->i_tlock);
	}

	(void) bdev_strategy(bp);

	if (lwp != NULL) {
		if (!(bp->b_flags & B_READ))
			lwp->lwp_ru.oublock++;
		else
			lwp->lwp_ru.inblock++;
	}

	TRACE_1(TR_FAC_S5, TR_S5_STARTIO_END,
		"s5_startio_end:vp %p", ITOV(ip));
	return (bp);
}

/* ARGSUSED */
static int
s5_map(struct vnode *vp, offset_t off, struct as *as, caddr_t *addrp,
    size_t len, uchar_t prot, uchar_t maxprot, uint_t flags,
    struct cred *cr)
{
	struct segvn_crargs vn_a;
	int error;
	struct ulockfs *ulp;

	TRACE_1(TR_FAC_S5, TR_S5_MAP_START,
		"s5_map_start:vp %x", vp);

	if (error = s5_lockfs_vp_begin(vp, &ulp))
		goto out;

	if (vp->v_flag & VNOMAP) {
		error = ENOSYS;
		goto unlock;
	}

	if (off < 0 || (offset_t)(off + len) < 0) {
		error = EINVAL;
		goto unlock;
	}

	if (off > MAXOFF_T) {
		error = EFBIG;
		goto unlock;
	}

	if ((off + len) > MAXOFF_T) {
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
	vn_a.offset = off;
	vn_a.type = flags & MAP_TYPE;
	vn_a.prot = prot;
	vn_a.maxprot = maxprot;
	vn_a.cred = cr;
	vn_a.amp = NULL;

	error = as_map(as, *addrp, len, segvn_create, &vn_a);
	as_rangeunlock(as);

unlock:
	s5_lockfs_end(ulp);
out:
	TRACE_2(TR_FAC_S5, TR_S5_MAP_END,
		"s5_map_end:vp %x error %d", vp, error);
	return (error);
}

/* ARGSUSED */
static int
s5_addmap(struct vnode *vp, offset_t off, struct as *as, caddr_t addr,
    size_t len, uchar_t prot, uchar_t maxprot, uint_t flags,
    struct cred *cr)
{
	struct inode *ip = VTOI(vp);

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
s5_delmap(struct vnode *vp, offset_t off, struct as *as,
    caddr_t addr, size_t len, uint_t prot, uint_t maxprot, uint_t flags,
    struct cred *cr)
{
	struct inode *ip = VTOI(vp);

	if (vp->v_flag & VNOMAP) {
		return (ENOSYS);
	}

	mutex_enter(&ip->i_tlock);
	ip->i_mapcnt -= btopr(len); 	/* Count released mappings */
	ASSERT(ip->i_mapcnt >= 0);
	mutex_exit(&ip->i_tlock);
	return (0);
}

static int
s5_l_pathconf(struct vnode *vp, int cmd, ulong_t *valp,
    struct cred *cr)
{
	struct ulockfs	*ulp;
	int		error;

	/* file system has been forcibly unmounted */
	if (VTOI(vp)->i_s5vfs == NULL)
		return (EIO);
	if (curthread->t_flag & T_DONTBLOCK)
		return (fs_pathconf(vp, cmd, valp, cr));
	ulp = VTOUL(vp);
	if (error = s5_lockfs_begin(ulp))
		return (error);
	error = fs_pathconf(vp, cmd, valp, cr);
	s5_lockfs_end(ulp);

	return (error);
}

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	Copyright (c) 1986-1992,1994,1996-1999 by Sun Microsystems, Inc.
 *	Copyright (c) 1983,1984,1985,1986,1987,1988,1989 AT&T.
 *	All rights reserved.
 */

/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nfs3_vnops.c	1.183	99/12/11 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/mman.h>
#include <sys/pathname.h>
#include <sys/dirent.h>
#include <sys/debug.h>
#include <sys/vmsystm.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/swap.h>
#include <sys/errno.h>
#include <sys/strsubr.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/pathconf.h>
#include <sys/utsname.h>
#include <sys/dnlc.h>
#include <sys/acl.h>
#include <sys/systeminfo.h>

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>

#include <nfs/nfs.h>
#include <nfs/nfs_clnt.h>
#include <nfs/rnode.h>
#include <nfs/nfs_acl.h>
#include <nfs/lm.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>

#include <fs/fs_subr.h>

#include <sys/ddi.h>

static int	nfs3_rdwrlbn(vnode_t *, page_t *, u_offset_t, size_t, int,
			cred_t *);
static int	nfs3write(vnode_t *, caddr_t, u_offset_t, int, cred_t *,
			stable_how *);
static int	nfs3read(vnode_t *, caddr_t, u_offset_t, int, size_t *,
			cred_t *);
static int	nfs3setattr(vnode_t *, struct vattr *, int, cred_t *);
static int	nfs3lookup_dnlc(vnode_t *, char *, vnode_t **, cred_t *);
static int	nfs3lookup_otw(vnode_t *, char *, vnode_t **, cred_t *, int);
static int	nfs3create(vnode_t *, char *, struct vattr *, enum vcexcl,
			int, vnode_t **, cred_t *, int);
static int	nfs3mknod(vnode_t *, char *, struct vattr *, enum vcexcl,
			int, vnode_t **, cred_t *);
static int	nfs3rename(vnode_t *, char *, vnode_t *, char *, cred_t *);
static int	do_nfs3readdir(vnode_t *, rddir_cache *, cred_t *);
static int	do_nfs3asyncreaddir(vnode_t *, rddir_cache *, cred_t *);
static void	nfs3readdir(vnode_t *, rddir_cache *, cred_t *);
static void	nfs3readdirplus(vnode_t *, rddir_cache *, cred_t *);
static int	nfs3_bio(struct buf *, stable_how *, cred_t *);
static int	nfs3_getapage(vnode_t *, u_offset_t, size_t, uint_t *,
			page_t *[], size_t, struct seg *, caddr_t,
			enum seg_rw, cred_t *);
static void	nfs3_readahead(vnode_t *, u_offset_t, caddr_t, struct seg *,
			cred_t *);
static int	nfs3_sync_putapage(vnode_t *, page_t *, u_offset_t, size_t,
			int, cred_t *);
static int	nfs3_sync_pageio(vnode_t *, page_t *, u_offset_t, size_t,
			int, cred_t *);
static int	nfs3_commit(vnode_t *, offset3, count3, cred_t *);
static void	nfs3_set_mod(vnode_t *);
static void	nfs3_get_commit(vnode_t *);
static void	nfs3_get_commit_range(vnode_t *, u_offset_t, size_t);
#if 0 /* unused */
#ifdef DEBUG
static int	nfs3_no_uncommitted_pages(vnode_t *);
#endif
#endif /* unused */
static int	nfs3_putpage_commit(vnode_t *, u_offset_t, size_t, cred_t *);
static int	nfs3_commit_vp(vnode_t *, u_offset_t, size_t,  cred_t *);
static int	nfs3_sync_commit(vnode_t *, page_t *, offset3, count3,
			cred_t *);
static void	nfs3_async_commit(vnode_t *, page_t *, offset3, count3,
			cred_t *);

/*
 * Error flags used to pass information about certain special errors
 * which need to be handled specially.
 */
#define	NFS_EOF			-98
#define	NFS_VERF_MISMATCH	-97

#define	ISVDEV(t) ((t == VBLK) || (t == VCHR) || (t == VFIFO))

/* ALIGN64 aligns the given buffer and adjust buffer size to 64 bit */
#define	ALIGN64(x, ptr, sz)						\
	x = ((uintptr_t)(ptr)) & (sizeof (uint64_t) - 1);		\
	if (x) {							\
		x = sizeof (uint64_t) - (x);				\
		sz -= (x);						\
		ptr += (x);						\
	}

/*
 * These are the vnode ops routines which implement the vnode interface to
 * the networked file system.  These routines just take their parameters,
 * make them look networkish by putting the right info into interface structs,
 * and then calling the appropriate remote routine(s) to do the work.
 *
 * Note on directory name lookup cacheing:  If we detect a stale fhandle,
 * we purge the directory cache relative to that vnode.  This way, the
 * user won't get burned by the cache repeatedly.  See <nfs/rnode.h> for
 * more details on rnode locking.
 */

static int	nfs3_open(vnode_t **, int, cred_t *);
static int	nfs3_close(vnode_t *, int, int, offset_t, cred_t *);
static int	nfs3_read(vnode_t *, struct uio *, int, cred_t *);
static int	nfs3_write(vnode_t *, struct uio *, int, cred_t *);
static int	nfs3_ioctl(vnode_t *, int, intptr_t, int, cred_t *, int *);
static int	nfs3_getattr(vnode_t *, struct vattr *, int, cred_t *);
static int	nfs3_setattr(vnode_t *, struct vattr *, int, cred_t *);
static int	nfs3_access(vnode_t *, int, int, cred_t *);
static int	nfs3_readlink(vnode_t *, struct uio *, cred_t *);
static int	nfs3_fsync(vnode_t *, int, cred_t *);
static void	nfs3_inactive(vnode_t *, cred_t *);
static int	nfs3_lookup(vnode_t *, char *, vnode_t **,
			struct pathname *, int, vnode_t *, cred_t *);
static int	nfs3_create(vnode_t *, char *, struct vattr *, enum vcexcl,
			int, vnode_t **, cred_t *, int);
static int	nfs3excl_create_settimes(vnode_t *, struct vattr *, cred_t *);
static int	nfs3_remove(vnode_t *, char *, cred_t *);
static int	nfs3_link(vnode_t *, vnode_t *, char *, cred_t *);
static int	nfs3_rename(vnode_t *, char *, vnode_t *, char *, cred_t *);
static int	nfs3_mkdir(vnode_t *, char *, struct vattr *,
			vnode_t **, cred_t *);
static int	nfs3_rmdir(vnode_t *, char *, vnode_t *, cred_t *);
static int	nfs3_symlink(vnode_t *, char *, struct vattr *, char *,
			cred_t *);
static int	nfs3_readdir(vnode_t *, struct uio *, cred_t *, int *);
static int	nfs3_fid(vnode_t *, fid_t *);
static void	nfs3_rwlock(vnode_t *, int);
static void	nfs3_rwunlock(vnode_t *, int);
static int	nfs3_seek(vnode_t *, offset_t, offset_t *);
static int	nfs3_getpage(vnode_t *, offset_t, size_t, uint_t *,
			page_t *[], size_t, struct seg *, caddr_t,
			enum seg_rw, cred_t *);
static int	nfs3_putpage(vnode_t *, offset_t, size_t, int, cred_t *);
static int	nfs3_map(vnode_t *, offset_t, struct as *, caddr_t *,
			size_t, uchar_t, uchar_t, uint_t, cred_t *);
static int	nfs3_addmap(vnode_t *, offset_t, struct as *, caddr_t,
			size_t, uchar_t, uchar_t, uint_t, cred_t *);
static int	nfs3_cmp(vnode_t *, vnode_t *);
static int	nfs3_frlock(vnode_t *, int, struct flock64 *, int, offset_t,
			cred_t *);
static int	nfs3_space(vnode_t *, int, struct flock64 *, int, offset_t,
			cred_t *);
static int	nfs3_realvp(vnode_t *, vnode_t **);
static int	nfs3_delmap(vnode_t *, offset_t, struct as *, caddr_t,
			size_t, uint_t, uint_t, uint_t, cred_t *);
static int	nfs3_pathconf(vnode_t *, int, ulong_t *, cred_t *);
static int	nfs3_pageio(vnode_t *, page_t *, u_offset_t, size_t, int,
			cred_t *);
static void	nfs3_dispose(vnode_t *, page_t *, int, int, cred_t *);
static int	nfs3_setsecattr(vnode_t *, vsecattr_t *, int, cred_t *);
static int	nfs3_getsecattr(vnode_t *, vsecattr_t *, int, cred_t *);
static int	nfs3_shrlock(vnode_t *, int, struct shrlock *, int);

struct vnodeops nfs3_vnodeops = {
	nfs3_open,
	nfs3_close,
	nfs3_read,
	nfs3_write,
	nfs3_ioctl,
	fs_setfl,
	nfs3_getattr,
	nfs3_setattr,
	nfs3_access,
	nfs3_lookup,
	nfs3_create,
	nfs3_remove,
	nfs3_link,
	nfs3_rename,
	nfs3_mkdir,
	nfs3_rmdir,
	nfs3_readdir,
	nfs3_symlink,
	nfs3_readlink,
	nfs3_fsync,
	nfs3_inactive,
	nfs3_fid,
	nfs3_rwlock,
	nfs3_rwunlock,
	nfs3_seek,
	nfs3_cmp,
	nfs3_frlock,
	nfs3_space,
	nfs3_realvp,
	nfs3_getpage,
	nfs3_putpage,
	nfs3_map,
	nfs3_addmap,
	nfs3_delmap,
	fs_poll,
	nfs_dump,
	nfs3_pathconf,
	nfs3_pageio,
	fs_nosys,	/* dumpctl */
	nfs3_dispose,
	nfs3_setsecattr,
	nfs3_getsecattr,
	nfs3_shrlock
};

/*
 * XXX:  This is referenced in modstubs.s
 */
struct vnodeops *
nfs3_getvnodeops(void)
{

	return (&nfs3_vnodeops);
}

/* ARGSUSED */
static int
nfs3_open(vnode_t **vpp, int flag, cred_t *cr)
{
	int error;
	struct vattr va;
	rnode_t *rp;

	rp = VTOR(*vpp);
	mutex_enter(&rp->r_statelock);
	if (rp->r_cred == NULL) {
		crhold(cr);
		rp->r_cred = cr;
	}
	mutex_exit(&rp->r_statelock);

	/*
	 * If there is no cached data or if close-to-open
	 * consistancy checking is turned off, we can avoid
	 * the over the wire getattr.  Otherwise, force a
	 * call to the server to get fresh attributes and to
	 * check caches. This is required for close-to-open
	 * consistency.
	 */
	if (((*vpp)->v_pages != NULL || rp->r_dir != NULL) &&
	    !(VTOMI(*vpp)->mi_flags & MI_NOCTO)) {
		va.va_mask = AT_ALL;
		error = nfs3_getattr_otw(*vpp, &va, cr);
	} else
		error = 0;

	return (error);
}

static int
nfs3_close(vnode_t *vp, int flag, int count, offset_t offset, cred_t *cr)
{
	rnode_t *rp;
	int error;

	/*
	 * If we are using local locking for this filesystem, then
	 * release all of the SYSV style record locks.  Otherwise,
	 * we are doing network locking and we need to release all
	 * of the network locks.  All of the locks held by this
	 * process on this file are released no matter what the
	 * incoming reference count is.
	 */
	if (VTOMI(vp)->mi_flags & MI_LLOCK) {
		cleanlocks(vp, ttoproc(curthread)->p_pid, 0);
		cleanshares(vp, ttoproc(curthread)->p_pid);
	} else
		nfs_lockrelease(vp, flag, offset, cr);

	if (count > 1)
		return (0);

	/*
	 * If the file has been `unlinked', then purge the
	 * DNLC so that this vnode will get reycled quicker
	 * and the .nfs* file on the server will get removed.
	 */
	rp = VTOR(vp);
	if (rp->r_unldvp != NULL)
		dnlc_purge_vp(vp);

	/*
	 * If the file was open for write and there are pages,
	 * then if the file system was mounted using the "no-close-
	 *	to-open" semantics, then start an asynchronous flush
	 *	of the all of the pages in the file.
	 * else the file system was not mounted using the "no-close-
	 *	to-open" semantics, then do a synchronous flush and
	 *	commit of all of the dirty and uncommitted pages.
	 *
	 * The asynchronous flush of the pages in the "nocto" path
	 * mostly just associates a cred pointer with the rnode so
	 * writes which happen later will have a better chance of
	 * working.  It also starts the data being written to the
	 * server, but without unnecessarily delaying the application.
	 */
	if ((flag & FWRITE) && vp->v_pages != NULL) {
		if (VTOMI(vp)->mi_flags & MI_NOCTO)
			error = nfs3_putpage(vp, (u_offset_t)0, 0, B_ASYNC, cr);
		else
			error = nfs3_putpage_commit(vp, (u_offset_t)0, 0, cr);
		if (!error) {
			mutex_enter(&rp->r_statelock);
			error = rp->r_error;
			rp->r_error = 0;
			mutex_exit(&rp->r_statelock);
		}
	} else {
		mutex_enter(&rp->r_statelock);
		error = rp->r_error;
		rp->r_error = 0;
		mutex_exit(&rp->r_statelock);
	}

	return (error);
}

/* ARGSUSED */
static int
nfs3_read(vnode_t *vp, struct uio *uiop, int ioflag, cred_t *cr)
{
	rnode_t *rp;
	u_offset_t off;
	offset_t diff;
	uint_t on;
	uint_t n;
	caddr_t base;
	uint_t flags;
	int error;

	rp = VTOR(vp);

	ASSERT(nfs_rw_lock_held(&rp->r_rwlock, RW_READER));

	if (vp->v_type != VREG)
		return (EISDIR);

	if (uiop->uio_resid == 0)
		return (0);

	if (uiop->uio_loffset < 0)
		return (EINVAL);

	/*
	 * Bypass VM if caching has been disabled (e.g., locking) or if
	 * using client-side direct I/O.
	 */

	if (vp->v_flag & VNOCACHE || rp->r_flags & RDIRECTIO ||
		(VTOMI(vp)->mi_flags) & MI_DIRECTIO) {

		size_t bufsize;
		size_t resid = 0;

		/*
		 * Let's try to do read in as large a chunk as we can
		 * (Filesystem (NFS client) bsize if possible/needed).
		 * For V3, this is 32K and for V2, this is 8K.
		 */
		bufsize = MIN(uiop->uio_resid, VTOMI(vp)->mi_curread);
		base = kmem_alloc(bufsize, KM_SLEEP);
		do {
			n = MIN(uiop->uio_resid, bufsize);
			error = nfs3read(vp, base,
			    (u_offset_t)uiop->uio_loffset,
			    n, &resid, cr);
			if (!error) {
				n -= resid;
				error = uiomove(base, n, UIO_READ, uiop);
			}
		} while (!error && uiop->uio_resid > 0 && n);
		kmem_free(base, bufsize);
		return (error);
	}

	do {
		off = uiop->uio_loffset & MAXBMASK; /* mapping offset */
		on = uiop->uio_loffset & MAXBOFFSET; /* Relative offset */
		n = MIN(MAXBSIZE - on, uiop->uio_resid);

		error = nfs3_validate_caches(vp, cr);
		if (error)
			break;

		mutex_enter(&rp->r_statelock);
		diff = rp->r_size - uiop->uio_loffset;
		mutex_exit(&rp->r_statelock);
		if (diff <= 0)
			break;
		if (diff < n)
			n = (uint_t)diff;

		base = segmap_getmapflt(segkmap, vp, (off + on),
		    n, 1, S_READ);

		error = uiomove(base + on, n, UIO_READ, uiop);

		if (!error) {
			/*
			 * If read a whole block or read to eof,
			 * won't need this buffer again soon.
			 */
			mutex_enter(&rp->r_statelock);
			if (n + on == MAXBSIZE ||
			    uiop->uio_loffset == rp->r_size)
				flags = SM_DONTNEED;
			else
				flags = 0;
			mutex_exit(&rp->r_statelock);
			error = segmap_release(segkmap, base, flags);
		} else
			(void) segmap_release(segkmap, base, 0);
	} while (!error && uiop->uio_resid > 0);

	return (error);
}

static int
nfs3_write(vnode_t *vp, struct uio *uiop, int ioflag, cred_t *cr)
{
	rnode_t *rp;
	u_offset_t off;
	caddr_t base;
	uint_t flags;
	int remainder;
	int n;
	int on;
	int error;
	int resid;
	u_offset_t offset;
	mntinfo_t *mi;
	uint_t bsize;

	rp = VTOR(vp);

	ASSERT(nfs_rw_lock_held(&rp->r_rwlock, RW_WRITER));

	if (vp->v_type != VREG)
		return (EISDIR);

	if (uiop->uio_resid == 0)
		return (0);

	if (ioflag & FAPPEND) {
		struct vattr va;

		va.va_mask = AT_SIZE;
		error = nfs3getattr(vp, &va, cr);
		if (error)
			return (error);
		uiop->uio_loffset = va.va_size;
	}

	if (uiop->uio_loffset < (offset_t)0)
		return (EINVAL);

	offset = uiop->uio_loffset + uiop->uio_resid;

	/*
	 * Check to make sure that the process will not exceed
	 * its limit on file size.  It is okay to write up to
	 * the limit, but not beyond.  Thus, the write which
	 * reaches the limit will be short and the next write
	 * will return an error.
	 */
	remainder = 0;
	if (offset > uiop->uio_llimit) {
		remainder = offset - uiop->uio_llimit;
		uiop->uio_resid = uiop->uio_llimit - uiop->uio_loffset;
		if (uiop->uio_resid <= 0) {
			uiop->uio_resid += remainder;
			psignal(ttoproc(curthread), SIGXFSZ);
			return (EFBIG);
		}
	}

	if (nfs_rw_enter_sig(&rp->r_lkserlock, RW_READER, INTR(vp)))
		return (EINTR);

	/*
	 * Bypass VM if caching has been disabled (e.g., locking) or if
	 * using client-side direct I/O.
	 */

	if (vp->v_flag & VNOCACHE || rp->r_flags & RDIRECTIO ||
		(VTOMI(vp)->mi_flags) & MI_DIRECTIO) {

		size_t bufsize;
		int count;
		u_offset_t org_offset;
		stable_how stab_comm;
nfs3_fwrite:

		if (rp->r_flags & RDONTWRITE) {
			resid = uiop->uio_resid;
			offset = uiop->uio_loffset;
			error = rp->r_error;
			goto bottom;
		}

		bufsize = MIN(uiop->uio_resid, PAGESIZE);
		base = kmem_alloc(bufsize, KM_SLEEP);
		stab_comm = FILE_SYNC;

		do {
			resid = uiop->uio_resid;
			offset = uiop->uio_loffset;
			count = MIN(uiop->uio_resid, PAGESIZE);
			org_offset = uiop->uio_loffset;
			error = uiomove(base, count, UIO_WRITE, uiop);
			if (!error) {
				error = nfs3write(vp, base, org_offset,
				    count, cr, &stab_comm);
			}
		} while (!error && uiop->uio_resid > 0);

		kmem_free(base, bufsize);
		goto bottom;
	}

	mi = VTOMI(vp);

	bsize = vp->v_vfsp->vfs_bsize;

	do {
		u_offset_t uoff = (u_offset_t)uiop->uio_loffset;
		/* mapping offset */
		off = uoff & (u_offset_t)MAXBMASK;
		/* Relative offset */
		on = uoff & (u_offset_t)MAXBOFFSET;
		n = MIN(MAXBSIZE - on, uiop->uio_resid);

		resid = uiop->uio_resid;
		offset = uiop->uio_loffset;

		if (rp->r_flags & RDONTWRITE) {
			error = rp->r_error;
			break;
		}

		base = segmap_getmapflt(segkmap, vp, (off + on),
		    (uint_t)n, 0, S_READ);

		error = writerp(rp, base + on, n, uiop);

		if (!error) {
			if (mi->mi_flags & MI_NOAC)
				flags = SM_WRITE;
			else if ((uiop->uio_loffset % bsize) == 0 ||
			    IS_SWAPVP(vp)) {
				/*
				 * Have written a whole block.
				 * Start an asynchronous write
				 * and mark the buffer to
				 * indicate that it won't be
				 * needed again soon.
				 */
				flags = SM_WRITE | SM_ASYNC | SM_DONTNEED;
			} else
				flags = 0;
			if ((ioflag & (FSYNC|FDSYNC)) ||
			    (rp->r_flags & ROUTOFSPACE)) {
				flags &= ~SM_ASYNC;
				flags |= SM_WRITE;
			}
			error = segmap_release(segkmap, base, flags);
		} else {
			(void) segmap_release(segkmap, base, 0);
			/*
			 * In the event that we got an access error while
			 * faulting in a page for a write-only file just
			 * force a write.
			 */
			if (error == EACCES)
				goto nfs3_fwrite;
		}
	} while (!error && uiop->uio_resid > 0);

bottom:
	if (error) {
		uiop->uio_resid = resid + remainder;
		uiop->uio_loffset = offset;
	} else
		uiop->uio_resid += remainder;

	nfs_rw_exit(&rp->r_lkserlock);

	return (error);
}

/*
 * Flags are composed of {B_ASYNC, B_INVAL, B_FREE, B_DONTNEED}
 */
static int
nfs3_rdwrlbn(vnode_t *vp, page_t *pp, u_offset_t off, size_t len,
	int flags, cred_t *cr)
{
	struct buf *bp;
	int error;
	page_t *savepp;
	uchar_t fsdata;
	stable_how stab_comm;

	bp = pageio_setup(pp, len, vp, flags);
	ASSERT(bp != NULL);

	/*
	 * pageio_setup should have set b_addr to 0.  This
	 * is correct since we want to do I/O on a page
	 * boundary.  bp_mapin will use this addr to calculate
	 * an offset, and then set b_addr to the kernel virtual
	 * address it allocated for us.
	 */
	ASSERT(bp->b_un.b_addr == 0);

	bp->b_edev = 0;
	bp->b_dev = 0;
	bp->b_lblkno = lbtodb(off);
	bp_mapin(bp);

	if ((flags & (B_WRITE|B_ASYNC)) == (B_WRITE|B_ASYNC) &&
	    freemem > desfree)
		stab_comm = UNSTABLE;
	else
		stab_comm = FILE_SYNC;

	error = nfs3_bio(bp, &stab_comm, cr);

	bp_mapout(bp);
	pageio_done(bp);

	if (stab_comm == UNSTABLE)
		fsdata = C_DELAYCOMMIT;
	else
		fsdata = C_NOCOMMIT;

	savepp = pp;
	do {
		pp->p_fsdata = fsdata;
	} while ((pp = pp->p_next) != savepp);

	return (error);
}

/*
 * Write to file.  Writes to remote server in largest size
 * chunks that the server can handle.  Write is synchronous.
 */
static int
nfs3write(vnode_t *vp, caddr_t base, u_offset_t offset, int count, cred_t *cr,
	stable_how *stab_comm)
{
	mntinfo_t *mi;
	WRITE3args args;
	WRITE3res res;
	int error;
	int tsize;
	stable_how stable;
	rnode_t *rp;
	int douprintf = 1;
	klwp_t *lwp = ttolwp(curthread);

	rp = VTOR(vp);
	mi = VTOMI(vp);

	stable = *stab_comm;
	*stab_comm = FILE_SYNC;

	args.file = *VTOFH3(vp);
	args.stable = stable;

	do {
		tsize = MIN(mi->mi_curwrite, count);
		args.offset = (offset3)offset;
		args.count = (count3)tsize;
		args.data.data_len = (uint_t)tsize;
		args.data.data_val = base;

		if (mi->mi_io_kstats) {
			mutex_enter(&mi->mi_lock);
			kstat_runq_enter(KSTAT_IO_PTR(mi->mi_io_kstats));
			mutex_exit(&mi->mi_lock);
		}
		args.mblk = NULL;
		do {
			error = rfs3call(mi, NFSPROC3_WRITE,
			    xdr_WRITE3args, (caddr_t)&args,
			    xdr_WRITE3res, (caddr_t)&res, cr,
			    &douprintf, &res.status, 0, NULL);
		} while (error == ENFS_TRYAGAIN);
		if (mi->mi_io_kstats) {
			mutex_enter(&mi->mi_lock);
			kstat_runq_exit(KSTAT_IO_PTR(mi->mi_io_kstats));
			mutex_exit(&mi->mi_lock);
		}

		if (error)
			return (error);
		error = geterrno3(res.status);
		if (!error) {
			if (res.resok.count > args.count) {
				cmn_err(CE_WARN,
				    "nfs3write: server %s wrote %u, "
				    "requested was %u",
				    rp->r_server->sv_hostname,
				    res.resok.count, args.count);
				return (EIO);
			}
			if (res.resok.committed == UNSTABLE) {
				*stab_comm = UNSTABLE;
				if (args.stable == DATA_SYNC ||
				    args.stable == FILE_SYNC) {
					cmn_err(CE_WARN,
			"nfs3write: server %s did not commit to stable storage",
					    rp->r_server->sv_hostname);
					return (EIO);
				}
			}
			tsize = (int)res.resok.count;
			count -= tsize;
			base += tsize;
			offset += tsize;
			if (mi->mi_io_kstats) {
				mutex_enter(&mi->mi_lock);
				KSTAT_IO_PTR(mi->mi_io_kstats)->writes++;
				KSTAT_IO_PTR(mi->mi_io_kstats)->nwritten +=
				    tsize;
				mutex_exit(&mi->mi_lock);
			}
			if (lwp != NULL)
				lwp->lwp_ru.oublock++;
			mutex_enter(&rp->r_statelock);
			if (rp->r_flags & RHAVEVERF) {
				if (bcmp(&rp->r_verf, &res.resok.verf,
				    sizeof (writeverf3)) != 0) {
					nfs3_set_mod(vp);
					bcopy(&res.resok.verf, &rp->r_verf,
					    sizeof (writeverf3));
				}
			} else {
				bcopy(&res.resok.verf, &rp->r_verf,
				    sizeof (writeverf3));
				rp->r_flags |= RHAVEVERF;
			}
			mutex_exit(&rp->r_statelock);
		}
	} while (!error && count);

	if (res.status == NFS3_OK)
		nfs3_check_wcc_data(vp, &res.resok.file_wcc);
	else
		nfs3_check_wcc_data(vp, &res.resfail.file_wcc);

	return (error);
}

/*
 * Read from a file.  Reads data in largest chunks our interface can handle.
 */
static int
nfs3read(vnode_t *vp, caddr_t base, u_offset_t offset, int count,
	size_t *residp, cred_t *cr)
{
	mntinfo_t *mi;
	READ3args args;
	READ3res res;
	int tsize;
	int error;
	int rpcerror;
	int douprintf;
	failinfo_t fi;
	rnode_t *rp;
	int seq;
	struct vattr va;
	klwp_t *lwp = ttolwp(curthread);

	rp = VTOR(vp);
	mi = VTOMI(vp);
	douprintf = 1;

	args.file = *VTOFH3(vp);
	fi.vp = vp;
	fi.fhp = (caddr_t)&args.file;
	fi.copyproc = nfs3copyfh;
	fi.lookupproc = nfs3lookup;

	do {
		if (mi->mi_io_kstats) {
			mutex_enter(&mi->mi_lock);
			kstat_runq_enter(KSTAT_IO_PTR(mi->mi_io_kstats));
			mutex_exit(&mi->mi_lock);
		}

		do {
			tsize = MIN(mi->mi_curread, count);
			res.resok.data.data_val = base;
			res.resok.data.data_len = tsize;
			args.offset = (offset3)offset;
			args.count = (count3)tsize;
			rpcerror = rfs3call(mi, NFSPROC3_READ,
			    xdr_READ3args, (caddr_t)&args,
			    xdr_READ3res, (caddr_t)&res, cr,
			    &douprintf, &res.status, 0, &fi);
		} while (rpcerror == ENFS_TRYAGAIN);

		if (mi->mi_io_kstats) {
			mutex_enter(&mi->mi_lock);
			kstat_runq_exit(KSTAT_IO_PTR(mi->mi_io_kstats));
			mutex_exit(&mi->mi_lock);
		}

		if (rpcerror)
			break;

		error = geterrno3(res.status);
		if (!error) {
			if (res.resok.count != res.resok.data.data_len) {
				cmn_err(CE_WARN,
				"nfs3read: server %s returned incorrect amount",
				    rp->r_server->sv_hostname);
				error = EIO;
			} else {
				count -= res.resok.count;
				base += res.resok.count;
				offset += res.resok.count;
				if (mi->mi_io_kstats) {
					mutex_enter(&mi->mi_lock);
					KSTAT_IO_PTR(mi->mi_io_kstats)->reads++;
					KSTAT_IO_PTR(mi->mi_io_kstats)->nread +=
					    res.resok.count;
					mutex_exit(&mi->mi_lock);
				}
				if (lwp != NULL)
					lwp->lwp_ru.inblock++;
			}
		}
	} while (!error && count && !res.resok.eof);

	*residp = count;

	if (!rpcerror) {
		if (!error) {
			if (res.resok.file_attributes.attributes) {
				/* fattr3_to_vattr return error if time ovf */
				error = fattr3_to_vattr(vp,
				    &res.resok.file_attributes.attr, &va);
				mutex_enter(&rp->r_statelock);
				if (error ||
				    !CACHE_VALID(rp, va.va_mtime, va.va_size)) {
					mutex_exit(&rp->r_statelock);
					PURGE_ATTRCACHE(vp);
				} else {
					seq = rp->r_seq;
					mutex_exit(&rp->r_statelock);
					nfs_attrcache_va(vp, &va, seq);
				}
			}
		}
	} else
		error = rpcerror;

	return (error);
}

/* ARGSUSED */
static int
nfs3_ioctl(vnode_t *vp, int cmd, intptr_t arg, int flag, cred_t *cr, int *rvalp)
{

	switch (cmd) {
		case _FIODIRECTIO:
			return (nfs_directio(vp, (int)arg, cr));
		default:
			return (ENOTTY);
	}
}

static int
nfs3_getattr(vnode_t *vp, struct vattr *vap, int flags, cred_t *cr)
{
	int error;
	rnode_t *rp = VTOR(vp);

	/*
	 * If it has been specified that the return value will
	 * just be used as a hint, and we are only being asked
	 * for size, fsid or rdevid, then return the client's
	 * notion of these values without checking to make sure
	 * that the attribute cache is up to date.
	 * The whole point is to avoid an over the wire GETATTR
	 * call.
	 */
	if (flags & ATTR_HINT) {
		if (vap->va_mask ==
		    (vap->va_mask & (AT_SIZE | AT_FSID | AT_RDEV))) {
			mutex_enter(&rp->r_statelock);
			if (vap->va_mask | AT_SIZE)
				vap->va_size = rp->r_size;
			if (vap->va_mask | AT_FSID)
				vap->va_fsid = rp->r_attr.va_fsid;
			if (vap->va_mask | AT_RDEV)
				vap->va_rdev = rp->r_attr.va_rdev;
			mutex_exit(&rp->r_statelock);
			return (0);
		}
	}

	/*
	 * Only need to flush pages if asking for the mtime
	 * and if there any dirty pages or any outstanding
	 * asynchronous (write) requests for this file.
	 */
	if (vap->va_mask & AT_MTIME) {
		rp = VTOR(vp);
		if (vp->v_pages != NULL &&
		    ((rp->r_flags & RDIRTY) || rp->r_awcount > 0)) {
			error = nfs3_putpage(vp, (u_offset_t)0, 0, 0, cr);
			if (error && (error == ENOSPC || error == EDQUOT)) {
				mutex_enter(&rp->r_statelock);
				if (!rp->r_error)
					rp->r_error = error;
				mutex_exit(&rp->r_statelock);
			}
		}
	}
	return (nfs3getattr(vp, vap, cr));
}

static int
nfs3_setattr(vnode_t *vp, struct vattr *vap, int flags, cred_t *cr)
{
	int error;
	uint_t mask;
	struct vattr va;

	mask = vap->va_mask;

	if (mask & AT_NOSET)
		return (EINVAL);

	va.va_mask = AT_UID | AT_MODE;
	error = nfs3getattr(vp, &va, cr);
	if (error)
		return (error);

	if (mask & (AT_UID | AT_GID)) {
		/*
		 * To change file ownership, a process must be the
		 * super-user if:
		 *
		 * If it is not the owner of the file, or
		 * if doing restricted chown semantics and
		 * either changing the ownership to someone else or
		 * changing the group to a group that we are not
		 * currently in.
		 */
		if (cr->cr_uid != va.va_uid ||
		    (rstchown &&
		    (((mask & AT_UID) && vap->va_uid != va.va_uid) ||
		    ((mask & AT_GID) && !groupmember(vap->va_gid, cr))))) {
			if (!suser(cr))
				return (EPERM);
		}
		/*
		 * In any case, clear the setuid and setgid bits on this
		 * file to eliminate a possible security problem.
		 */
		if (cr->cr_uid != 0) {
			if (!(mask & AT_MODE)) {
				vap->va_mask |= AT_MODE;
				vap->va_mode = va.va_mode;
			}
			vap->va_mode &= ~(VSUID | VSGID);
		}
	}
	if (mask & (AT_MTIME | AT_ATIME)) {
		/*
		 * To change either the access time or modified time
		 * on a file, a process must be either the owner of
		 * file, super-user, or have write permission on the
		 * file.  If it is neither the owner or super-user,
		 * then don't let it set the times to specific values
		 * as this may be used to mask security problems.
		 */
		if (cr->cr_uid != va.va_uid && cr->cr_uid != 0) {
			if (flags & ATTR_UTIME)
				return (EPERM);
			error = nfs3_access(vp, VWRITE, 0, cr);
			if (error)
				return (error);
		}
	}
	return (nfs3setattr(vp, vap, flags, cr));
}

static int
nfs3setattr(vnode_t *vp, struct vattr *vap, int flags, cred_t *cr)
{
	int error;
	uint_t mask;
	SETATTR3args args;
	SETATTR3res res;
	int douprintf;
	rnode_t *rp;
	struct vattr va;
	mode_t omode;
	vsecattr_t *vsp;

	mask = vap->va_mask;

	rp = VTOR(vp);

	/*
	 * Only need to flush pages if there are any pages and
	 * if the file is marked as dirty in some fashion.  The
	 * file must be flushed so that we can accurately
	 * determine the size of the file and the cached data
	 * after the SETATTR returns.  A file is considered to
	 * be dirty if it is either marked with RDIRTY, has
	 * outstanding i/o's active, or is mmap'd.  In this
	 * last case, we can't tell whether there are dirty
	 * pages, so we flush just to be sure.
	 */
	if (vp->v_pages != NULL &&
	    ((rp->r_flags & RDIRTY) ||
	    rp->r_count > 0 ||
	    rp->r_mapcnt > 0)) {
		ASSERT(vp->v_type != VCHR);
		error = nfs3_putpage(vp, (u_offset_t)0, 0, 0, cr);
		if (error && (error == ENOSPC || error == EDQUOT)) {
			mutex_enter(&rp->r_statelock);
			if (!rp->r_error)
				rp->r_error = error;
			mutex_exit(&rp->r_statelock);
		}
	}

	args.object = *RTOFH3(rp);
	/*
	 * If the intent is for the server to set the times,
	 * there is no point in have the mask indicating set mtime or
	 * atime, because the vap values may be junk, and so result
	 * in an overflow error. Remove these flags from the vap mask
	 * before calling in this case, and restore them afterwards.
	 */
	if ((mask & (AT_ATIME | AT_MTIME)) && !(flags & ATTR_UTIME)) {
		/* Use server times, so don't set the args time fields */
		vap->va_mask &= ~(AT_ATIME | AT_MTIME);
		error = vattr_to_sattr3(vap, &args.new_attributes);
		vap->va_mask |= (mask & (AT_ATIME | AT_MTIME));
		if (mask & AT_ATIME) {
			args.new_attributes.atime.set_it = SET_TO_SERVER_TIME;
		}
		if (mask & AT_MTIME) {
			args.new_attributes.mtime.set_it = SET_TO_SERVER_TIME;
		}
	} else {
		/* Either do not set times, or use client time */
		error = vattr_to_sattr3(vap, &args.new_attributes);
	}

	if (error) {
		/* req time field(s) overflow - return immediately */
		return (error);
	}

	va.va_mask = AT_MODE | AT_CTIME;
	error = nfs3getattr(vp, &va, cr);
	if (error)
		return (error);
	omode = va.va_mode;

tryagain:
	if (mask & AT_SIZE) {
		args.guard.check = TRUE;
		args.guard.obj_ctime.seconds = va.va_ctime.tv_sec;
		args.guard.obj_ctime.nseconds = va.va_ctime.tv_nsec;
	} else
		args.guard.check = FALSE;

	douprintf = 1;
	error = rfs3call(VTOMI(vp), NFSPROC3_SETATTR,
	    xdr_SETATTR3args, (caddr_t)&args,
	    xdr_SETATTR3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, NULL);

	/*
	 * Purge the access cache and ACL cache if changing either the
	 * owner of the file, the group owner, or the mode.  These may
	 * change the access permissions of the file, so purge old
	 * information and start over again.
	 */
	if (mask & (AT_UID | AT_GID | AT_MODE)) {
		(void) nfs_access_purge_rp(rp);
		if (rp->r_secattr != NULL) {
			mutex_enter(&rp->r_statelock);
			vsp = rp->r_secattr;
			rp->r_secattr = NULL;
			mutex_exit(&rp->r_statelock);
			if (vsp != NULL)
				nfs_acl_free(vsp);
		}
	}

	if (error) {
		PURGE_ATTRCACHE(vp);
		return (error);
	}

	error = geterrno3(res.status);
	if (!error) {
		/*
		 * If changing the size of the file, invalidate
		 * any local cached data which is no longer part
		 * of the file.  We also possibly invalidate the
		 * last page in the file.  We could use
		 * pvn_vpzero(), but this would mark the page as
		 * modified and require it to be written back to
		 * the server for no particularly good reason.
		 * This way, if we access it, then we bring it
		 * back in.  A read should be cheaper than a
		 * write.
		 */
		if (mask & AT_SIZE) {
			nfs_invalidate_pages(vp,
			    (vap->va_size & PAGEMASK), cr);
		}
		nfs3_cache_wcc_data(vp, &res.resok.obj_wcc, cr);
		/*
		 * Some servers will change the mode to clear the setuid
		 * and setgid bits when changing the uid or gid.  The
		 * client needs to compensate appropriately.
		 */
		if (mask & (AT_UID | AT_GID)) {
			int terror;

			va.va_mask = AT_MODE;
			terror = nfs3getattr(vp, &va, cr);
			if (!terror &&
			    (((mask & AT_MODE) && va.va_mode != vap->va_mode) ||
			    (!(mask & AT_MODE) && va.va_mode != omode))) {
				va.va_mask = AT_MODE;
				if (mask & AT_MODE)
					va.va_mode = vap->va_mode;
				else
					va.va_mode = omode;
				(void) nfs3setattr(vp, &va, 0, cr);
			}
		}
	} else {
		nfs3_cache_wcc_data(vp, &res.resfail.obj_wcc, cr);
		/*
		 * If we got back a "not synchronized" error, then
		 * we need to retry with a new guard value.  The
		 * guard value used is the change time.  If the
		 * server returned post_op_attr, then we can just
		 * retry because we have the latest attributes.
		 * Otherwise, we issue a GETATTR to get the latest
		 * attributes and then retry.  If we couldn't get
		 * the attributes this way either, then we give
		 * up because we can't complete the operation as
		 * required.
		 */
		if (res.status == NFS3ERR_NOT_SYNC) {
			va.va_mask = AT_CTIME;
			if (nfs3getattr(vp, &va, cr) == 0)
				goto tryagain;
		}
		PURGE_STALE_FH(error, vp, cr);
	}

	return (error);
}

/* ARGSUSED */
static int
nfs3_access(vnode_t *vp, int mode, int flags, cred_t *cr)
{
	int error;
	ACCESS3args args;
	ACCESS3res res;
	int douprintf;
	uint32 acc;
	rnode_t *rp;
	cred_t *cred;
	failinfo_t fi;
	nfs_access_type_t cacc;

	acc = 0;
	if (mode & VREAD)
		acc |= ACCESS3_READ;
	if (mode & VWRITE) {
		if ((vp->v_vfsp->vfs_flag & VFS_RDONLY) && !ISVDEV(vp->v_type))
			return (EROFS);
		if (vp->v_type == VDIR)
			acc |= ACCESS3_DELETE;
		acc |= ACCESS3_MODIFY | ACCESS3_EXTEND;
	}
	if (mode & VEXEC) {
		if (vp->v_type == VDIR)
			acc |= ACCESS3_LOOKUP;
		else
			acc |= ACCESS3_EXECUTE;
	}

	error = nfs3_validate_caches(vp, cr);
	if (error)
		return (error);

	rp = VTOR(vp);
	cacc = nfs_access_check(rp, acc, cr);
	if (cacc == NFS_ACCESS_ALLOWED)
		return (0);
	if (cacc == NFS_ACCESS_DENIED)
		return (EACCES);

	args.object = *VTOFH3(vp);
	args.access = acc;
	fi.vp = vp;
	fi.fhp = (caddr_t)&args.object;
	fi.copyproc = nfs3copyfh;
	fi.lookupproc = nfs3lookup;

	cred = cr;
tryagain:
	douprintf = 1;
	error = rfs3call(VTOMI(vp), NFSPROC3_ACCESS,
	    xdr_ACCESS3args, (caddr_t)&args,
	    xdr_ACCESS3res, (caddr_t)&res, cred,
	    &douprintf, &res.status, 0, &fi);

	if (error) {
		if (cred != cr)
			crfree(cred);
		return (error);
	}

	error = geterrno3(res.status);
	if (!error) {
		nfs3_cache_post_op_attr(vp, &res.resok.obj_attributes, cr);
		if ((args.access & res.resok.access) != args.access) {
			/*
			 * The following code implements the semantic that
			 * a setuid root program has *at least* the
			 * permissions of the user that is running the
			 * program.  See rfs3call() for more portions
			 * of the implementation of this functionality.
			 */
			if (cred->cr_uid == 0 && cred->cr_ruid != 0) {
				cred = crdup(cr);
				cred->cr_uid = cred->cr_ruid;
				goto tryagain;
			}
			error = EACCES;
		}
		nfs_access_cache(rp, args.access, res.resok.access, cr);
	} else {
		nfs3_cache_post_op_attr(vp, &res.resfail.obj_attributes, cr);
		PURGE_STALE_FH(error, vp, cr);
	}

	if (cred != cr)
		crfree(cred);

	return (error);
}

static int nfs3_do_symlink_cache = 1;

static int
nfs3_readlink(vnode_t *vp, struct uio *uiop, cred_t *cr)
{
	int error;
	READLINK3args args;
	READLINK3res res;
	rnode_t *rp;
	int douprintf;
	int len;
	failinfo_t fi;

	/*
	 * Can't readlink anything other than a symbolic link.
	 */
	if (vp->v_type != VLNK)
		return (EINVAL);

	rp = VTOR(vp);
	if (nfs3_do_symlink_cache && rp->r_symlink.contents != NULL) {
		error = nfs3_validate_caches(vp, cr);
		if (error)
			return (error);
		mutex_enter(&rp->r_statelock);
		if (rp->r_symlink.contents != NULL) {
			error = uiomove(rp->r_symlink.contents,
			    rp->r_symlink.len, UIO_READ, uiop);
			mutex_exit(&rp->r_statelock);
			return (error);
		}
		mutex_exit(&rp->r_statelock);
	}

	args.symlink = *VTOFH3(vp);
	fi.vp = vp;
	fi.fhp = (caddr_t)&args.symlink;
	fi.copyproc = nfs3copyfh;
	fi.lookupproc = nfs3lookup;
#ifdef DEBUG
	res.resok.data = symlink_cache_alloc(MAXPATHLEN, KM_SLEEP);
#else
	res.resok.data = kmem_alloc(MAXPATHLEN, KM_SLEEP);
#endif

	douprintf = 1;

	error = rfs3call(VTOMI(vp), NFSPROC3_READLINK,
	    xdr_nfs_fh3, (caddr_t)&args,
	    xdr_READLINK3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, &fi);

	if (error) {
#ifdef DEBUG
		symlink_cache_free((void *)res.resok.data, MAXPATHLEN);
#else
		kmem_free(res.resok.data, MAXPATHLEN);
#endif
		return (error);
	}

	error = geterrno3(res.status);
	if (!error) {
		nfs3_cache_post_op_attr(vp, &res.resok.symlink_attributes, cr);
		len = strlen(res.resok.data);
		error = uiomove(res.resok.data, len, UIO_READ, uiop);
		if (nfs3_do_symlink_cache && rp->r_symlink.contents == NULL) {
			mutex_enter(&rp->r_statelock);
			if (rp->r_symlink.contents == NULL) {
				rp->r_symlink.contents = res.resok.data;
				rp->r_symlink.len = len;
				rp->r_symlink.size = MAXPATHLEN;
				mutex_exit(&rp->r_statelock);
			} else {
				mutex_exit(&rp->r_statelock);
#ifdef DEBUG
				symlink_cache_free((void *)res.resok.data,
				    MAXPATHLEN);
#else
				kmem_free(res.resok.data, MAXPATHLEN);
#endif
			}
		} else {
#ifdef DEBUG
			symlink_cache_free((void *)res.resok.data, MAXPATHLEN);
#else
			kmem_free(res.resok.data, MAXPATHLEN);
#endif
		}
	} else {
		nfs3_cache_post_op_attr(vp,
		    &res.resfail.symlink_attributes, cr);
		PURGE_STALE_FH(error, vp, cr);
#ifdef DEBUG
		symlink_cache_free((void *)res.resok.data, MAXPATHLEN);
#else
		kmem_free(res.resok.data, MAXPATHLEN);
#endif
	}

	/*
	 * The over the wire error for attempting to readlink something
	 * other than a symbolic link is ENXIO.  However, we need to
	 * return EINVAL instead of ENXIO, so we map it here.
	 */
	return (error == ENXIO ? EINVAL : error);
}

/*
 * Flush local dirty pages to stable storage on the server.
 *
 * If FNODSYNC is specified, then there is nothing to do because
 * metadata changes are not cached on the client before being
 * sent to the server.
 */
static int
nfs3_fsync(vnode_t *vp, int syncflag, cred_t *cr)
{
	int error;

	if ((syncflag & FNODSYNC) || IS_SWAPVP(vp))
		return (0);
	error = nfs3_putpage_commit(vp, (u_offset_t)0, 0, cr);
	if (!error)
		error = VTOR(vp)->r_error;
	return (error);
}

/*
 * Weirdness: if the file was removed or the target of a rename
 * operation while it was open, it got renamed instead.  Here we
 * remove the renamed file.
 */
static void
nfs3_inactive(vnode_t *vp, cred_t *cr)
{
	rnode_t *rp;

	ASSERT(vp != &nfs3_notfound);

	rp = VTOR(vp);
redo:
	mutex_enter(&nfs_rtable_lock);
	if (rp->r_unldvp != NULL) {
		/*
		 * Save the vnode pointer for the directory where the
		 * unlinked-open file got renamed, then set it to NULL
		 * to prevent another thread from getting here before
		 * we're done with the remove.  While we have the
		 * statelock, make local copies of the pertinent rnode
		 * fields.  If we weren't to do this in an atomic way, the
		 * the unl* fields could become inconsistent with respect
		 * to each other due to a race condition between this
		 * code and nfs_remove().  See bug report 1034328.
		 */
		mutex_enter(&rp->r_statelock);
		if (rp->r_unldvp != NULL) {
			vnode_t *unldvp;
			char *unlname;
			cred_t *unlcred;
			REMOVE3args args;
			REMOVE3res res;
			int douprintf;
			int error;

			unldvp = rp->r_unldvp;
			rp->r_unldvp = NULL;
			unlname = rp->r_unlname;
			rp->r_unlname = NULL;
			unlcred = rp->r_unlcred;
			rp->r_unlcred = NULL;
			mutex_exit(&rp->r_statelock);
			mutex_exit(&nfs_rtable_lock);

			/*
			 * If there are any dirty pages left, then flush
			 * them.  This is unfortunate because they just
			 * may get thrown away during the remove operation,
			 * but we have to do this for correctness.
			 */
			if (vp->v_pages != NULL &&
			    ((rp->r_flags & RDIRTY) || rp->r_count > 0)) {
				ASSERT(vp->v_type != VCHR);
				error = nfs3_putpage(vp, (u_offset_t)0, 0,
				    0, cr);
				if (error) {
					mutex_enter(&rp->r_statelock);
					if (!rp->r_error)
						rp->r_error = error;
					mutex_exit(&rp->r_statelock);
				}
			}

			/*
			 * Do the remove operation on the renamed file
			 */
			setdiropargs3(&args.object, unlname, unldvp);
			douprintf = 1;
#if 0 /* notyet */
			/*
			 * Can't do this yet.  We may be being called from
			 * dnlc_purge_XXX while that routine is holding a
			 * mutex lock to the nc_rele list.  The calls to
			 * nfs3_cache_wcc_data may result in calls to
			 * dnlc_purge_XXX.  This will result in a deadlock.
			 */
			error = rfs3call(VTOMI(unldvp), NFSPROC3_REMOVE,
			    xdr_diropargs3, (caddr_t)&args,
			    xdr_REMOVE3res, (caddr_t)&res, unlcred,
			    &douprintf, &res.status, 0, NULL);

			if (error) {
				PURGE_ATTRCACHE(unldvp);
			} else {
				error = geterrno3(res.status);
				if (!error) {
					nfs3_cache_wcc_data(unldvp,
					    &res.resok.dir_wcc, cr);
					if (VTOR(unldvp)->r_dir != NULL)
						nfs_purge_rddir_cache(unldvp);
				} else {
					nfs3_cache_wcc_data(unldvp,
					    &res.resfail.dir_wcc, cr);
					PURGE_STALE_FH(error, unldvp, cr);
				}
			}
#else
			(void) rfs3call(VTOMI(unldvp), NFSPROC3_REMOVE,
			    xdr_diropargs3, (caddr_t)&args,
			    xdr_REMOVE3res, (caddr_t)&res, unlcred,
			    &douprintf, &res.status, 0, NULL);

			PURGE_ATTRCACHE(unldvp);
#endif

			/*
			 * Release stuff held for the remove
			 */
			VN_RELE(unldvp);
			kmem_free(unlname, MAXNAMELEN);
			crfree(unlcred);
			goto redo;
		}
		mutex_exit(&rp->r_statelock);
	}

	rp_addfree(rp, cr);
	mutex_exit(&nfs_rtable_lock);
}

/*
 * Remote file system operations having to do with directory manipulation.
 */

static int
nfs3_lookup(vnode_t *dvp, char *nm, vnode_t **vpp, struct pathname *pnp,
	int flags, vnode_t *rdir, cred_t *cr)
{
	int error;
	vnode_t *vp;
	rnode_t *drp;

	drp = VTOR(dvp);
	if (nfs_rw_enter_sig(&drp->r_rwlock, RW_READER, INTR(dvp)))
		return (EINTR);

	error = nfs3lookup(dvp, nm, vpp, pnp, flags, rdir, cr, 0);

	nfs_rw_exit(&drp->r_rwlock);

	/*
	 * If vnode is a device, create special vnode.
	 */
	if (!error && ISVDEV((*vpp)->v_type)) {
		vp = *vpp;
		*vpp = specvp(vp, vp->v_rdev, vp->v_type, cr);
		VN_RELE(vp);
	}

	return (error);
}

static int nfs3_lookup_neg_cache = 1;

#ifdef DEBUG
static int nfs3_lookup_dnlc_hits = 0;
static int nfs3_lookup_dnlc_misses = 0;
static int nfs3_lookup_dnlc_neg_hits = 0;
static int nfs3_lookup_dnlc_disappears = 0;
static int nfs3_lookup_dnlc_lookups = 0;
#endif

/* ARGSUSED */
int
nfs3lookup(vnode_t *dvp, char *nm, vnode_t **vpp, struct pathname *pnp,
	int flags, vnode_t *rdir, cred_t *cr, int rfscall_flags)
{
	int error;

	/*
	 * If lookup is for "", just return dvp.  Don't need
	 * to send it over the wire, look it up in the dnlc,
	 * or perform any access checks.
	 */
	if (*nm == '\0') {
		VN_HOLD(dvp);
		*vpp = dvp;
		return (0);
	}

	/*
	 * Can't do lookups in non-directories.
	 */
	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	/*
	 * If we're called with RFSCALL_SOFT, it's important that
	 * the only rfscall is one we make directly; if we permit
	 * an access call because we're looking up "." or validating
	 * a dnlc hit, we'll deadlock because that rfscall will not
	 * have the RFSCALL_SOFT set.
	 */
	if (rfscall_flags & RFSCALL_SOFT)
		goto callit;

	/*
	 * If lookup is for ".", just return dvp.  Don't need
	 * to send it over the wire or look it up in the dnlc,
	 * just need to check access.
	 */
	if (strcmp(nm, ".") == 0) {
		error = nfs3_access(dvp, VEXEC, 0, cr);
		if (error)
			return (error);
		VN_HOLD(dvp);
		*vpp = dvp;
		return (0);
	}

	/*
	 * Lookup this name in the DNLC.  If there was a valid entry,
	 * then return the results of the lookup.
	 */
	error = nfs3lookup_dnlc(dvp, nm, vpp, cr);
	if (error || *vpp != NULL)
		return (error);

callit:
	error = nfs3lookup_otw(dvp, nm, vpp, cr, rfscall_flags);

	return (error);
}

static int
nfs3lookup_dnlc(vnode_t *dvp, char *nm, vnode_t **vpp, cred_t *cr)
{
	int error;
	vnode_t *vp;

	ASSERT(*nm != '\0');
	ASSERT(dvp->v_type == VDIR);

	/*
	 * Lookup this name in the DNLC.  If successful, then check
	 * access and then recheck the DNLC.  The DNLC is rechecked
	 * just in case this entry got invalidated during the call
	 * to nfs3_access.
	 *
	 * The assumption is made that nfs3_access invokes
	 * nfs3_validate_caches to make sure the access cache is valid.
	 * If the attributes were timed out, an ACCESS call is made to
	 * the server which returns new attributes.  When this happens,
	 * the attribute cache is updated and the DNLC will be purged
	 * if appropriate.
	 *
	 * Another assumption that is being made is that it is safe
	 * to say that a file exists which may not on the server.
	 * Any operations to the server will fail with ESTALE.
	 */
#ifdef DEBUG
	nfs3_lookup_dnlc_lookups++;
#endif
	vp = dnlc_lookup(dvp, nm, NOCRED);
	if (vp != NULL) {
		VN_RELE(vp);
		if (vp == &nfs3_notfound) {
			if (!(dvp->v_vfsp->vfs_flag & VFS_RDONLY))
				PURGE_ATTRCACHE(dvp);
			error = nfs3_validate_caches(dvp, cr);
			if (error)
				return (error);
		}
		error = nfs3_access(dvp, VEXEC, 0, cr);
		if (error)
			return (error);
		vp = dnlc_lookup(dvp, nm, NOCRED);
		if (vp != NULL) {
			if (vp == &nfs3_notfound) {
				VN_RELE(vp);
#ifdef DEBUG
				nfs3_lookup_dnlc_neg_hits++;
#endif
				return (ENOENT);
			}
			*vpp = vp;
#ifdef DEBUG
			nfs3_lookup_dnlc_hits++;
#endif
			return (0);
		}
#ifdef DEBUG
		nfs3_lookup_dnlc_disappears++;
#endif
	}
#ifdef DEBUG
	else
		nfs3_lookup_dnlc_misses++;
#endif

	*vpp = NULL;

	return (0);
}

static int
nfs3lookup_otw(vnode_t *dvp, char *nm, vnode_t **vpp, cred_t *cr,
	int rfscall_flags)
{
	int error;
	LOOKUP3args args;
	LOOKUP3res res;
	int douprintf;
	struct vattr vattr;
	vnode_t *vp;
	failinfo_t fi;

	ASSERT(*nm != '\0');
	ASSERT(dvp->v_type == VDIR);

	setdiropargs3(&args.what, nm, dvp);

	fi.vp = dvp;
	fi.fhp = (caddr_t)&args.what.dir;
	fi.copyproc = nfs3copyfh;
	fi.lookupproc = nfs3lookup;

	douprintf = 1;

	error = rfs3call(VTOMI(dvp), NFSPROC3_LOOKUP,
	    xdr_diropargs3, (caddr_t)&args,
	    xdr_LOOKUP3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, rfscall_flags, &fi);

	if (error)
		return (error);

	error = geterrno3(res.status);
	if (!error) {
		nfs3_cache_post_op_attr(dvp, &res.resok.dir_attributes, cr);
		if (res.resok.obj_attributes.attributes) {
			vp = makenfs3node(&res.resok.object,
			    &res.resok.obj_attributes.attr,
			    dvp->v_vfsp, cr, VTOR(dvp)->r_path, nm);
		} else {
			vp = makenfs3node(&res.resok.object, NULL,
			    dvp->v_vfsp, cr, VTOR(dvp)->r_path, nm);
			if (vp->v_type == VNON) {
				vattr.va_mask = AT_TYPE;
				error = nfs3getattr(vp, &vattr, cr);
				if (error) {
					VN_RELE(vp);
					return (error);
				}
				vp->v_type = vattr.va_type;
			}
		}
		if (!(rfscall_flags & RFSCALL_SOFT))
			dnlc_update(dvp, nm, vp, NOCRED);
		*vpp = vp;
	} else {
		nfs3_cache_post_op_attr(dvp, &res.resfail.dir_attributes, cr);
		PURGE_STALE_FH(error, dvp, cr);
		if (error == ENOENT && nfs3_lookup_neg_cache)
			dnlc_enter(dvp, nm, &nfs3_notfound, NOCRED);
	}

	return (error);
}

#ifdef DEBUG
static int nfs3_create_misses = 0;
#endif

/* ARGSUSED */
static int
nfs3_create(vnode_t *dvp, char *nm, struct vattr *va, enum vcexcl exclusive,
	int mode, vnode_t **vpp, cred_t *cr, int lfaware)
{
	int error;
	vnode_t *vp;
	rnode_t *rp;
	struct vattr vattr;
	rnode_t *drp;
	vnode_t *tempvp;

	drp = VTOR(dvp);

	if (nfs_rw_enter_sig(&drp->r_rwlock, RW_WRITER, INTR(dvp)))
		return (EINTR);

top:
	/*
	 * We make a copy of the attributes because the caller does not
	 * expect us to change what va points to.
	 */
	vattr = *va;

	/*
	 * If the pathname is "", just use dvp.  Don't need
	 * to send it over the wire, look it up in the dnlc,
	 * or perform any access checks.
	 */
	if (*nm == '\0') {
		error = 0;
		VN_HOLD(dvp);
		vp = dvp;
	/*
	 * If the pathname is ".", just use dvp.  Don't need
	 * to send it over the wire or look it up in the dnlc,
	 * just need to check access.
	 */
	} else if (strcmp(nm, ".") == 0) {
		error = nfs3_access(dvp, VEXEC, 0, cr);
		if (error) {
			nfs_rw_exit(&drp->r_rwlock);
			return (error);
		}
		VN_HOLD(dvp);
		vp = dvp;
	/*
	 * We need to go over the wire, just to be sure whether the
	 * file exists or not.  Using the DNLC can be dangerous in
	 * this case when making a decision regarding existence.
	 */
	} else {
		error = nfs3lookup_otw(dvp, nm, &vp, cr, 0);
	}
	if (!error) {
		if (exclusive == EXCL)
			error = EEXIST;
		else if (vp->v_type == VDIR && (mode & VWRITE))
			error = EISDIR;
		else {
			/*
			 * If vnode is a device, create special vnode.
			 */
			if (ISVDEV(vp->v_type)) {
				tempvp = vp;
				vp = specvp(vp, vp->v_rdev, vp->v_type, cr);
				VN_RELE(tempvp);
			}
			if (!(error = VOP_ACCESS(vp, mode, 0, cr))) {
				if ((vattr.va_mask & AT_SIZE) &&
				    vp->v_type == VREG) {
					rp = VTOR(vp);
					/*
					 * Check here for large file handled
					 * by LF-unaware process (as
					 * ufs_create() does)
					 */
					if (!(lfaware & FOFFMAX)) {
						mutex_enter(&rp->r_statelock);
						if (rp->r_size > MAXOFF32_T)
							error = EOVERFLOW;
						mutex_exit(&rp->r_statelock);
					}
					if (!error) {
						vattr.va_mask = AT_SIZE;
						error = nfs3setattr(vp,
						    &vattr, 0, cr);
					}
				}
			}
		}
		nfs_rw_exit(&drp->r_rwlock);
		if (error) {
			VN_RELE(vp);
		} else
			*vpp = vp;
		return (error);
	}

	dnlc_remove(dvp, nm);

	/*
	 * Decide what the group-id of the created file should be.
	 * Set it in attribute list as advisory...
	 */
	error = setdirgid(dvp, &vattr.va_gid, cr);
	if (error)
		return (error);
	vattr.va_mask |= AT_GID;

	ASSERT(vattr.va_mask & AT_TYPE);
	if (vattr.va_type == VREG) {
		ASSERT(vattr.va_mask & AT_MODE);
		if (MANDMODE(vattr.va_mode)) {
			nfs_rw_exit(&drp->r_rwlock);
			return (EACCES);
		}
		error = nfs3create(dvp, nm, &vattr, exclusive, mode, vpp, cr,
		    lfaware);
		/*
		 * If this is not an exclusive create, then the CREATE
		 * request will be made with the GUARDED mode set.  This
		 * means that the server will return EEXIST if the file
		 * exists.  The file could exist because of a retransmitted
		 * request.  In this case, we recover by starting over and
		 * checking to see whether the file exists.  This second
		 * time through it should and a CREATE request will not be
		 * sent.
		 *
		 * This handles the problem of a dangling CREATE request
		 * which contains attributes which indicate that the file
		 * should be truncated.  This retransmitted request could
		 * possibly truncate valid data in the file if not caught
		 * by the duplicate request mechanism on the server or if
		 * not caught by other means.  The scenario is:
		 *
		 * Client transmits CREATE request with size = 0
		 * Client times out, retransmits request.
		 * Response to the first request arrives from the server
		 *  and the client proceeds on.
		 * Client writes data to the file.
		 * The server now processes retransmitted CREATE request
		 *  and truncates file.
		 *
		 * The use of the GUARDED CREATE request prevents this from
		 * happening because the retransmitted CREATE would fail
		 * with EEXIST and would not truncate the file.
		 */
		if (error == EEXIST && exclusive == NONEXCL) {
#ifdef DEBUG
			nfs3_create_misses++;
#endif
			goto top;
		}
		nfs_rw_exit(&drp->r_rwlock);
		return (error);
	}
	error = nfs3mknod(dvp, nm, &vattr, exclusive, mode, vpp, cr);
	nfs_rw_exit(&drp->r_rwlock);
	return (error);
}

/* ARGSUSED */
static int
nfs3create(vnode_t *dvp, char *nm, struct vattr *va, enum vcexcl exclusive,
	int mode, vnode_t **vpp, cred_t *cr, int lfaware)
{
	int error;
	CREATE3args args;
	CREATE3res res;
	int douprintf;
	vnode_t *vp;
	struct vattr vattr;
	nfstime3 *verfp;
	rnode_t *rp;

	setdiropargs3(&args.where, nm, dvp);
	if (exclusive == EXCL) {
		args.how.mode = EXCLUSIVE;
		/*
		 * Construct the create verifier.  This verifier needs
		 * to be unique between different clients.  It also needs
		 * to vary for each exclusive create request generated
		 * from the client to the server.
		 *
		 * The first attempt is made to use the hostid and a
		 * unique number on the client.  If the hostid has not
		 * been set, the high resolution time that the exclusive
		 * create request is being made is used.  This will work
		 * unless two different clients, both with the hostid
		 * not set, attempt an exclusive create request on the
		 * same file, at exactly the same clock time.  The
		 * chances of this happening seem small enough to be
		 * reasonable.
		 */
		verfp = (nfstime3 *)&args.how.createhow3_u.verf;
		verfp->seconds = nfs_atoi(hw_serial);
		if (verfp->seconds != 0)
			verfp->nseconds = newnum();
		else {
			verfp->seconds = hrestime.tv_sec;
			verfp->nseconds = hrestime.tv_nsec;
		}
		/*
		 * Since the server will use this value for the mtime,
		 * make sure that it can't overflow. Zero out the MSB.
		 * The actual value does not matter here, only its uniqeness.
		 */
		verfp->seconds %= INT32_MAX;
	} else {
		/*
		 * Issue the non-exclusive create in guarded mode.  This
		 * may result in some false EEXIST responses for
		 * retransmitted requests, but these will be handled at
		 * a higher level.  By using GUARDED, duplicate requests
		 * to do file truncation and possible access problems
		 * can be avoided.
		 */
		args.how.mode = GUARDED;
		error = vattr_to_sattr3(va,
				&args.how.createhow3_u.obj_attributes);
		if (error) {
			/* req time field(s) overflow - return immediately */
			return (error);
		}
	}

	douprintf = 1;
	error = rfs3call(VTOMI(dvp), NFSPROC3_CREATE,
	    xdr_CREATE3args, (caddr_t)&args,
	    xdr_CREATE3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, NULL);

	if (error) {
		PURGE_ATTRCACHE(dvp);
		return (error);
	}

	error = geterrno3(res.status);
	if (!error) {
		nfs3_cache_wcc_data(dvp, &res.resok.dir_wcc, cr);
		if (VTOR(dvp)->r_dir != NULL)
			nfs_purge_rddir_cache(dvp);

		/*
		 * On exclusive create the times need to be explicitly
		 * set to clear any potential verifier that may be stored
		 * in one of these fields (see comment below).  This
		 * is done here to cover the case where no post op attrs
		 * were returned or a 'invalid' time was returned in
		 * the attributes.
		 */
		if (exclusive == EXCL) {
			va->va_mask |= (AT_MTIME | AT_ATIME);
		}

		if (!res.resok.obj.handle_follows) {
			error = nfs3lookup(dvp, nm, &vp, NULL, 0, NULL, cr, 0);
			if (error)
				return (error);
		} else {
			if (res.resok.obj_attributes.attributes) {
				vp = makenfs3node(&res.resok.obj.handle,
				    &res.resok.obj_attributes.attr,
				    dvp->v_vfsp, cr, NULL, NULL);
			} else {
				vp = makenfs3node(&res.resok.obj.handle, NULL,
				    dvp->v_vfsp, cr, NULL, NULL);

				/*
				 * On an exclusive create, it is possible
				 * that attributes were returned but those
				 * postop attributes failed to decode
				 * properly.  If this is the case,
				 * then most likely the atime or mtime
				 * were invalid for our client; this
				 * is caused by the server storing the
				 * create verifier in one of the time
				 * fields(most likely mtime).
				 * So... we are going to setattr just the
				 * atime/mtime to clear things up.
				 */
				if (exclusive == EXCL) {
					if (error =
						nfs3excl_create_settimes(vp,
							va, cr)) {
						/*
						 * Setting the times failed.
						 * Remove the file and return
						 * the error.
						 */
						VN_RELE(vp);
						(void) nfs3_remove(dvp,
							nm, cr);
						return (error);
					}
				}

				/*
				 * This handles the non-exclusive case
				 * and the exclusive case where no post op
				 * attrs were returned.
				 */
				if (vp->v_type == VNON) {
					vattr.va_mask = AT_TYPE;
					error = nfs3getattr(vp, &vattr, cr);
					if (error) {
						VN_RELE(vp);
						return (error);
					}
					vp->v_type = vattr.va_type;
				}
			}
			dnlc_update(dvp, nm, vp, NOCRED);
		}

		rp = VTOR(vp);

		/*
		 * Check here for large file handled by
		 * LF-unaware process (as ufs_create() does)
		 */
		if ((va->va_mask & AT_SIZE) && vp->v_type == VREG &&
		    !(lfaware & FOFFMAX)) {
			mutex_enter(&rp->r_statelock);
			if (rp->r_size > MAXOFF32_T) {
				mutex_exit(&rp->r_statelock);
				VN_RELE(vp);
				return (EOVERFLOW);
			}
			mutex_exit(&rp->r_statelock);
		}

		if (exclusive == EXCL &&
			(va->va_mask & ~(AT_GID | AT_SIZE))) {
			/*
			 * If doing an exclusive create, then generate
			 * a SETATTR to set the initial attributes.
			 * Try to set the mtime and the atime to the
			 * server's current time.  It is somewhat
			 * expected that these fields will be used to
			 * store the exclusive create cookie.  If not,
			 * server implementors will need to know that
			 * a SETATTR will follow an exclusive create
			 * and the cookie should be destroyed if
			 * appropriate. This work may have been done
			 * earlier in this function if post op attrs
			 * were not available.
			 *
			 * The AT_GID and AT_SIZE bits are turned off
			 * so that the SETATTR request will not attempt
			 * to process these.  The gid will be set
			 * separately if appropriate.  The size is turned
			 * off because it is assumed that a new file will
			 * be created empty and if the file wasn't empty,
			 * then the exclusive create will have failed
			 * because the file must have existed already.
			 * Therefore, no truncate operation is needed.
			 */
			va->va_mask &= ~(AT_GID | AT_SIZE);
			error = nfs3setattr(vp, va, 0, cr);
			if (error) {
				/*
				 * Couldn't correct the attributes of
				 * the newly created file and the
				 * attributes are wrong.  Remove the
				 * file and return an error to the
				 * application.
				 */
				VN_RELE(vp);
				(void) nfs3_remove(dvp, nm, cr);
				return (error);
			}
		}

		if (va->va_gid != rp->r_attr.va_gid) {
			/*
			 * If the gid on the file isn't right, then
			 * generate a SETATTR to attempt to change
			 * it.  This may or may not work, depending
			 * upon the server's semantics for allowing
			 * file ownership changes.
			 */
			va->va_mask = AT_GID;
			(void) nfs3setattr(vp, va, 0, cr);
		}

		/*
		 * If vnode is a device create special vnode
		 */
		if (ISVDEV(vp->v_type)) {
			*vpp = specvp(vp, vp->v_rdev, vp->v_type, cr);
			VN_RELE(vp);
		} else
			*vpp = vp;
	} else {
		nfs3_cache_wcc_data(dvp, &res.resfail.dir_wcc, cr);
		PURGE_STALE_FH(error, dvp, cr);
	}

	return (error);
}

/*
 * Special setattr function to take care of rest of atime/mtime
 * after successful exclusive create.  This function exists to avoid
 * handling attributes from the server; exclusive the atime/mtime fields
 * may be 'invalid' in client's view and therefore can not be trusted.
 */
static int
nfs3excl_create_settimes(vnode_t *vp, struct vattr *vap, cred_t *cr)
{
	int error;
	uint_t mask;
	SETATTR3args args;
	SETATTR3res res;
	int douprintf;
	rnode_t *rp;

	/* save the caller's mask so that it can be reset later */
	mask = vap->va_mask;

	rp = VTOR(vp);

	args.object = *RTOFH3(rp);
	args.guard.check = FALSE;

	/* Use the mask to initialize the arguments */
	vap->va_mask = 0;
	error = vattr_to_sattr3(vap, &args.new_attributes);

	/* We want to set just atime/mtime on this request */
	args.new_attributes.atime.set_it = SET_TO_SERVER_TIME;
	args.new_attributes.mtime.set_it = SET_TO_SERVER_TIME;

	douprintf = 1;
	error = rfs3call(VTOMI(vp), NFSPROC3_SETATTR,
	    xdr_SETATTR3args, (caddr_t)&args,
	    xdr_SETATTR3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, NULL);

	if (error) {
		vap->va_mask = mask;
		return (error);
	}

	error = geterrno3(res.status);
	if (!error) {
		/*
		 * It is important to pick up the attributes.
		 * Since this is the exclusive create path, the
		 * attributes on the initial create were ignored
		 * and we need these to have the correct info.
		 */
		nfs3_cache_wcc_data(vp, &res.resok.obj_wcc, cr);
		/*
		 * No need to do the atime/mtime work again so clear
		 * the bits.
		 */
		mask &= ~(AT_ATIME | AT_MTIME);
	} else {
		nfs3_cache_wcc_data(vp, &res.resfail.obj_wcc, cr);
	}

	vap->va_mask = mask;

	return (error);
}

/* ARGSUSED */
static int
nfs3mknod(vnode_t *dvp, char *nm, struct vattr *va, enum vcexcl exclusive,
	int mode, vnode_t **vpp, cred_t *cr)
{
	int error;
	MKNOD3args args;
	MKNOD3res res;
	int douprintf;
	vnode_t *vp;
	struct vattr vattr;

	switch (va->va_type) {
	case VCHR:
	case VBLK:
		setdiropargs3(&args.where, nm, dvp);
		args.what.type = (va->va_type == VCHR) ? NF3CHR : NF3BLK;
		error = vattr_to_sattr3(va,
		    &args.what.mknoddata3_u.device.dev_attributes);
		if (error) {
			/* req time field(s) overflow - return immediately */
			return (error);
		}
		args.what.mknoddata3_u.device.spec.specdata1 =
		    getmajor(va->va_rdev);
		args.what.mknoddata3_u.device.spec.specdata2 =
		    getminor(va->va_rdev);
		break;

	case VFIFO:
	case VSOCK:
		setdiropargs3(&args.where, nm, dvp);
		args.what.type = (va->va_type == VFIFO) ? NF3FIFO : NF3SOCK;
		error = vattr_to_sattr3(va,
				&args.what.mknoddata3_u.pipe_attributes);
		if (error) {
			/* req time field(s) overflow - return immediately */
			return (error);
		}
		break;

	default:
		return (EINVAL);
	}

	douprintf = 1;
	error = rfs3call(VTOMI(dvp), NFSPROC3_MKNOD,
	    xdr_MKNOD3args, (caddr_t)&args,
	    xdr_MKNOD3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, NULL);

	if (error) {
		PURGE_ATTRCACHE(dvp);
		return (error);
	}

	error = geterrno3(res.status);
	if (!error) {
		nfs3_cache_wcc_data(dvp, &res.resok.dir_wcc, cr);
		if (VTOR(dvp)->r_dir != NULL)
			nfs_purge_rddir_cache(dvp);

		if (!res.resok.obj.handle_follows) {
			error = nfs3lookup(dvp, nm, &vp, NULL, 0, NULL, cr, 0);
			if (error)
				return (error);
		} else {
			if (res.resok.obj_attributes.attributes) {
				vp = makenfs3node(&res.resok.obj.handle,
				    &res.resok.obj_attributes.attr,
				    dvp->v_vfsp, cr, NULL, NULL);
			} else {
				vp = makenfs3node(&res.resok.obj.handle, NULL,
				    dvp->v_vfsp, cr, NULL, NULL);
				if (vp->v_type == VNON) {
					vattr.va_mask = AT_TYPE;
					error = nfs3getattr(vp, &vattr, cr);
					if (error) {
						VN_RELE(vp);
						return (error);
					}
					vp->v_type = vattr.va_type;
				}

			}
			dnlc_update(dvp, nm, vp, NOCRED);
		}

		if (va->va_gid != VTOR(vp)->r_attr.va_gid) {
			va->va_mask = AT_GID;
			(void) nfs3setattr(vp, va, 0, cr);
		}

		/*
		 * If vnode is a device create special vnode
		 */
		if (ISVDEV(vp->v_type)) {
			*vpp = specvp(vp, vp->v_rdev, vp->v_type, cr);
			VN_RELE(vp);
		} else
			*vpp = vp;
	} else {
		nfs3_cache_wcc_data(dvp, &res.resfail.dir_wcc, cr);
		PURGE_STALE_FH(error, dvp, cr);
	}
	return (error);
}

/*
 * Weirdness: if the vnode to be removed is open
 * we rename it instead of removing it and nfs_inactive
 * will remove the new name.
 */
static int
nfs3_remove(vnode_t *dvp, char *nm, cred_t *cr)
{
	int error;
	REMOVE3args args;
	REMOVE3res res;
	vnode_t *vp;
	char *tmpname;
	int douprintf;
	rnode_t *rp;
	rnode_t *drp;

	drp = VTOR(dvp);
	if (nfs_rw_enter_sig(&drp->r_rwlock, RW_WRITER, INTR(dvp)))
		return (EINTR);

	error = nfs3lookup(dvp, nm, &vp, NULL, 0, NULL, cr, 0);
	if (error) {
		nfs_rw_exit(&drp->r_rwlock);
		return (error);
	}

	if (vp->v_type == VDIR && !suser(cr)) {
		VN_RELE(vp);
		nfs_rw_exit(&drp->r_rwlock);
		return (EPERM);
	}

	/*
	 * First just remove the entry from the name cache, as it
	 * is most likely the only entry for this vp.
	 */
	dnlc_remove(dvp, nm);

	/*
	 * If the file has a v_count > 1 then there may be more than one
	 * entry in the name cache due multiple links or an open file,
	 * but we don't have the real reference count so flush all
	 * possible entries.
	 */
	if (vp->v_count > 1)
		dnlc_purge_vp(vp);

	/*
	 * Now we have the real reference count on the vnode
	 */
	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	if (vp->v_count > 1 &&
	    (rp->r_unldvp == NULL || strcmp(nm, rp->r_unlname) == 0)) {
		mutex_exit(&rp->r_statelock);
		tmpname = newname();
		error = nfs3rename(dvp, nm, dvp, tmpname, cr);
		if (error)
			kmem_free(tmpname, MAXNAMELEN);
		else {
			mutex_enter(&rp->r_statelock);
			if (rp->r_unldvp == NULL) {
				VN_HOLD(dvp);
				rp->r_unldvp = dvp;
				if (rp->r_unlcred != NULL)
					crfree(rp->r_unlcred);
				crhold(cr);
				rp->r_unlcred = cr;
				rp->r_unlname = tmpname;
			} else {
				kmem_free(rp->r_unlname, MAXNAMELEN);
				rp->r_unlname = tmpname;
			}
			mutex_exit(&rp->r_statelock);
		}
	} else {
		mutex_exit(&rp->r_statelock);
		/*
		 * We need to flush any dirty pages which happen to
		 * be hanging around before removing the file.  This
		 * shouldn't happen very often and mostly on file
		 * systems mounted "nocto".
		 */
		if (vp->v_pages != NULL &&
		    ((rp->r_flags & RDIRTY) || rp->r_count > 0)) {
			error = nfs3_putpage(vp, (u_offset_t)0, 0, 0, cr);
			if (error && (error == ENOSPC || error == EDQUOT)) {
				mutex_enter(&rp->r_statelock);
				if (!rp->r_error)
					rp->r_error = error;
				mutex_exit(&rp->r_statelock);
			}
		}

		setdiropargs3(&args.object, nm, dvp);

		douprintf = 1;
		error = rfs3call(VTOMI(dvp), NFSPROC3_REMOVE,
		    xdr_diropargs3, (caddr_t)&args,
		    xdr_REMOVE3res, (caddr_t)&res, cr,
		    &douprintf, &res.status, 0, NULL);

		PURGE_ATTRCACHE(vp);

		if (error) {
			PURGE_ATTRCACHE(dvp);
		} else {
			error = geterrno3(res.status);
			if (!error) {
				nfs3_cache_wcc_data(dvp, &res.resok.dir_wcc,
				    cr);
				if (drp->r_dir != NULL)
					nfs_purge_rddir_cache(dvp);
			} else {
				nfs3_cache_wcc_data(dvp, &res.resfail.dir_wcc,
				    cr);
				PURGE_STALE_FH(error, dvp, cr);
			}
		}
	}

	VN_RELE(vp);

	nfs_rw_exit(&drp->r_rwlock);

	return (error);
}

static int
nfs3_link(vnode_t *tdvp, vnode_t *svp, char *tnm, cred_t *cr)
{
	int error;
	LINK3args args;
	LINK3res res;
	vnode_t *realvp;
	int douprintf;
	mntinfo_t *mi;
	rnode_t *tdrp;

	if (VOP_REALVP(svp, &realvp) == 0)
		svp = realvp;

	mi = VTOMI(svp);

	if (!(mi->mi_flags & MI_LINK))
		return (EOPNOTSUPP);

	args.file = *VTOFH3(svp);
	setdiropargs3(&args.link, tnm, tdvp);

	tdrp = VTOR(tdvp);
	if (nfs_rw_enter_sig(&tdrp->r_rwlock, RW_WRITER, INTR(tdvp)))
		return (EINTR);

	dnlc_remove(tdvp, tnm);

	douprintf = 1;
	error = rfs3call(mi, NFSPROC3_LINK,
	    xdr_LINK3args, (caddr_t)&args,
	    xdr_LINK3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, NULL);

	if (error) {
		PURGE_ATTRCACHE(tdvp);
		PURGE_ATTRCACHE(svp);
		nfs_rw_exit(&tdrp->r_rwlock);
		return (error);
	}

	error = geterrno3(res.status);

	if (!error) {
		nfs3_cache_post_op_attr(svp, &res.resok.file_attributes, cr);
		nfs3_cache_wcc_data(tdvp, &res.resok.linkdir_wcc, cr);
		if (tdrp->r_dir != NULL)
			nfs_purge_rddir_cache(tdvp);
		dnlc_update(tdvp, tnm, svp, NOCRED);
	} else {
		nfs3_cache_post_op_attr(svp, &res.resfail.file_attributes, cr);
		nfs3_cache_wcc_data(tdvp, &res.resfail.linkdir_wcc, cr);
		if (error == EOPNOTSUPP) {
			mutex_enter(&mi->mi_lock);
			mi->mi_flags &= ~MI_LINK;
			mutex_exit(&mi->mi_lock);
		}
	}

	nfs_rw_exit(&tdrp->r_rwlock);

	return (error);
}

static int
nfs3_rename(vnode_t *odvp, char *onm, vnode_t *ndvp, char *nnm, cred_t *cr)
{
	vnode_t *realvp;

	if (VOP_REALVP(ndvp, &realvp) == 0)
		ndvp = realvp;

	return (nfs3rename(odvp, onm, ndvp, nnm, cr));
}

/*
 * nfs3rename does the real work of renaming in NFS Version 3.
 */
static int
nfs3rename(vnode_t *odvp, char *onm, vnode_t *ndvp, char *nnm, cred_t *cr)
{
	int error;
	RENAME3args args;
	RENAME3res res;
	int douprintf;
	vnode_t *nvp;
	vnode_t *ovp = NULL;
	char *tmpname;
	rnode_t *rp;
	rnode_t *odrp;
	rnode_t *ndrp;

	if (strcmp(onm, ".") == 0 || strcmp(onm, "..") == 0 ||
	    strcmp(nnm, ".") == 0 || strcmp(nnm, "..") == 0)
		return (EINVAL);

	odrp = VTOR(odvp);
	ndrp = VTOR(ndvp);
	if ((intptr_t)odrp < (intptr_t)ndrp) {
		if (nfs_rw_enter_sig(&odrp->r_rwlock, RW_WRITER, INTR(odvp)))
			return (EINTR);
		if (nfs_rw_enter_sig(&ndrp->r_rwlock, RW_WRITER, INTR(ndvp))) {
			nfs_rw_exit(&odrp->r_rwlock);
			return (EINTR);
		}
	} else {
		if (nfs_rw_enter_sig(&ndrp->r_rwlock, RW_WRITER, INTR(ndvp)))
			return (EINTR);
		if (nfs_rw_enter_sig(&odrp->r_rwlock, RW_WRITER, INTR(odvp))) {
			nfs_rw_exit(&ndrp->r_rwlock);
			return (EINTR);
		}
	}

	/*
	 * Lookup the target file.  If it exists, it needs to be
	 * checked to see whether it is a mount point and whether
	 * it is active (open).
	 */
	error = nfs3lookup(ndvp, nnm, &nvp, NULL, 0, NULL, cr, 0);
	if (!error) {
		/*
		 * If this file has been mounted on, then just
		 * return busy because renaming to it would remove
		 * the mounted file system from the name space.
		 */
		if (nvp->v_vfsmountedhere != NULL) {
			VN_RELE(nvp);
			nfs_rw_exit(&odrp->r_rwlock);
			nfs_rw_exit(&ndrp->r_rwlock);
			return (EBUSY);
		}

		/*
		 * Purge the name cache of all references to this vnode
		 * so that we can check the reference count to infer
		 * whether it is active or not.
		 */
		dnlc_purge_vp(nvp);

		/*
		 * If the vnode is active and is not a directory,
		 * arrange to rename it to a
		 * temporary file so that it will continue to be
		 * accessible.  This implements the "unlink-open-file"
		 * semantics for the target of a rename operation.
		 * Before doing this though, make sure that the
		 * source and target files are not already the same.
		 */
		if (nvp->v_count > 1 && nvp->v_type != VDIR) {
			/*
			 * Lookup the source name.
			 */
			error = nfs3lookup(odvp, onm, &ovp, NULL, 0, NULL,
			    cr, 0);

			/*
			 * The source name *should* already exist.
			 */
			if (error) {
				VN_RELE(nvp);
				nfs_rw_exit(&odrp->r_rwlock);
				nfs_rw_exit(&ndrp->r_rwlock);
				return (error);
			}

			/*
			 * Compare the two vnodes.  If they are the same,
			 * just release all held vnodes and return success.
			 */
			if (ovp == nvp) {
				VN_RELE(ovp);
				VN_RELE(nvp);
				nfs_rw_exit(&odrp->r_rwlock);
				nfs_rw_exit(&ndrp->r_rwlock);
				return (0);
			}

			/*
			 * Can't mix and match directories and non-
			 * directories in rename operations.  We already
			 * know that the target is not a directory.  If
			 * the source is a directory, return an error.
			 */
			if (ovp->v_type == VDIR) {
				VN_RELE(ovp);
				VN_RELE(nvp);
				nfs_rw_exit(&odrp->r_rwlock);
				nfs_rw_exit(&ndrp->r_rwlock);
				return (ENOTDIR);
			}

			/*
			 * The target file exists, is not the same as
			 * the source file, and is active.  Link it
			 * to a temporary filename to avoid having
			 * the server removing the file completely.
			 */
			tmpname = newname();
			error = nfs3_link(ndvp, nvp, tmpname, cr);
			if (error == EOPNOTSUPP) {
				error = nfs3_rename(ndvp, nnm, ndvp, tmpname,
				    cr);
			}
			if (error) {
				kmem_free(tmpname, MAXNAMELEN);
				VN_RELE(ovp);
				VN_RELE(nvp);
				nfs_rw_exit(&odrp->r_rwlock);
				nfs_rw_exit(&ndrp->r_rwlock);
				return (error);
			}
			rp = VTOR(nvp);
			mutex_enter(&rp->r_statelock);
			if (rp->r_unldvp == NULL) {
				VN_HOLD(ndvp);
				rp->r_unldvp = ndvp;
				if (rp->r_unlcred != NULL)
					crfree(rp->r_unlcred);
				crhold(cr);
				rp->r_unlcred = cr;
				rp->r_unlname = tmpname;
			} else {
				kmem_free(rp->r_unlname, MAXNAMELEN);
				rp->r_unlname = tmpname;
			}
			mutex_exit(&rp->r_statelock);
		}

		VN_RELE(nvp);
	}

	if (ovp == NULL) {
		/*
		 * When renaming directories to be a subdirectory of a
		 * different parent, the dnlc entry for ".." will no
		 * longer be valid, so it must be removed.
		 *
		 * We do a lookup here to determine whether we are renaming
		 * a directory and we need to check if we are renaming
		 * an unlinked file.  This might have already been done
		 * in previous code, so we check ovp == NULL to avoid
		 * doing it twice.
		 */

		error = nfs3lookup(odvp, onm, &ovp, NULL, 0, NULL, cr, 0);
		/*
		 * The source name *should* already exist.
		 */
		if (error) {
			nfs_rw_exit(&odrp->r_rwlock);
			nfs_rw_exit(&ndrp->r_rwlock);
			return (error);
		}
		ASSERT(ovp != NULL);
	}

	dnlc_remove(odvp, onm);
	dnlc_remove(ndvp, nnm);

	setdiropargs3(&args.from, onm, odvp);
	setdiropargs3(&args.to, nnm, ndvp);

	douprintf = 1;
	error = rfs3call(VTOMI(odvp), NFSPROC3_RENAME,
	    xdr_RENAME3args, (caddr_t)&args,
	    xdr_RENAME3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, NULL);

	if (error) {
		PURGE_ATTRCACHE(odvp);
		PURGE_ATTRCACHE(ndvp);
		VN_RELE(ovp);
		nfs_rw_exit(&odrp->r_rwlock);
		nfs_rw_exit(&ndrp->r_rwlock);
		return (error);
	}

	error = geterrno3(res.status);

	if (!error) {
		nfs3_cache_wcc_data(odvp, &res.resok.fromdir_wcc, cr);
		if (odrp->r_dir != NULL)
			nfs_purge_rddir_cache(odvp);
		nfs3_cache_wcc_data(ndvp, &res.resok.todir_wcc, cr);
		if (ndrp->r_dir != NULL)
			nfs_purge_rddir_cache(ndvp);
		/*
		 * when renaming directories to be a subdirectory of a
		 * different parent, the dnlc entry for ".." will no
		 * longer be valid, so it must be removed
		 */
		rp = VTOR(ovp);
		if (ndvp != odvp) {
			if (ovp->v_type == VDIR) {
				dnlc_remove(ovp, "..");
				if (rp->r_dir != NULL)
					nfs_purge_rddir_cache(ovp);
			}
		}

		/*
		 * If we are renaming the unlinked file, update the
		 * r_unldvp and r_unlname as needed.
		 */
		mutex_enter(&rp->r_statelock);
		if (rp->r_unldvp != NULL) {
			(void) strncpy(rp->r_unlname, nnm, MAXNAMELEN);
			rp->r_unlname[MAXNAMELEN - 1] = '\0';

			if (ndvp != rp->r_unldvp) {
				VN_RELE(rp->r_unldvp);
				rp->r_unldvp = ndvp;
				VN_HOLD(ndvp);
			}
		}
		mutex_exit(&rp->r_statelock);
	} else {
		nfs3_cache_wcc_data(odvp, &res.resfail.fromdir_wcc, cr);
		nfs3_cache_wcc_data(ndvp, &res.resfail.todir_wcc, cr);
		/*
		 * System V defines rename to return EEXIST, not
		 * ENOTEMPTY if the target directory is not empty.
		 * Over the wire, the error is NFSERR_ENOTEMPTY
		 * which geterrno maps to ENOTEMPTY.
		 */
		if (error == ENOTEMPTY)
			error = EEXIST;
	}

	VN_RELE(ovp);

	nfs_rw_exit(&odrp->r_rwlock);
	nfs_rw_exit(&ndrp->r_rwlock);

	return (error);
}

static int
nfs3_mkdir(vnode_t *dvp, char *nm, struct vattr *va, vnode_t **vpp, cred_t *cr)
{
	int error;
	MKDIR3args args;
	MKDIR3res res;
	int douprintf;
	struct vattr vattr;
	vnode_t *vp;
	rnode_t *drp;

	setdiropargs3(&args.where, nm, dvp);

	/*
	 * Decide what the group-id and set-gid bit of the created directory
	 * should be.  May have to do a setattr to get the gid right.
	 */
	error = setdirgid(dvp, &va->va_gid, cr);
	if (error)
		return (error);
	error = setdirmode(dvp, &va->va_mode, cr);
	if (error)
		return (error);
	va->va_mask |= AT_MODE|AT_GID;
	error = vattr_to_sattr3(va, &args.attributes);
	if (error) {
		/* req time field(s) overflow - return immediately */
		return (error);
	}

	drp = VTOR(dvp);
	if (nfs_rw_enter_sig(&drp->r_rwlock, RW_WRITER, INTR(dvp)))
		return (EINTR);

	dnlc_remove(dvp, nm);

	douprintf = 1;
	error = rfs3call(VTOMI(dvp), NFSPROC3_MKDIR,
	    xdr_MKDIR3args, (caddr_t)&args,
	    xdr_MKDIR3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, NULL);

	if (error) {
		PURGE_ATTRCACHE(dvp);
		nfs_rw_exit(&drp->r_rwlock);
		return (error);
	}

	error = geterrno3(res.status);
	if (!error) {
		nfs3_cache_wcc_data(dvp, &res.resok.dir_wcc, cr);
		if (drp->r_dir != NULL)
			nfs_purge_rddir_cache(dvp);

		if (!res.resok.obj.handle_follows) {
			error = nfs3lookup(dvp, nm, &vp, NULL, 0, NULL, cr, 0);
			if (error) {
				nfs_rw_exit(&drp->r_rwlock);
				return (error);
			}
		} else {
			if (res.resok.obj_attributes.attributes) {
				vp = makenfs3node(&res.resok.obj.handle,
				    &res.resok.obj_attributes.attr,
				    dvp->v_vfsp, cr, NULL, NULL);
			} else {
				vp = makenfs3node(&res.resok.obj.handle, NULL,
				    dvp->v_vfsp, cr, NULL, NULL);
				if (vp->v_type == VNON) {
					vattr.va_mask = AT_TYPE;
					error = nfs3getattr(vp, &vattr, cr);
					if (error) {
						VN_RELE(vp);
						nfs_rw_exit(&drp->r_rwlock);
						return (error);
					}
					vp->v_type = vattr.va_type;
				}
			}
			dnlc_update(dvp, nm, vp, NOCRED);
		}
		if (va->va_gid != VTOR(vp)->r_attr.va_gid) {
			va->va_mask = AT_GID;
			(void) nfs3setattr(vp, va, 0, cr);
		}
		*vpp = vp;
	} else {
		nfs3_cache_wcc_data(dvp, &res.resfail.dir_wcc, cr);
		PURGE_STALE_FH(error, dvp, cr);
	}

	nfs_rw_exit(&drp->r_rwlock);

	return (error);
}

static int
nfs3_rmdir(vnode_t *dvp, char *nm, vnode_t *cdir, cred_t *cr)
{
	int error;
	RMDIR3args args;
	RMDIR3res res;
	vnode_t *vp;
	int douprintf;
	rnode_t *drp;

	drp = VTOR(dvp);
	if (nfs_rw_enter_sig(&drp->r_rwlock, RW_WRITER, INTR(dvp)))
		return (EINTR);

	/*
	 * Attempt to prevent a rmdir(".") from succeeding.
	 */
	error = nfs3lookup(dvp, nm, &vp, NULL, 0, NULL, cr, 0);
	if (error) {
		nfs_rw_exit(&drp->r_rwlock);
		return (error);
	}

	if (vp == cdir) {
		VN_RELE(vp);
		nfs_rw_exit(&drp->r_rwlock);
		return (EINVAL);
	}

	setdiropargs3(&args.object, nm, dvp);

	/*
	 * We need to use dnlc_purge_vp so that all references to the
	 * directory being removed are deleted.  This includes the
	 * entry for the directory itself in its parent directory as
	 * well as any entries which may exist in the directory being
	 * removed.
	 */
	dnlc_purge_vp(vp);

	douprintf = 1;
	error = rfs3call(VTOMI(dvp), NFSPROC3_RMDIR,
	    xdr_diropargs3, (caddr_t)&args,
	    xdr_RMDIR3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, NULL);

	PURGE_ATTRCACHE(vp);

	if (error) {
		PURGE_ATTRCACHE(dvp);
		VN_RELE(vp);
		nfs_rw_exit(&drp->r_rwlock);
		return (error);
	}

	error = geterrno3(res.status);
	if (!error) {
		nfs3_cache_wcc_data(dvp, &res.resok.dir_wcc, cr);
		if (drp->r_dir != NULL)
			nfs_purge_rddir_cache(dvp);
		if (VTOR(vp)->r_dir != NULL)
			nfs_purge_rddir_cache(vp);
	} else {
		nfs3_cache_wcc_data(dvp, &res.resfail.dir_wcc, cr);
		PURGE_STALE_FH(error, dvp, cr);
		/*
		 * System V defines rmdir to return EEXIST, not
		 * ENOTEMPTY if the directory is not empty.  Over
		 * the wire, the error is NFSERR_ENOTEMPTY which
		 * geterrno maps to ENOTEMPTY.
		 */
		if (error == ENOTEMPTY)
			error = EEXIST;
	}

	VN_RELE(vp);

	nfs_rw_exit(&drp->r_rwlock);

	return (error);
}

static int
nfs3_symlink(vnode_t *dvp, char *lnm, struct vattr *tva, char *tnm, cred_t *cr)
{
	int error;
	SYMLINK3args args;
	SYMLINK3res res;
	int douprintf;
	mntinfo_t *mi;
	vnode_t *vp;
	rnode_t *rp;
	char *contents;
	rnode_t *drp;

	mi = VTOMI(dvp);

	if (!(mi->mi_flags & MI_SYMLINK))
		return (EOPNOTSUPP);

	setdiropargs3(&args.where, lnm, dvp);
	error = vattr_to_sattr3(tva, &args.symlink.symlink_attributes);
	if (error) {
		/* req time field(s) overflow - return immediately */
		return (error);
	}
	args.symlink.symlink_data = tnm;

	drp = VTOR(dvp);
	if (nfs_rw_enter_sig(&drp->r_rwlock, RW_WRITER, INTR(dvp)))
		return (EINTR);

	dnlc_remove(dvp, lnm);

	douprintf = 1;
	error = rfs3call(mi, NFSPROC3_SYMLINK,
	    xdr_SYMLINK3args, (caddr_t)&args,
	    xdr_SYMLINK3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, NULL);

	if (error) {
		PURGE_ATTRCACHE(dvp);
		nfs_rw_exit(&drp->r_rwlock);
		return (error);
	}

	error = geterrno3(res.status);
	if (!error) {
		nfs3_cache_wcc_data(dvp, &res.resok.dir_wcc, cr);
		if (drp->r_dir != NULL)
			nfs_purge_rddir_cache(dvp);

		if (res.resok.obj.handle_follows) {
			if (res.resok.obj_attributes.attributes) {
				vp = makenfs3node(&res.resok.obj.handle,
				    &res.resok.obj_attributes.attr,
				    dvp->v_vfsp, cr, NULL, NULL);
			} else {
				vp = makenfs3node(&res.resok.obj.handle, NULL,
				    dvp->v_vfsp, cr, NULL, NULL);
				vp->v_type = VLNK;
				vp->v_rdev = 0;
			}
			dnlc_update(dvp, lnm, vp, NOCRED);
			rp = VTOR(vp);
			if (nfs3_do_symlink_cache &&
			    rp->r_symlink.contents == NULL) {
#ifdef DEBUG
				contents = symlink_cache_alloc(MAXPATHLEN,
				    KM_NOSLEEP);
#else
				contents = kmem_alloc(MAXPATHLEN, KM_NOSLEEP);
#endif
				if (contents != NULL) {
					mutex_enter(&rp->r_statelock);
					if (rp->r_symlink.contents == NULL) {
						rp->r_symlink.len = strlen(tnm);
						bcopy(tnm, contents,
						    rp->r_symlink.len);
						rp->r_symlink.contents =
						    contents;
						rp->r_symlink.size = MAXPATHLEN;
						mutex_exit(&rp->r_statelock);
					} else {
						mutex_exit(&rp->r_statelock);
#ifdef DEBUG
						symlink_cache_free(
						    (void *)contents,
							MAXPATHLEN);
#else
						kmem_free(contents, MAXPATHLEN);
#endif
					}
				}
			}
			VN_RELE(vp);
		}
	} else {
		nfs3_cache_wcc_data(dvp, &res.resfail.dir_wcc, cr);
		PURGE_STALE_FH(error, dvp, cr);
		if (error == EOPNOTSUPP) {
			mutex_enter(&mi->mi_lock);
			mi->mi_flags &= ~MI_SYMLINK;
			mutex_exit(&mi->mi_lock);
		}
	}

	nfs_rw_exit(&drp->r_rwlock);

	return (error);
}

#ifdef DEBUG
static int nfs3_readdir_cache_hits = 0;
static int nfs3_readdir_cache_shorts = 0;
#endif

/*
 * Read directory entries.
 * There are some weird things to look out for here.  The uio_loffset
 * field is either 0 or it is the offset returned from a previous
 * readdir.  It is an opaque value used by the server to find the
 * correct directory block to read. The count field is the number
 * of blocks to read on the server.  This is advisory only, the server
 * may return only one block's worth of entries.  Entries may be compressed
 * on the server.
 */
static int
nfs3_readdir(vnode_t *vp, struct uio *uiop, cred_t *cr, int *eofp)
{
	int error;
	uint_t count;
	rnode_t *rp;
	rddir_cache *rdc;
	rddir_cache *nrdc;
	rddir_cache *rrdc;

	rp = VTOR(vp);

	ASSERT(nfs_rw_lock_held(&rp->r_rwlock, RW_READER));

	/*
	 * Make sure that the directory cache is valid.
	 */
	if (rp->r_dir != NULL) {
		if (nfs_disable_rddir_cache != 0) {
			/*
			 * Setting nfs_disable_rddir_cache in /etc/system
			 * allows interoperability with servers that do not
			 * properly update the attributes of directories.
			 * Any cached information gets purged before an
			 * access is made to it.
			 */
			nfs_purge_rddir_cache(vp);
		}

		error = nfs3_validate_caches(vp, cr);
		if (error)
			return (error);
	}

	count = uiop->uio_iov->iov_len;

	nrdc = NULL;
top:
	/*
	 * Short circuit last readdir which always returns 0 bytes.
	 * This can be done after the directory has been read through
	 * completely at least once.  This will set r_direof which
	 * can be used to find the value of the last cookie.
	 */
	mutex_enter(&rp->r_statelock);
	if (rp->r_direof != NULL &&
	    uiop->uio_loffset == rp->r_direof->nfs3_ncookie) {
		mutex_exit(&rp->r_statelock);
#ifdef DEBUG
		nfs3_readdir_cache_shorts++;
#endif
		if (eofp)
			*eofp = 1;
		if (nrdc != NULL) {
#ifdef DEBUG
			rddir_cache_free((void *)nrdc, sizeof (*nrdc));
#else
			kmem_free(nrdc, sizeof (*nrdc));
#endif
		}
		return (0);
	}
	/*
	 * Look for a cache entry.  Cache entries are identified
	 * by the NFS cookie value and the byte count requested.
	 */
	rdc = rp->r_dir;
	while (rdc != NULL) {
		/*
		 * To NFS 3, the cookie is an opaque 8 byte entity.  To
		 * the rest of the system, the cookie is really an
		 * offset.  Thus, NFS 3 stores the cookie in offset_t
		 * sized elements and compares them to offset_t offsets.
		 * This is valid as long as the client makes no other
		 * assumptions about the values of cookies.  The only
		 * valid tests are equal and not equal.
		 */
		if (rdc->nfs3_cookie == uiop->uio_loffset &&
		    rdc->buflen == count) {
			/*
			 * If the cache entry is in the process of being
			 * filled in, wait until this completes.  The
			 * RDDIRWAIT bit is set to indicate that someone
			 * is waiting and then the thread currently
			 * filling the entry is done, it should do a
			 * cv_broadcast to wakeup all of the threads
			 * waiting for it to finish.
			 */
			if (rdc->flags & RDDIR) {
				nfs_rw_exit(&rp->r_rwlock);
				rdc->flags |= RDDIRWAIT;
				if (!cv_wait_sig(&rdc->cv, &rp->r_statelock)) {
					/*
					 * We got interrupted, probably
					 * the user typed ^C or an alarm
					 * fired.  We free the new entry
					 * if we allocated one.
					 */
					mutex_exit(&rp->r_statelock);
					(void) nfs_rw_enter_sig(&rp->r_rwlock,
						RW_READER, FALSE);
					if (nrdc != NULL) {
#ifdef DEBUG
						rddir_cache_free((void *)nrdc,
						    sizeof (*nrdc));
#else
						kmem_free(nrdc, sizeof (*nrdc));
#endif
					}
					return (EINTR);
				}
				mutex_exit(&rp->r_statelock);
				(void) nfs_rw_enter_sig(&rp->r_rwlock,
					RW_READER, FALSE);
				goto top;
			}
			/*
			 * Check to see if a readdir is required to
			 * fill the entry.  If so, mark this entry
			 * as being filled, remove our reference,
			 * and branch to the code to fill the entry.
			 */
			if (rdc->flags & RDDIRREQ) {
				rdc->flags &= ~RDDIRREQ;
				rdc->flags |= RDDIR;
				if (nrdc != NULL) {
#ifdef DEBUG
					rddir_cache_free((void *)nrdc,
					    sizeof (*nrdc));
#else
					kmem_free(nrdc, sizeof (*nrdc));
#endif
				}
				nrdc = rdc;
				mutex_exit(&rp->r_statelock);
				goto bottom;
			}
#ifdef DEBUG
			nfs3_readdir_cache_hits++;
#endif
			/*
			 * If an error occurred while attempting
			 * to fill the cache entry, just return it.
			 */
			if (rdc->error) {
				error = rdc->error;
				mutex_exit(&rp->r_statelock);
				if (nrdc != NULL) {
#ifdef DEBUG
					rddir_cache_free((void *)nrdc,
					    sizeof (*nrdc));
#else
					kmem_free(nrdc, sizeof (*nrdc));
#endif
				}
				return (error);
			}

			/*
			 * The cache entry is complete and good,
			 * copyout the dirent structs to the calling
			 * thread.
			 */
			error = uiomove(rdc->entries, rdc->entlen,
			    UIO_READ, uiop);

			/*
			 * If no error occurred during the copyout,
			 * update the offset in the uio struct to
			 * contain the value of the next NFS 3 cookie
			 * and set the eof value appropriately.
			 */
			if (!error) {
				uiop->uio_loffset = rdc->nfs3_ncookie;
				if (eofp)
					*eofp = rdc->eof;
			}
			/*
			 * Decide whether to do readahead.  Don't if
			 * have already read to the end of directory.
			 */
			if (rdc->eof) {
				mutex_exit(&rp->r_statelock);
				if (nrdc != NULL) {
#ifdef DEBUG
					rddir_cache_free((void *)nrdc,
					    sizeof (*nrdc));
#else
					kmem_free(nrdc, sizeof (*nrdc));
#endif
				}
				return (error);
			}
			/*
			 * Now look for a readahead entry.
			 */
			rrdc = rp->r_dir;
			while (rrdc != NULL) {
				if (rrdc->nfs3_cookie == rdc->nfs3_ncookie &&
				    rrdc->buflen == count)
					break;
				rrdc = rrdc->next;
			}
			/*
			 * Check to see whether we found an entry
			 * for the readahead.  If so, we don't need
			 * to do anything further, so free the new
			 * entry if one was allocated.  Otherwise,
			 * allocate a new entry, add it to the cache,
			 * and then initiate an asynchronous readdir
			 * operation to fill it.
			 */
			if (rrdc != NULL) {
				if (nrdc != NULL) {
#ifdef DEBUG
					rddir_cache_free((void *)nrdc,
					    sizeof (*nrdc));
#else
					kmem_free(nrdc, sizeof (*nrdc));
#endif
				}
			} else {
				if (nrdc != NULL)
					rrdc = nrdc;
				else {
#ifdef DEBUG
					rrdc = rddir_cache_alloc(sizeof (*rrdc),
					    KM_NOSLEEP);
#else
					rrdc = kmem_alloc(sizeof (*rrdc),
					    KM_NOSLEEP);
#endif
				}
				if (rrdc != NULL) {
					rrdc->nfs3_cookie = rdc->nfs3_ncookie;
					rrdc->buflen = count;
					rrdc->flags = RDDIR;
					cv_init(&rrdc->cv, NULL,
					    CV_DEFAULT, NULL);
					rrdc->next = rp->r_dir;
					rp->r_dir = rrdc;
					mutex_exit(&rp->r_statelock);
					nfs_async_readdir(vp, rrdc, cr,
					    do_nfs3asyncreaddir);
					return (error);
				}
			}
			mutex_exit(&rp->r_statelock);
			return (error);
		}
		rdc = rdc->next;
	}

	/*
	 * Didn't find an entry in the cache.  Construct a new empty
	 * entry and link it into the cache.  Other processes attempting
	 * to access this entry will need to wait until it is filled in.
	 *
	 * Since kmem_alloc may block, another pass through the cache
	 * will need to be taken to make sure that another process
	 * hasn't already added an entry to the cache for this request.
	 */
	if (nrdc == NULL) {
		mutex_exit(&rp->r_statelock);
		nfs_rw_exit(&rp->r_rwlock);
#ifdef DEBUG
		nrdc = rddir_cache_alloc(sizeof (*nrdc), KM_SLEEP);
#else
		nrdc = kmem_alloc(sizeof (*nrdc), KM_SLEEP);
#endif
		nrdc->nfs3_cookie = uiop->uio_loffset;
		nrdc->buflen = count;
		nrdc->flags = RDDIR;
		cv_init(&nrdc->cv, NULL, CV_DEFAULT, NULL);
		(void) nfs_rw_enter_sig(&rp->r_rwlock, RW_READER, FALSE);
		goto top;
	}

	/*
	 * Add this entry to the cache.
	 */
	nrdc->next = rp->r_dir;
	rp->r_dir = nrdc;
	mutex_exit(&rp->r_statelock);

bottom:
	/*
	 * Do the readdir.  This routine decides whether to use
	 * READDIR or READDIRPLUS.
	 */
	error = do_nfs3readdir(vp, nrdc, cr);

	/*
	 * If this operation failed, just return the error which occurred.
	 */
	if (error != 0)
		return (error);

	/*
	 * Since the RPC operation will have taken sometime and blocked
	 * this process, another pass through the cache will need to be
	 * taken to find the correct cache entry.  It is possible that
	 * the correct cache entry will not be there (although one was
	 * added) because the directory changed during the RPC operation
	 * and the readdir cache was flushed.  In this case, just start
	 * over.  It is hoped that this will not happen too often... :-)
	 */
	nrdc = NULL;
	goto top;
	/* NOTREACHED */
}

static int
do_nfs3readdir(vnode_t *vp, rddir_cache *rdc, cred_t *cr)
{
	int error;
	rnode_t *rp;
	mntinfo_t *mi;

	rp = VTOR(vp);
	mi = VTOMI(vp);

	/*
	 * Issue the proper request.  READDIRPLUS is used unless
	 * the server does not support READDIRPLUS or until the
	 * directory has been completely read once using READDIRPLUS.
	 * Once the directory has been completely read, the DNLC is
	 * assumed to be loaded and any further use of READDIRPLUS
	 * is just unnecessary overhead.  READDIR is used to retrieve
	 * directory entries which we do not find cached.  If the
	 * directory cache and the DNLC are flushed because the
	 * directory has changed, READDIRPLUS is used once again to
	 * repopulate the DNLC.
	 */
	if (!(mi->mi_flags & MI_READDIR) && !(rp->r_flags & REOF)) {
		nfs3readdirplus(vp, rdc, cr);
		if (rdc->error == EOPNOTSUPP)
			nfs3readdir(vp, rdc, cr);
	} else
		nfs3readdir(vp, rdc, cr);

	mutex_enter(&rp->r_statelock);
	rdc->flags &= ~RDDIR;
	if (rdc->flags & RDDIRWAIT) {
		rdc->flags &= ~RDDIRWAIT;
		cv_broadcast(&rdc->cv);
	}
	error = rdc->error;
	if (error)
		rdc->flags |= RDDIRREQ;
	mutex_exit(&rp->r_statelock);

	return (error);
}

static int
do_nfs3asyncreaddir(vnode_t *vp, rddir_cache *rdc, cred_t *cr)
{
	int error;
	rnode_t *rp;

	rp = VTOR(vp);
	(void) nfs_rw_enter_sig(&rp->r_rwlock, RW_READER, FALSE);

	error = do_nfs3readdir(vp, rdc, cr);

	nfs_rw_exit(&rp->r_rwlock);

	return (error);
}

static void
nfs3readdir(vnode_t *vp, rddir_cache *rdc, cred_t *cr)
{
	int error;
	READDIR3args args;
	READDIR3res res;
	rnode_t *rp;
	uint_t count;
	int douprintf;
	failinfo_t fi, *fip;
	mntinfo_t *mi;

	count = rdc->buflen;

	rp = VTOR(vp);
	mi = VTOMI(vp);

	args.dir = *RTOFH3(rp);
	args.cookie = (cookie3)rdc->nfs3_cookie;
	bcopy(rp->r_cookieverf, args.cookieverf, sizeof (cookieverf3));
	args.count = count;
	/*
	 * NFS client failover support
	 * suppress failover unless we have a zero cookie
	 */
	if (args.cookie == (cookie3) 0) {
		fi.vp = vp;
		fi.fhp = (caddr_t)&args.dir;
		fi.copyproc = nfs3copyfh;
		fi.lookupproc = nfs3lookup;
		fip = &fi;
	} else {
		fip = NULL;
	}

	res.resok.reply.entries = kmem_alloc(count, KM_SLEEP);
	res.resok.size = count;
	res.resok.cookie = args.cookie;

	douprintf = 1;

	if (mi->mi_io_kstats) {
		mutex_enter(&mi->mi_lock);
		kstat_runq_enter(KSTAT_IO_PTR(mi->mi_io_kstats));
		mutex_exit(&mi->mi_lock);
	}

	error = rfs3call(VTOMI(vp), NFSPROC3_READDIR,
	    xdr_READDIR3args, (caddr_t)&args,
	    xdr_READDIR3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, fip);

	if (mi->mi_io_kstats) {
		mutex_enter(&mi->mi_lock);
		kstat_runq_exit(KSTAT_IO_PTR(mi->mi_io_kstats));
		mutex_exit(&mi->mi_lock);
	}

	if (!error) {
		error = geterrno3(res.status);
		if (!error) {
			nfs3_cache_post_op_attr(vp, &res.resok.dir_attributes,
			    cr);
			rdc->nfs3_ncookie = (offset_t)res.resok.cookie;
			bcopy(res.resok.cookieverf,
			    rp->r_cookieverf, sizeof (cookieverf3));
			rdc->entries = (char *)res.resok.reply.entries;
			if (res.resok.reply.eof) {
				rdc->eof = 1;
				mutex_enter(&rp->r_statelock);
				rp->r_flags |= REOF;
				rp->r_direof = rdc;
				mutex_exit(&rp->r_statelock);
			} else
				rdc->eof = 0;
			rdc->entlen = res.resok.size;
			rdc->error = 0;
			if (mi->mi_io_kstats) {
				mutex_enter(&mi->mi_lock);
				KSTAT_IO_PTR(mi->mi_io_kstats)->reads++;
				KSTAT_IO_PTR(mi->mi_io_kstats)->nread +=
				    res.resok.size;
				mutex_exit(&mi->mi_lock);
			}
		} else {
			nfs3_cache_post_op_attr(vp,
			    &res.resfail.dir_attributes, cr);
			PURGE_STALE_FH(error, vp, cr);
		}
	}
	if (error) {
		kmem_free(res.resok.reply.entries, count);
		rdc->entries = NULL;
		rdc->error = error;
	}
}

#ifdef nextdp
#undef nextdp
#endif
#define	nextdp(dp)	((struct dirent64 *)((char *)(dp) + (dp)->d_reclen))
/*
 * Read directory entries.
 * There are some weird things to look out for here.  The uio_loffset
 * field is either 0 or it is the offset returned from a previous
 * readdir.  It is an opaque value used by the server to find the
 * correct directory block to read. The count field is the number
 * of blocks to read on the server.  This is advisory only, the server
 * may return only one block's worth of entries.  Entries may be compressed
 * on the server.
 */
static void
nfs3readdirplus(vnode_t *vp, rddir_cache *rdc, cred_t *cr)
{
	int error;
	READDIRPLUS3args args;
	READDIRPLUS3res res;
	rnode_t *rp;
	mntinfo_t *mi;
	uint_t count, x;
	int douprintf;
	char *buf;
	char *ibufp;
	struct dirent64 *idp;
	struct dirent64 *odp;
	post_op_attr *atp;
	post_op_fh3 *fhp;
	int isize;
	int osize;
	vnode_t *nvp;
	offset_t loff;
	failinfo_t fi, *fip;

	count = rdc->buflen;

	rp = VTOR(vp);
	mi = VTOMI(vp);

	args.dir = *RTOFH3(rp);
	args.cookie = (cookie3)rdc->nfs3_cookie;
	bcopy(rp->r_cookieverf,  args.cookieverf, sizeof (cookieverf3));
	args.dircount = count;
	args.maxcount = MAXBSIZE;
	/*
	 * NFS client failover support
	 * suppress failover unless we have a zero cookie
	 */
	if (args.cookie == (cookie3) 0) {
		fi.vp = vp;
		fi.fhp = (caddr_t)&args.dir;
		fi.copyproc = nfs3copyfh;
		fi.lookupproc = nfs3lookup;
		fip = &fi;
	} else {
		fip = NULL;
	}

	res.resok.reply.entries = kmem_alloc(MAXBSIZE, KM_SLEEP);
	res.resok.size = MAXBSIZE;

	loff = rdc->nfs3_cookie;

	douprintf = 1;

	if (mi->mi_io_kstats) {
		mutex_enter(&mi->mi_lock);
		kstat_runq_enter(KSTAT_IO_PTR(mi->mi_io_kstats));
		mutex_exit(&mi->mi_lock);
	}

	error = rfs3call(mi, NFSPROC3_READDIRPLUS,
	    xdr_READDIRPLUS3args, (caddr_t)&args,
	    xdr_READDIRPLUS3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, fip);

	if (mi->mi_io_kstats) {
		mutex_enter(&mi->mi_lock);
		kstat_runq_exit(KSTAT_IO_PTR(mi->mi_io_kstats));
		mutex_exit(&mi->mi_lock);
	}

	if (error) {
		rdc->entries = NULL;
		rdc->error = error;
		goto out;
	}

	error = geterrno3(res.status);
	if (error) {
		nfs3_cache_post_op_attr(vp, &res.resfail.dir_attributes, cr);
		PURGE_STALE_FH(error, vp, cr);
		if (error == EOPNOTSUPP) {
			mutex_enter(&mi->mi_lock);
			mi->mi_flags |= MI_READDIR;
			mutex_exit(&mi->mi_lock);
		}
		rdc->entries = NULL;
		rdc->error = error;
		goto out;
	}

	nfs3_cache_post_op_attr(vp, &res.resok.dir_attributes, cr);

	buf = kmem_alloc(count, KM_SLEEP);

	odp = (struct dirent64 *)buf;
	ibufp = (char *)res.resok.reply.entries;
	isize = res.resok.size;
	osize = 0;
	while (isize > 0) {
		idp = (struct dirent64 *)ibufp;
		if (osize + idp->d_reclen > count) {
			res.resok.reply.eof = FALSE;
			break;
		}
		bcopy(idp, odp, idp->d_reclen);
		loff = (offset_t)idp->d_off;
		osize += odp->d_reclen;
		odp = nextdp(odp);
		isize -= idp->d_reclen;
		ibufp += idp->d_reclen;

		/* reclen is 8-byte aligned so no ibuf alignment needed here */

		atp = (post_op_attr *)ibufp;
		if (!atp->attributes) {
			ibufp += sizeof (atp->attributes);
			isize -= sizeof (atp->attributes);
		} else {
			ibufp += sizeof (*atp);
			isize -= sizeof (*atp);
		}

		/* Make sure that ibufp is aligned on an 8-byte boundary */
		ALIGN64(x, ibufp, isize)

		fhp = (post_op_fh3 *)ibufp;
		if (!fhp->handle_follows) {
			ibufp += sizeof (fhp->handle_follows);
			isize -= sizeof (fhp->handle_follows);
		} else {
			ibufp += sizeof (*fhp);
			isize -= sizeof (*fhp);
		}

		/* Make sure that ibufp is aligned on an 8-byte boundary */
		ALIGN64(x, ibufp, isize)

		/*
		 * Add this entry to the DNLC if it isn't "." and
		 * we have attributes.  Otherwise, we end up
		 * polluting the DNLC with "." entries or not
		 * being able to determine what type of file
		 * this entry references.
		 */
		if (fhp->handle_follows &&
		    strcmp(idp->d_name, ".") != 0 && atp->attributes) {
			nvp = makenfs3node(&fhp->handle, &atp->attr,
			    vp->v_vfsp, cr, rp->r_path, idp->d_name);
			dnlc_update(vp, idp->d_name, nvp, NOCRED);
			VN_RELE(nvp);
		}
	}

	rdc->nfs3_ncookie = loff;
	bcopy(res.resok.cookieverf, rp->r_cookieverf, sizeof (cookieverf3));
	rdc->entries = buf;
	if (res.resok.reply.eof) {
		rdc->eof = 1;
		mutex_enter(&rp->r_statelock);
		rp->r_flags |= REOF;
		rp->r_direof = rdc;
		mutex_exit(&rp->r_statelock);
	} else
		rdc->eof = 0;
	rdc->entlen = osize;
	rdc->error = 0;

	if (mi->mi_io_kstats) {
		mutex_enter(&mi->mi_lock);
		KSTAT_IO_PTR(mi->mi_io_kstats)->reads++;
		KSTAT_IO_PTR(mi->mi_io_kstats)->nread += res.resok.size;
		mutex_exit(&mi->mi_lock);
	}

out:
	kmem_free(res.resok.reply.entries, MAXBSIZE);
}

#ifdef DEBUG
static int nfs3_bio_do_stop = 0;
#endif

static int
nfs3_bio(struct buf *bp, stable_how *stab_comm, cred_t *cr)
{
	rnode_t *rp = VTOR(bp->b_vp);
	int count;
	int error;
	int read = (bp->b_flags & B_READ);
	cred_t *cred;
	u_offset_t offset, curcount;

	offset = ldbtob(bp->b_lblkno);

	if (read) {
		mutex_enter(&rp->r_statelock);
		if (rp->r_cred != NULL) {
			cred = rp->r_cred;
			crhold(cred);
		} else {
			rp->r_cred = cr;
			crhold(cr);
			cred = cr;
			crhold(cred);
		}
		mutex_exit(&rp->r_statelock);
	read_again:
		error = bp->b_error = nfs3read(bp->b_vp, bp->b_un.b_addr,
		    offset, bp->b_bcount, &bp->b_resid, cred);
		crfree(cred);
		if (!error) {
			if (bp->b_resid) {
				/*
				 * Didn't get it all because we hit EOF,
				 * zero all the memory beyond the EOF.
				 */
				/* bzero(rdaddr + */
				bzero(bp->b_un.b_addr +
				    bp->b_bcount - bp->b_resid, bp->b_resid);
			}
			mutex_enter(&rp->r_statelock);
			if (bp->b_resid == bp->b_bcount &&
			    offset >= rp->r_size) {
				/*
				 * We didn't read anything at all as we are
				 * past EOF.  Return an error indicator back
				 * but don't destroy the pages (yet).
				 */
				error = NFS_EOF;
			}
			mutex_exit(&rp->r_statelock);
		} else if (error == EACCES) {
			mutex_enter(&rp->r_statelock);
			if (cred != cr) {
				if (rp->r_cred != NULL)
					crfree(rp->r_cred);
				rp->r_cred = cr;
				crhold(cr);
				cred = cr;
				crhold(cred);
				mutex_exit(&rp->r_statelock);
				goto read_again;
			}
			mutex_exit(&rp->r_statelock);
		}
	} else {
		if (!(rp->r_flags & RDONTWRITE)) {
			mutex_enter(&rp->r_statelock);
			if (rp->r_cred != NULL) {
				cred = rp->r_cred;
				crhold(cred);
			} else {
				rp->r_cred = cr;
				crhold(cr);
				cred = cr;
				crhold(cred);
			}
			mutex_exit(&rp->r_statelock);
		write_again:
			mutex_enter(&rp->r_statelock);
			curcount = rp->r_size - offset;
			count = MIN(bp->b_bcount, (int)curcount);
			mutex_exit(&rp->r_statelock);
			if (count < 0)
				cmn_err(CE_PANIC, "nfs3_bio: write count < 0");
#ifdef DEBUG
			if (count == 0) {
				cmn_err(CE_WARN,
				    "nfs3_bio: zero length write at %lld",
				    offset);
				nfs_printfhandle(&VTOR(bp->b_vp)->r_fh);
				if (nfs3_bio_do_stop)
					debug_enter("nfs3_bio");
			}
#endif
			error = nfs3write(bp->b_vp, bp->b_un.b_addr, offset,
			    count, cred, stab_comm);
			if (error == EACCES) {
				mutex_enter(&rp->r_statelock);
				if (cred != cr) {
					if (rp->r_cred != NULL)
						crfree(rp->r_cred);
					rp->r_cred = cr;
					crhold(cr);
					crfree(cred);
					cred = cr;
					crhold(cred);
					mutex_exit(&rp->r_statelock);
					goto write_again;
				}
				mutex_exit(&rp->r_statelock);
			}
			bp->b_error = error;
			if (error && error != EINTR) {
				/*
				 * Don't print EDQUOT errors on the console.
				 * Don't print asynchronous EACCES errors.
				 * Don't print EFBIG errors.
				 * Print all other write errors.
				 */
				if (error != EDQUOT && error != EFBIG &&
				    (error != EACCES ||
				    !(bp->b_flags & B_ASYNC)))
					nfs_write_error(bp->b_vp, error, cred);
				/*
				 * Update r_error and r_flags as appropriate.
				 * If the error was ESTALE, then mark the
				 * rnode as not being writeable and save
				 * the error status.  Otherwise, save any
				 * errors which occur from asynchronous
				 * page invalidations.  Any errors occurring
				 * from other operations should be saved
				 * by the caller.
				 */
				mutex_enter(&rp->r_statelock);
				if (error == ESTALE) {
					rp->r_flags |= RDONTWRITE;
					if (!rp->r_error)
						rp->r_error = error;
				} else if (!rp->r_error &&
				    (bp->b_flags & (B_INVAL|B_FORCE|B_ASYNC)) ==
				    (B_INVAL|B_FORCE|B_ASYNC)) {
					rp->r_error = error;
				}
				mutex_exit(&rp->r_statelock);
			}
			crfree(cred);
		} else
			error = rp->r_error;
	}

	if (error != 0 && error != NFS_EOF)
		bp->b_flags |= B_ERROR;

	return (error);
}

static int
nfs3_fid(vnode_t *vp, fid_t *fidp)
{
	rnode_t *rp;

	rp = VTOR(vp);

	if (fidp->fid_len < (ushort_t)rp->r_fh.fh_len) {
		fidp->fid_len = rp->r_fh.fh_len;
		return (ENOSPC);
	}
	fidp->fid_len = rp->r_fh.fh_len;
	bcopy(rp->r_fh.fh_buf, fidp->fid_data, fidp->fid_len);
	return (0);
}

static void
nfs3_rwlock(vnode_t *vp, int write_lock)
{
	rnode_t *rp = VTOR(vp);

	if (write_lock)
		(void) nfs_rw_enter_sig(&rp->r_rwlock, RW_WRITER, FALSE);
	else
		(void) nfs_rw_enter_sig(&rp->r_rwlock, RW_READER, FALSE);
}

/* ARGSUSED */
static void
nfs3_rwunlock(vnode_t *vp, int write_lock)
{
	rnode_t *rp = VTOR(vp);

	nfs_rw_exit(&rp->r_rwlock);
}

/* ARGSUSED */
static int
nfs3_seek(vnode_t *vp, offset_t ooff, offset_t *noffp)
{

	/*
	 * Because we stuff the readdir cookie into the offset field
	 * someone may attempt to do an lseek with the cookie which
	 * we want to succeed.
	 */
	if (vp->v_type == VDIR)
		return (0);
	if (*noffp < 0)
		return (EINVAL);
	return (0);
}

/*
 * number of pages to read ahead
 * optimized for 100 base-T.
 */
static int nfs3_nra = 4;

/* number of readaheads for offset 0 */

#define	NFS3_FIRST_NRA	1
static int nfs3_first_nra = NFS3_FIRST_NRA;

#ifdef DEBUG
static int nfs3_lostpage = 0;	/* number of times we lost original page */
#endif

/*
 * Return all the pages from [off..off+len) in file
 */
static int
nfs3_getpage(vnode_t *vp, offset_t off, size_t len, uint_t *protp,
	page_t *pl[], size_t plsz, struct seg *seg, caddr_t addr,
	enum seg_rw rw, cred_t *cr)
{
	rnode_t *rp = VTOR(vp);
	int error;

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	if (protp != NULL)
		*protp = PROT_ALL;

	/*
	 * Now valididate that the caches are up to date.
	 */
	(void) nfs3_validate_caches(vp, cr);

retry:
	/*
	 * If we are getting called as a side effect of an nfs_write()
	 * operation the local file size might not be extended yet.
	 * In this case we want to be able to return pages of zeroes.
	 */
	mutex_enter(&rp->r_statelock);
	if (off + len > rp->r_size + PAGEOFFSET && seg != segkmap) {
		mutex_exit(&rp->r_statelock);
		return (EFAULT);		/* beyond EOF */
	}
	mutex_exit(&rp->r_statelock);

	if (len <= PAGESIZE) {
		error = nfs3_getapage(vp, off, len, protp, pl, plsz,
		    seg, addr, rw, cr);
	} else {
		error = pvn_getpages(nfs3_getapage, vp, off, len, protp,
		    pl, plsz, seg, addr, rw, cr);
	}

	switch (error) {
	case NFS_EOF:
		nfs_purge_caches(vp, cr);
		goto retry;
	case ESTALE:
		PURGE_STALE_FH(error, vp, cr);
	}

	return (error);
}

/*
 * Called from pvn_getpages or nfs3_getpage to get a particular page.
 */
/* ARGSUSED */
static int
nfs3_getapage(vnode_t *vp, u_offset_t off, size_t len, uint_t *protp,
	page_t *pl[], size_t plsz, struct seg *seg, caddr_t addr,
	enum seg_rw rw, cred_t *cr)
{
	rnode_t *rp;
	uint_t bsize;
	struct buf *bp;
	page_t *pp;
	u_offset_t lbn;
	u_offset_t io_off;
	u_offset_t blkoff;
	u_offset_t rablkoff;
	size_t io_len;
	uint_t blksize;
	int error;
	int readahead;
	int readahead_issued;
	page_t *pagefound;
	page_t *savepp;

	rp = VTOR(vp);
	bsize = MAX(vp->v_vfsp->vfs_bsize, PAGESIZE);

reread:
	bp = NULL;
	pp = NULL;
	pagefound = NULL;

	if (pl != NULL)
		pl[0] = NULL;

	error = 0;
	lbn = off / bsize;
	blkoff = lbn * bsize;

	/*
	 * Calculate the number of readaheads to do.
	 */
#if 0 /* notdef */
	mutex_enter(&rp->r_statelock);
	if (!(vp->v_flag & VNOCACHE) && (rp->r_nextr == off || off == 0))
		readahead = nfs3_nra;
	else
		readahead = 0;
	mutex_exit(&rp->r_statelock);

	/*
	 * Queueing up the readahead before doing the synchronous read
	 * results in a significant increase in read throughput because
	 * of the increased parallelism between the async threads and
	 * the process context.
	 */
	if ((off & (MAXBSIZE - 1)) == 0 && rw != S_CREATE) {
		rablkoff = blkoff;
		mutex_enter(&rp->r_statelock);
		while (readahead > 0 && rablkoff + bsize < rp->r_size) {
			mutex_exit(&rp->r_statelock);
			nfs_async_readahead(vp, rablkoff + bsize,
			    addr + (rablkoff + bsize - off),
			    seg, cr, nfs3_readahead);
			readahead--;
			rablkoff += bsize;
			mutex_enter(&rp->r_statelock);
		}
		mutex_exit(&rp->r_statelock);
	}
#else
	/*
	 * Queueing up the readahead before doing the synchronous read
	 * results in a significant increase in read throughput because
	 * of the increased parallelism between the async threads and
	 * the process context.
	 */
	if ((off & (MAXBSIZE - 1)) == 0 &&
	    rw != S_CREATE &&
	    !(vp->v_flag & VNOCACHE)) {
		mutex_enter(&rp->r_statelock);

		/*
		 * nfs3_first_nra is to control number
		 * of reahaheads when offset == 0.
		 */

		if (off == 0)
			readahead = nfs3_first_nra;
		else if (rp->r_nextr == off)
			readahead = nfs3_nra;
		else
			readahead = 0;

		/*
		 * Indicate that we did a readahead so
		 * readahead offset is not updated
		 * by the synchronous read below.
		 */
		readahead_issued = readahead;
		rablkoff = blkoff;
		while (readahead > 0 && rablkoff + bsize < rp->r_size) {
			mutex_exit(&rp->r_statelock);
			nfs_async_readahead(vp, rablkoff + bsize,
			    addr + (rablkoff + bsize - off),
			    seg, cr, nfs3_readahead);
			readahead--;
			rablkoff += bsize;
			mutex_enter(&rp->r_statelock);
			/*
			 * set readahead offset to
			 * offset of last async readahead
			 * request.
			 */
			rp->r_nextr = rablkoff;
		}
		mutex_exit(&rp->r_statelock);
	}
#endif

again:
	if ((pagefound = page_exists(vp, off)) == NULL) {
		if (pl == NULL) {
			nfs_async_readahead(vp, blkoff, addr, seg, cr,
			    nfs3_readahead);
		} else if (rw == S_CREATE) {
			/*
			 * Block for this page is not allocated, or the offset
			 * is beyond the current allocation size, or we're
			 * allocating a swap slot and the page was not found,
			 * so allocate it and return a zero page.
			 */
			if ((pp = page_create_va(vp, off,
			    PAGESIZE, PG_WAIT, seg, addr)) == NULL)
				cmn_err(CE_PANIC, "nfs3_getapage: page_create");
			io_len = PAGESIZE;
			mutex_enter(&rp->r_statelock);
			rp->r_nextr = off + PAGESIZE;
			mutex_exit(&rp->r_statelock);
		} else {
			/*
			 * Need to go to server to get a block
			 */
			mutex_enter(&rp->r_statelock);
			if (blkoff < rp->r_size &&
			    blkoff + bsize > rp->r_size) {
				/*
				 * If less than a block left in
				 * file read less than a block.
				 */
				if (rp->r_size <= off) {
					/*
					 * Trying to access beyond EOF,
					 * set up to get at least one page.
					 */
					blksize = off + PAGESIZE - blkoff;
				} else
					blksize = rp->r_size - blkoff;
			} else
				blksize = bsize;
			mutex_exit(&rp->r_statelock);

			pp = pvn_read_kluster(vp, off, seg, addr, &io_off,
			    &io_len, blkoff, blksize, 0);

			/*
			 * Some other thread has entered the page,
			 * so just use it.
			 */
			if (pp == NULL)
				goto again;

			/*
			 * Now round the request size up to page boundaries.
			 * This ensures that the entire page will be
			 * initialized to zeroes if EOF is encountered.
			 */
			io_len = ptob(btopr(io_len));

			bp = pageio_setup(pp, io_len, vp, B_READ);
			ASSERT(bp != NULL);

			/*
			 * pageio_setup should have set b_addr to 0.  This
			 * is correct since we want to do I/O on a page
			 * boundary.  bp_mapin will use this addr to calculate
			 * an offset, and then set b_addr to the kernel virtual
			 * address it allocated for us.
			 */
			ASSERT(bp->b_un.b_addr == 0);

			bp->b_edev = 0;
			bp->b_dev = 0;
			bp->b_lblkno = lbtodb(io_off);
			bp_mapin(bp);

			/*
			 * If doing a write beyond what we believe is EOF,
			 * don't bother trying to read the pages from the
			 * server, we'll just zero the pages here.  We
			 * don't check that the rw flag is S_WRITE here
			 * because some implementations may attempt a
			 * read access to the buffer before copying data.
			 */
			mutex_enter(&rp->r_statelock);
			if (io_off >= rp->r_size && seg == segkmap) {
				mutex_exit(&rp->r_statelock);
				bzero(bp->b_un.b_addr, io_len);
			} else {
				mutex_exit(&rp->r_statelock);
				error = nfs3_bio(bp, NULL, cr);
			}

			/*
			 * Unmap the buffer before freeing it.
			 */
			bp_mapout(bp);
			pageio_done(bp);

			savepp = pp;
			do {
				pp->p_fsdata = C_NOCOMMIT;
			} while ((pp = pp->p_next) != savepp);

			if (error == NFS_EOF) {
				/*
				 * If doing a write system call just return
				 * zeroed pages, else user tried to get pages
				 * beyond EOF, return error.  We don't check
				 * that the rw flag is S_WRITE here because
				 * some implementations may attempt a read
				 * access to the buffer before copying data.
				 */
				if (seg == segkmap)
					error = 0;
				else
					error = EFAULT;
			}

			if (!readahead_issued && !error) {
			    mutex_enter(&rp->r_statelock);
			    rp->r_nextr = io_off + io_len;
			    mutex_exit(&rp->r_statelock);
			}
		}
	}

out:
	if (pl == NULL)
		return (error);

	if (error) {
		if (pp != NULL)
			pvn_read_done(pp, B_ERROR);
		return (error);
	}

	if (pagefound) {
		se_t se = (rw == S_CREATE ? SE_EXCL : SE_SHARED);

		/*
		 * Page exists in the cache, acquire the appropriate lock.
		 * If this fails, start all over again.
		 */
		if ((pp = page_lookup(vp, off, se)) == NULL) {
#ifdef DEBUG
			nfs3_lostpage++;
#endif
			goto reread;
		}
		pl[0] = pp;
		pl[1] = NULL;
		return (0);
	}

	if (pp != NULL)
		pvn_plist_init(pp, pl, plsz, off, io_len, rw);

	return (error);
}

static void
nfs3_readahead(vnode_t *vp, u_offset_t blkoff, caddr_t addr, struct seg *seg,
	cred_t *cr)
{
	int error;
	page_t *pp;
	u_offset_t io_off;
	size_t io_len;
	struct buf *bp;
	uint_t bsize, blksize;
	rnode_t *rp = VTOR(vp);
	page_t *savepp;

	bsize = MAX(vp->v_vfsp->vfs_bsize, PAGESIZE);

	mutex_enter(&rp->r_statelock);
	if (blkoff < rp->r_size && blkoff + bsize > rp->r_size) {
		/*
		 * If less than a block left in file read less
		 * than a block.
		 */
		blksize = rp->r_size - blkoff;
	} else
		blksize = bsize;
	mutex_exit(&rp->r_statelock);

	pp = pvn_read_kluster(vp, blkoff, segkmap, addr,
	    &io_off, &io_len, blkoff, blksize, 1);
	/*
	 * The isra flag passed to the kluster function is 1, we may have
	 * gotten a return value of NULL for a variety of reasons (# of free
	 * pages < minfree, someone entered the page on the vnode etc). In all
	 * cases, we want to punt on the readahead.
	 */
	if (pp == NULL)
		return;

	/*
	 * Now round the request size up to page boundaries.
	 * This ensures that the entire page will be
	 * initialized to zeroes if EOF is encountered.
	 */
	io_len = ptob(btopr(io_len));

	bp = pageio_setup(pp, io_len, vp, B_READ);
	ASSERT(bp != NULL);

	/*
	 * pageio_setup should have set b_addr to 0.  This is correct since
	 * we want to do I/O on a page boundary. bp_mapin() will use this addr
	 * to calculate an offset, and then set b_addr to the kernel virtual
	 * address it allocated for us.
	 */
	ASSERT(bp->b_un.b_addr == 0);

	bp->b_edev = 0;
	bp->b_dev = 0;
	bp->b_lblkno = lbtodb(io_off);
	bp_mapin(bp);

	/*
	 * If doing a write beyond what we believe is EOF, don't bother trying
	 * to read the pages from the server, we'll just zero the pages here.
	 * We don't check that the rw flag is S_WRITE here because some
	 * implementations may attempt a read access to the buffer before
	 * copying data.
	 */
	mutex_enter(&rp->r_statelock);
	if (io_off >= rp->r_size && seg == segkmap) {
		mutex_exit(&rp->r_statelock);
		bzero(bp->b_un.b_addr, io_len);
		error = 0;
	} else {
		mutex_exit(&rp->r_statelock);
		error = nfs3_bio(bp, NULL, cr);
		if (error == NFS_EOF)
			error = 0;
	}

	/*
	 * Unmap the buffer before freeing it.
	 */
	bp_mapout(bp);
	pageio_done(bp);

	savepp = pp;
	do {
		pp->p_fsdata = C_NOCOMMIT;
	} while ((pp = pp->p_next) != savepp);

	pvn_read_done(pp, error ? B_READ | B_ERROR : B_READ);

	/*
	 * In case of error set readahead offset
	 * to the lowest offset.
	 * pvn_read_done() calls VN_DISPOSE to destroy the pages
	 */
	if (error && rp->r_nextr > io_off) {
		mutex_enter(&rp->r_statelock);
		if (rp->r_nextr > io_off)
			rp->r_nextr = io_off;
		mutex_exit(&rp->r_statelock);
	}
}

/*
 * Flags are composed of {B_INVAL, B_FREE, B_DONTNEED, B_FORCE}
 * If len == 0, do from off to EOF.
 *
 * The normal cases should be len == 0 && off == 0 (entire vp list) or
 * len == MAXBSIZE (from segmap_release actions), and len == PAGESIZE
 * (from pageout).
 */
static int
nfs3_putpage(vnode_t *vp, offset_t off, size_t len, int flags, cred_t *cr)
{
	int error;
	rnode_t *rp;

	ASSERT(cr != NULL);

	/*
	 * XXX - Why should this check be made here?
	 */
	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	if (len == 0 && !(flags & B_INVAL) &&
	    (vp->v_vfsp->vfs_flag & VFS_RDONLY))
		return (0);

	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	rp->r_count++;
	mutex_exit(&rp->r_statelock);
	error = nfs_putpages(vp, off, len, flags, cr);
	mutex_enter(&rp->r_statelock);
	rp->r_count--;
	cv_broadcast(&rp->r_cv);
	mutex_exit(&rp->r_statelock);

	return (error);
}

/*
 * Write out a single page, possibly klustering adjacent dirty pages.
 */
int
nfs3_putapage(vnode_t *vp, page_t *pp, u_offset_t *offp, size_t *lenp,
	int flags, cred_t *cr)
{
	u_offset_t io_off;
	u_offset_t lbn_off;
	u_offset_t lbn;
	size_t io_len;
	uint_t bsize;
	int error;
	rnode_t *rp;

	ASSERT(!(vp->v_vfsp->vfs_flag & VFS_RDONLY));
	ASSERT(pp != NULL);
	ASSERT(cr != NULL);

	rp = VTOR(vp);
	ASSERT(rp->r_count > 0);

	bsize = MAX(vp->v_vfsp->vfs_bsize, PAGESIZE);
	lbn = pp->p_offset / bsize;
	lbn_off = lbn * bsize;

	/*
	 * Find a kluster that fits in one block, or in
	 * one page if pages are bigger than blocks.  If
	 * there is less file space allocated than a whole
	 * page, we'll shorten the i/o request below.
	 */
	pp = pvn_write_kluster(vp, pp, &io_off, &io_len, lbn_off,
	    roundup(bsize, PAGESIZE), flags);

	/*
	 * pvn_write_kluster shouldn't have returned a page with offset
	 * behind the original page we were given.  Verify that.
	 */
	ASSERT((pp->p_offset / bsize) >= lbn);

	/*
	 * Now pp will have the list of kept dirty pages marked for
	 * write back.  It will also handle invalidation and freeing
	 * of pages that are not dirty.  Check for page length rounding
	 * problems.
	 */
	if (io_off + io_len > lbn_off + bsize) {
		ASSERT((io_off + io_len) - (lbn_off + bsize) < PAGESIZE);
		io_len = lbn_off + bsize - io_off;
	}
	/*
	 * The RMODINPROGRESS flag makes sure that nfs(3)_bio() sees a
	 * consistent value of r_size. RMODINPROGRESS is set in writerp().
	 * When RMODINPROGRESS is set it indicates that a uiomove() is in
	 * progress and the r_size has not been made consistent with the
	 * new size of the file. When the uiomove() completes the r_size is
	 * updated and the RMODINPROGRESS flag is cleared.
	 *
	 * The RMODINPROGRESS flag makes sure that nfs(3)_bio() sees a
	 * consistent value of r_size. Without this handshaking, it is
	 * possible that nfs(3)_bio() picks  up the old value of r_size
	 * before the uiomove() in writerp() completes. This will result
	 * in the write through nfs(3)_bio() being dropped.
	 *
	 * More precisely, there is a window between the time the uiomove()
	 * completes and the time the r_size is updated. If a VOP_PUTPAGE()
	 * operation intervenes in this window, the page will be picked up,
	 * because it is dirty (it will be unlocked, unless it was
	 * pagecreate'd). When the page is picked up as dirty, the dirty
	 * bit is reset (pvn_getdirty()). In nfs(3)write(), r_size is
	 * checked. This will still be the old size. Therefore the page will
	 * not be written out. When segmap_release() calls VOP_PUTPAGE(),
	 * the page will be found to be clean and the write will be dropped.
	 */
	if (rp->r_flags & RMODINPROGRESS) {
		mutex_enter(&rp->r_statelock);
		if ((rp->r_flags & RMODINPROGRESS) &&
		    rp->r_modaddr + MAXBSIZE > io_off &&
		    rp->r_modaddr < io_off + io_len) {
			page_t *plist;
			/*
			 * A write is in progress for this region of the file.
			 * If we did not detect RMODINPROGRESS here then this
			 * path through nfs_putapage() would eventually go to
			 * nfs(3)_bio() and may not write out all of the data
			 * in the pages. We end up losing data. So we decide
			 * to set the modified bit on each page in the page
			 * list and mark the rnode with RDIRTY. This write
			 * will be restarted at some later time.
			 */
			plist = pp;
			while (plist != NULL) {
				pp = plist;
				page_sub(&plist, pp);
				hat_setmod(pp);
				page_io_unlock(pp);
				page_unlock(pp);
			}
			rp->r_flags |= RDIRTY;
			mutex_exit(&rp->r_statelock);
			if (offp)
				*offp = io_off;
			if (lenp)
				*lenp = io_len;
			return (0);
		}
		mutex_exit(&rp->r_statelock);
	}

	if (flags & B_ASYNC) {
		error = nfs_async_putapage(vp, pp, io_off, io_len, flags, cr,
		    nfs3_sync_putapage);
	} else
		error = nfs3_sync_putapage(vp, pp, io_off, io_len, flags, cr);

	if (offp)
		*offp = io_off;
	if (lenp)
		*lenp = io_len;
	return (error);
}

static int
nfs3_sync_putapage(vnode_t *vp, page_t *pp, u_offset_t io_off, size_t io_len,
	int flags, cred_t *cr)
{
	int error;
	rnode_t *rp;

	flags |= B_WRITE;

	error = nfs3_rdwrlbn(vp, pp, io_off, io_len, flags, cr);

	rp = VTOR(vp);

	if ((error == ENOSPC || error == EDQUOT ||
			error == EFBIG || error == EACCES) &&
	    (flags & (B_INVAL|B_FORCE)) != (B_INVAL|B_FORCE)) {
		if (!(rp->r_flags & ROUTOFSPACE)) {
			mutex_enter(&rp->r_statelock);
			rp->r_flags |= ROUTOFSPACE;
			mutex_exit(&rp->r_statelock);
		}
		flags |= B_ERROR;
		pvn_write_done(pp, flags);
		error = nfs3_putpage(vp, io_off, io_len,
		    B_INVAL | B_FORCE | (flags & B_ASYNC), cr);
	} else {
		if (error)
			flags |= B_ERROR;
		else if (rp->r_flags & ROUTOFSPACE) {
			mutex_enter(&rp->r_statelock);
			rp->r_flags &= ~ROUTOFSPACE;
			mutex_exit(&rp->r_statelock);
		}
		pvn_write_done(pp, flags);
		if (freemem < desfree)
			(void) nfs3_commit_vp(vp, (u_offset_t)0, 0, cr);
	}

	return (error);
}

static int
nfs3_map(vnode_t *vp, offset_t off, struct as *as, caddr_t *addrp,
	size_t len, uchar_t prot, uchar_t maxprot, uint_t flags, cred_t *cr)
{
	struct segvn_crargs vn_a;
	int error;
	rnode_t *rp = VTOR(vp);

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	if (off < 0 || (offset_t)(off + len) < 0)
		return (EINVAL);

	if (vp->v_type != VREG)
		return (ENODEV);

	/*
	 * Check to see if the vnode is currently marked as not cachable.
	 * This means portions of the file are locked (through VOP_FRLOCK).
	 * In this case the map request must be refused.  We use
	 * rp->r_lkserlock to avoid a race with concurrent lock requests.
	 */
	if (nfs_rw_enter_sig(&rp->r_lkserlock, RW_READER, INTR(vp)))
		return (EINTR);
	mutex_enter(&rp->r_statelock);
	if ((vp->v_flag & VNOCACHE) ||
	    (rp->r_flags & RDIRECTIO) ||
	    (VTOMI(vp)->mi_flags & MI_DIRECTIO)) {
		mutex_exit(&rp->r_statelock);
		nfs_rw_exit(&rp->r_lkserlock);
		return (EAGAIN);
	}
	mutex_exit(&rp->r_statelock);

	/*
	 * Don't allow concurrent locks and mapping if mandatory locking is
	 * enabled.
	 */
	if (flk_has_remote_locks(vp) || lm_has_sleep(vp)) {
		struct vattr va;

		va.va_mask = AT_MODE;
		error = VOP_GETATTR(vp, &va, 0, cr);
		if (error != 0) {
			goto done;
		} else if (MANDLOCK(vp, va.va_mode)) {
			error = EAGAIN;
			goto done;
		}
	}

	as_rangelock(as);
	if (!(flags & MAP_FIXED)) {
		map_addr(addrp, len, off, 1, flags);
		if (*addrp == NULL) {
			as_rangeunlock(as);
			error = ENOMEM;
			goto done;
		}
	} else {
		/*
		 * User specified address - blow away any previous mappings
		 */
		(void) as_unmap(as, *addrp, len);
	}

	vn_a.vp = vp;
	vn_a.offset = off;
	vn_a.type = (flags & MAP_TYPE);
	vn_a.prot = (uchar_t)prot;
	vn_a.maxprot = (uchar_t)maxprot;
	vn_a.flags = (flags & ~MAP_TYPE);
	vn_a.cred = cr;
	vn_a.amp = NULL;

	error = as_map(as, *addrp, len, segvn_create, &vn_a);
	as_rangeunlock(as);

done:
	nfs_rw_exit(&rp->r_lkserlock);
	return (error);
}

/* ARGSUSED */
static int
nfs3_addmap(vnode_t *vp, offset_t off, struct as *as, caddr_t addr,
	size_t len, uchar_t prot, uchar_t maxprot, uint_t flags, cred_t *cr)
{
	rnode_t *rp;
	struct vattr va;
	int error = 0;

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	rp->r_mapcnt += btopr(len);
	mutex_exit(&rp->r_statelock);
	/*
	 * If there is no cached data or if close-to-open
	 * consistency checking is turned off, we can avoid
	 * the over the wire getattr.  Otherwise, force a
	 * call to the server to get fresh attributes and to
	 * check caches. This is required for close-to-open
	 * consistency.
	 */
	if (vp->v_pages == NULL || VTOMI(vp)->mi_flags & MI_NOCTO) {

		return (0);
	}

	va.va_mask = AT_ALL;
	error = nfs3_getattr_otw(vp, &va, cr);
	if (error) {
		mutex_enter(&rp->r_statelock);
		rp->r_mapcnt -= btopr(len);
		mutex_exit(&rp->r_statelock);
		ASSERT(rp->r_mapcnt >= 0);
	}
	return (error);

}

static int
nfs3_cmp(vnode_t *vp1, vnode_t *vp2)
{

	return (vp1 == vp2);
}

static int
nfs3_frlock(vnode_t *vp, int cmd, struct flock64 *bfp, int flag,
	offset_t offset, cred_t *cr)
{
	netobj lm_fh3;
	int rc;
	u_offset_t start, end;
	rnode_t *rp;
	int error = 0, intr = INTR(vp);

	/* check for valid cmd parameter */
	if (cmd != F_GETLK && cmd != F_SETLK && cmd != F_SETLKW)
		return (EINVAL);

	/* Verify l_type. */
	switch (bfp->l_type) {
	case F_RDLCK:
		if (cmd != F_GETLK && !(flag & FREAD))
			return (EBADF);
		break;
	case F_WRLCK:
		if (cmd != F_GETLK && !(flag & FWRITE))
			return (EBADF);
		break;
	case F_UNLCK:
		intr = 0;
		break;

	default:
		return (EINVAL);
	}

	/* check the validity of the lock range */
	if (rc = flk_convert_lock_data(vp, bfp, &start, &end, offset))
		return (rc);
	if (rc = flk_check_lock_data(start, end, MAXEND))
		return (rc);

	rp = VTOR(vp);

	/*
	 * Check whether the given lock request can proceed, given the
	 * current file mappings.
	 */
	if (nfs_rw_enter_sig(&rp->r_lkserlock, RW_WRITER, intr))
		return (EINTR);
	if (cmd == F_SETLK || cmd == F_SETLKW) {
		if (!lm_safelock(vp, bfp, cr)) {
			rc = EAGAIN;
			goto done;
		}
	}

	/*
	 * If the filesystem is mounted using local locking, pass the
	 * request off to the local locking code.
	 */
	if (VTOMI(vp)->mi_flags & MI_LLOCK) {
		rc = fs_frlock(vp, cmd, bfp, flag, offset, cr);
		goto done;
	}

	/*
	 * Flush the cache after waiting for async I/O to finish.  For new
	 * locks, this is so that the process gets the latest bits from the
	 * server.  For unlocks, this is so that other clients see the
	 * latest bits once the file has been unlocked.  If currently dirty
	 * pages can't be flushed, then don't allow a lock to be set.  But
	 * allow unlocks to succeed, to avoid having orphan locks on the
	 * server.
	 */
	if (cmd != F_GETLK) {
		mutex_enter(&rp->r_statelock);
		while (rp->r_count > 0) {
		    if (intr) {
			if (cv_wait_sig(&rp->r_cv, &rp->r_statelock) == 0) {
				rc = EINTR;
				break;
			}
		    } else
			cv_wait(&rp->r_cv, &rp->r_statelock);
		}
		mutex_exit(&rp->r_statelock);
		if (rc != 0)
			goto done;
		error = nfs3_putpage(vp, (u_offset_t)0, 0, B_INVAL, cr);
		if (error) {
			if (error == ENOSPC || error == EDQUOT) {
				mutex_enter(&rp->r_statelock);
				if (!rp->r_error)
					rp->r_error = error;
				mutex_exit(&rp->r_statelock);
			}
			if (bfp->l_type != F_UNLCK) {
				rc = ENOLCK;
				goto done;
			}
		}
	}

	lm_fh3.n_len = VTOFH3(vp)->fh3_length;
	lm_fh3.n_bytes = (char *)&(VTOFH3(vp)->fh3_u.data);

	/*
	 * Call the lock manager to do the real work of contacting
	 * the server and obtaining the lock.
	 */
	rc = lm4_frlock(vp, cmd, bfp, flag, offset, cr, &lm_fh3);

	if (rc == 0)
		nfs_lockcompletion(vp, cmd);

done:
	nfs_rw_exit(&rp->r_lkserlock);
	return (rc);
}

/*
 * Free storage space associated with the specified vnode.  The portion
 * to be freed is specified by bfp->l_start and bfp->l_len (already
 * normalized to a "whence" of 0).
 *
 * This is an experimental facility whose continued existence is not
 * guaranteed.  Currently, we only support the special case
 * of l_len == 0, meaning free to end of file.
 */
/* ARGSUSED */
static int
nfs3_space(vnode_t *vp, int cmd, struct flock64 *bfp, int flag,
	offset_t offset, cred_t *cr)
{
	int error;

	ASSERT(vp->v_type == VREG);
	if (cmd != F_FREESP)
		return (EINVAL);

	error = convoff(vp, bfp, 0, offset);
	if (!error) {
		ASSERT(bfp->l_start >= 0);
		if (bfp->l_len == 0) {
			struct vattr va;

			va.va_mask = AT_SIZE;
			va.va_size = bfp->l_start;
			error = nfs3setattr(vp, &va, 0, cr);
		} else
			error = EINVAL;
	}

	return (error);
}

/* ARGSUSED */
static int
nfs3_realvp(vnode_t *vp, vnode_t **vpp)
{

	return (EINVAL);
}

/*
 * Remove some pages from an mmap'd vnode.  Just update the
 * count of pages.  If doing close-to-open, then flush and
 * commit all of the pages associated with this file.
 * Otherwise, start an asynchronous page flush to write out
 * any dirty pages.  This will also associate a credential
 * with the rnode which can be used to write the pages.
 */
/* ARGSUSED */
static int
nfs3_delmap(vnode_t *vp, offset_t off, struct as *as, caddr_t addr,
	size_t len, uint_t prot, uint_t maxprot, uint_t flags, cred_t *cr)
{
	int error;
	rnode_t *rp;

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	rp->r_mapcnt -= btopr(len);
	ASSERT(rp->r_mapcnt >= 0);
	mutex_exit(&rp->r_statelock);

	/*
	 * Initiate a page flush and potential commit if there are
	 * pages, the file system was not mounted readonly, the segment
	 * was mapped shared, and the pages themselves were writeable.
	 */

	if (vp->v_pages != NULL && !(vp->v_vfsp->vfs_flag & VFS_RDONLY) &&
		flags == MAP_SHARED && (maxprot & PROT_WRITE)) {

		mutex_enter(&rp->r_statelock);
		rp->r_flags |= RDIRTY;
		mutex_exit(&rp->r_statelock);
		if (VTOMI(vp)->mi_flags & MI_NOCTO)
			error = nfs3_putpage(vp, (u_offset_t)off, len,
								B_ASYNC, cr);
		else
			error = nfs3_putpage_commit(vp, (u_offset_t)off,
								len, cr);
		if (!error) {
			mutex_enter(&rp->r_statelock);
			error = rp->r_error;
			rp->r_error = 0;
			mutex_exit(&rp->r_statelock);
		}
	} else
		error = 0;

	return (error);
}

static int nfs3_pathconf_disable_cache = 0;

#ifdef DEBUG
static int nfs3_pathconf_cache_hits = 0;
static int nfs3_pathconf_cache_misses = 0;
#endif

static int
nfs3_pathconf(vnode_t *vp, int cmd, ulong_t *valp, cred_t *cr)
{
	int error;
	PATHCONF3args args;
	PATHCONF3res res;
	int douprintf;
	failinfo_t fi;
	rnode_t *rp;

	/*
	 * Large file spec - need to base answer on info stored
	 * on original FSINFO response.
	 */
	if (cmd == _PC_FILESIZEBITS) {
		unsigned long long ll;
		long l = 1;

		ll = VTOMI(vp)->mi_maxfilesize;

		if (ll == 0) {
			*valp = 0;
			return (0);
		}

		if (ll & 0xffffffff00000000) {
			l += 32; ll >>= 32;
		}
		if (ll & 0xffff0000) {
			l += 16; ll >>= 16;
		}
		if (ll & 0xff00) {
			l += 8; ll >>= 8;
		}
		if (ll & 0xf0) {
			l += 4; ll >>= 4;
		}
		if (ll & 0xc) {
			l += 2; ll >>= 2;
		}
		if (ll & 0x2) {
			l += 1;
		}
		*valp = l;
		return (0);
	}

	rp = VTOR(vp);
	if (rp->r_pathconf != NULL) {
		mutex_enter(&rp->r_statelock);
		if (rp->r_pathconf != NULL && nfs3_pathconf_disable_cache) {
			kmem_free(rp->r_pathconf, sizeof (*rp->r_pathconf));
			rp->r_pathconf = NULL;
		}
		if (rp->r_pathconf != NULL) {
			error = 0;
			switch (cmd) {
			case _PC_LINK_MAX:
				*valp = rp->r_pathconf->link_max;
				break;
			case _PC_NAME_MAX:
				*valp = rp->r_pathconf->name_max;
				break;
			case _PC_PATH_MAX:
				*valp = MAXPATHLEN;
				break;
			case _PC_CHOWN_RESTRICTED:
				*valp = rp->r_pathconf->chown_restricted;
				break;
			case _PC_NO_TRUNC:
				*valp = rp->r_pathconf->no_trunc;
				break;
			default:
				error = EINVAL;
				break;
			}
			mutex_exit(&rp->r_statelock);
#ifdef DEBUG
			nfs3_pathconf_cache_hits++;
#endif
			return (error);
		}
		mutex_exit(&rp->r_statelock);
	}
#ifdef DEBUG
	nfs3_pathconf_cache_misses++;
#endif

	args.object = *VTOFH3(vp);
	fi.vp = vp;
	fi.fhp = (caddr_t)&args.object;
	fi.copyproc = nfs3copyfh;
	fi.lookupproc = nfs3lookup;

	douprintf = 1;
	error = rfs3call(VTOMI(vp), NFSPROC3_PATHCONF,
	    xdr_nfs_fh3, (caddr_t)&args,
	    xdr_PATHCONF3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, &fi);

	if (error)
		return (error);

	error = geterrno3(res.status);

	if (!error) {
		nfs3_cache_post_op_attr(vp, &res.resok.obj_attributes, cr);
		if (!nfs3_pathconf_disable_cache) {
			mutex_enter(&rp->r_statelock);
			if (rp->r_pathconf == NULL) {
				rp->r_pathconf = kmem_alloc(
				    sizeof (*rp->r_pathconf), KM_NOSLEEP);
				if (rp->r_pathconf != NULL)
					*rp->r_pathconf = res.resok.info;
			}
			mutex_exit(&rp->r_statelock);
		}
		switch (cmd) {
		case _PC_LINK_MAX:
			*valp = res.resok.info.link_max;
			break;
		case _PC_NAME_MAX:
			*valp = res.resok.info.name_max;
			break;
		case _PC_PATH_MAX:
			*valp = MAXPATHLEN;
			break;
		case _PC_CHOWN_RESTRICTED:
			*valp = res.resok.info.chown_restricted;
			break;
		case _PC_NO_TRUNC:
			*valp = res.resok.info.no_trunc;
			break;
		default:
			return (EINVAL);
		}
	} else {
		nfs3_cache_post_op_attr(vp, &res.resfail.obj_attributes, cr);
		PURGE_STALE_FH(error, vp, cr);
	}

	return (error);
}

/*
 * Called by async thread to do synchronous pageio. Do the i/o, wait
 * for it to complete, and cleanup the page list when done.
 */
static int
nfs3_sync_pageio(vnode_t *vp, page_t *pp, u_offset_t io_off, size_t io_len,
	int flags, cred_t *cr)
{
	int error;

	error = nfs3_rdwrlbn(vp, pp, io_off, io_len, flags, cr);
	if (flags & B_READ)
		pvn_read_done(pp, (error ? B_ERROR : 0) | flags);
	else
		pvn_write_done(pp, (error ? B_ERROR : 0) | flags);
	return (error);
}

static int
nfs3_pageio(vnode_t *vp, page_t *pp, u_offset_t io_off, size_t io_len,
	int flags, cred_t *cr)
{
	int error;
	rnode_t *rp;

	if (pp == NULL)
		return (EINVAL);

	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	rp->r_count++;
	mutex_exit(&rp->r_statelock);

	if (flags & B_ASYNC) {
		error = nfs_async_pageio(vp, pp, io_off, io_len, flags, cr,
		    nfs3_sync_pageio);
	} else
		error = nfs3_rdwrlbn(vp, pp, io_off, io_len, flags, cr);
	mutex_enter(&rp->r_statelock);
	rp->r_count--;
	cv_broadcast(&rp->r_cv);
	mutex_exit(&rp->r_statelock);
	return (error);
}

static void
nfs3_dispose(vnode_t *vp, page_t *pp, int fl, int dn, cred_t *cr)
{
	int error;
	rnode_t *rp;
	page_t *plist;
	page_t *pptr;
	offset3 offset;
	count3 len;
	k_sigset_t smask;

	/*
	 * We should get called with fl equal to either B_FREE or
	 * B_INVAL.  Any other value is illegal.
	 *
	 * The page that we are either supposed to free or destroy
	 * should be exclusive locked and its io lock should not
	 * be held.
	 */
	ASSERT(fl == B_FREE || fl == B_INVAL);
	ASSERT((PAGE_EXCL(pp) && !page_iolock_assert(pp)) || panicstr);

	rp = VTOR(vp);

	/*
	 * If the page doesn't need to be committed or we shouldn't
	 * even bother attempting to commit it, then just make sure
	 * that the p_fsdata byte is clear and then either free or
	 * destroy the page as appropriate.
	 */
	if (pp->p_fsdata == C_NOCOMMIT || (rp->r_flags & RDONTWRITE)) {
		pp->p_fsdata = C_NOCOMMIT;
		if (fl == B_FREE)
			page_free(pp, dn);
		else
			page_destroy(pp, dn);
		return;
	}

	/*
	 * If there is a page invalidation operation going on, then
	 * if this is one of the pages being destroyed, then just
	 * clear the p_fsdata byte and then either free or destroy
	 * the page as appropriate.
	 */
	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	if ((rp->r_flags & RTRUNCATE) && pp->p_offset >= rp->r_truncaddr) {
		mutex_exit(&rp->r_statelock);
		pp->p_fsdata = C_NOCOMMIT;
		if (fl == B_FREE)
			page_free(pp, dn);
		else
			page_destroy(pp, dn);
		return;
	}

	/*
	 * If we are freeing this page and someone else is already
	 * waiting to do a commit, then just unlock the page and
	 * return.  That other thread will take care of commiting
	 * this page.  The page can be freed sometime after the
	 * commit has finished.  Otherwise, if the page is marked
	 * as delay commit, then we may be getting called from
	 * pvn_write_done, one page at a time.   This could result
	 * in one commit per page, so we end up doing lots of small
	 * commits instead of fewer larger commits.  This is bad,
	 * we want do as few commits as possible.
	 */
	if (fl == B_FREE) {
		if (rp->r_flags & RCOMMITWAIT) {
			page_unlock(pp);
			mutex_exit(&rp->r_statelock);
			return;
		}
		if (pp->p_fsdata == C_DELAYCOMMIT) {
			pp->p_fsdata = C_COMMIT;
			page_unlock(pp);
			mutex_exit(&rp->r_statelock);
			return;
		}
	}

	/*
	 * Check to see if there is a signal which would prevent an
	 * attempt to commit the pages from being successful.  If so,
	 * then don't bother with all of the work to gather pages and
	 * generate the unsuccessful RPC.  Just return from here and
	 * let the page be committed at some later time.
	 */
	sigintr(&smask, VTOMI(vp)->mi_flags & MI_INT);
	if (ttolwp(curthread) != NULL && ISSIG(curthread, JUSTLOOKING)) {
		sigunintr(&smask);
		page_unlock(pp);
		mutex_exit(&rp->r_statelock);
		return;
	}
	sigunintr(&smask);

	/*
	 * We are starting to need to commit pages, so let's try
	 * to commit as many as possible at once to reduce the
	 * overhead.
	 *
	 * Set the `commit inprogress' state bit.  We must
	 * first wait until any current one finishes.  Then
	 * we initialize the c_pages list with this page.
	 */
	while (rp->r_flags & RCOMMIT) {
		rp->r_flags |= RCOMMITWAIT;
		cv_wait(&rp->r_commit.c_cv, &rp->r_statelock);
		rp->r_flags &= ~RCOMMITWAIT;
	}
	rp->r_flags |= RCOMMIT;
	mutex_exit(&rp->r_statelock);
	ASSERT(rp->r_commit.c_pages == NULL);
	rp->r_commit.c_pages = pp;
	rp->r_commit.c_commbase = (offset3)pp->p_offset;
	rp->r_commit.c_commlen = PAGESIZE;

	/*
	 * Gather together all other pages which can be committed.
	 * They will all be chained off r_commit.c_pages.
	 */
	nfs3_get_commit(vp);

	/*
	 * Clear the `commit inprogress' status and disconnect
	 * the list of pages to be committed from the rnode.
	 * At this same time, we also save the starting offset
	 * and length of data to be committed on the server.
	 */
	plist = rp->r_commit.c_pages;
	rp->r_commit.c_pages = NULL;
	offset = rp->r_commit.c_commbase;
	len = rp->r_commit.c_commlen;
	mutex_enter(&rp->r_statelock);
	rp->r_flags &= ~RCOMMIT;
	cv_broadcast(&rp->r_commit.c_cv);
	mutex_exit(&rp->r_statelock);

	if (curproc == proc_pageout) {
		nfs_async_commit(vp, plist, offset, len, cr, nfs3_async_commit);
		return;
	}

	/*
	 * Actually generate the COMMIT3 over the wire operation.
	 */
	error = nfs3_commit(vp, offset, len, cr);

	/*
	 * If we got an error during the commit, just unlock all
	 * of the pages.  The pages will get retransmitted to the
	 * server during a putpage operation.
	 */
	if (error) {
		while (plist != NULL) {
			pptr = plist;
			page_sub(&plist, pptr);
			page_unlock(pptr);
		}
		return;
	}

	/*
	 * We've tried as hard as we can to commit the data to stable
	 * storage on the server.  We release the rest of the pages
	 * and clear the commit required state.  They will be put
	 * onto the tail of the cachelist if they are nolonger
	 * mapped.
	 */
	while (plist != pp) {
		pptr = plist;
		page_sub(&plist, pptr);
		pptr->p_fsdata = C_NOCOMMIT;
		(void) page_release(pptr, 1);
	}

	/*
	 * Now, as appropriate, either free or destroy the page
	 * that we were called with.
	 */
	pp->p_fsdata = C_NOCOMMIT;
	if (fl == B_FREE)
		page_free(pp, dn);
	else
		page_destroy(pp, dn);
}

static int
nfs3_commit(vnode_t *vp, offset3 offset, count3 count, cred_t *cr)
{
	int error;
	rnode_t *rp;
	COMMIT3args args;
	COMMIT3res res;
	int douprintf;
	cred_t *cred;

	rp = VTOR(vp);

	mutex_enter(&rp->r_statelock);
	if (rp->r_cred != NULL) {
		cred = rp->r_cred;
		crhold(cred);
	} else {
		rp->r_cred = cr;
		crhold(cr);
		cred = cr;
		crhold(cred);
	}
	mutex_exit(&rp->r_statelock);

	args.file = *VTOFH3(vp);
	args.offset = offset;
	args.count = count;

doitagain:
	douprintf = 1;
	error = rfs3call(VTOMI(vp), NFSPROC3_COMMIT,
	    xdr_COMMIT3args, (caddr_t)&args,
	    xdr_COMMIT3res, (caddr_t)&res, cred,
	    &douprintf, &res.status, 0, NULL);

	crfree(cred);

	if (error)
		return (error);

	error = geterrno3(res.status);
	if (!error) {
		ASSERT(rp->r_flags & RHAVEVERF);
		mutex_enter(&rp->r_statelock);
		if (bcmp((caddr_t)&res.resok.verf, (caddr_t)&rp->r_verf,
		    sizeof (writeverf3)) == 0) {
			mutex_exit(&rp->r_statelock);
			return (0);
		}
		nfs3_set_mod(vp);
		bcopy(&res.resok.verf, &rp->r_verf, sizeof (writeverf3));
		mutex_exit(&rp->r_statelock);
		error = NFS_VERF_MISMATCH;
	} else {
		if (error == EACCES) {
			mutex_enter(&rp->r_statelock);
			if (cred != cr) {
				if (rp->r_cred != NULL)
					crfree(rp->r_cred);
				rp->r_cred = cr;
				crhold(cr);
				cred = cr;
				crhold(cred);
				mutex_exit(&rp->r_statelock);
				goto doitagain;
			}
			mutex_exit(&rp->r_statelock);
		}
		/*
		 * Can't do a PURGE_STALE_FH here because this
		 * can cause a deadlock.  nfs3_commit can
		 * be called from nfs3_dispose which can be called
		 * indirectly via pvn_vplist_dirty.  PURGE_STALE_FH
		 * can call back to pvn_vplist_dirty.
		 */
		if (error == ESTALE) {
			mutex_enter(&rp->r_statelock);
			rp->r_flags |= RDONTWRITE;
			if (!rp->r_error)
				rp->r_error = error;
			mutex_exit(&rp->r_statelock);
			PURGE_ATTRCACHE(vp);
		} else {
			mutex_enter(&rp->r_statelock);
			if (!rp->r_error)
				rp->r_error = error;
			mutex_exit(&rp->r_statelock);
		}
	}

	return (error);
}

static void
nfs3_set_mod(vnode_t *vp)
{
	page_t *pp;
	kmutex_t *vphm;

	vphm = page_vnode_mutex(vp);
	mutex_enter(vphm);
	if ((pp = vp->v_pages) != NULL) {
		do {
			if (pp->p_fsdata != C_NOCOMMIT) {
				hat_setmod(pp);
				pp->p_fsdata = C_NOCOMMIT;
			}
		} while ((pp = pp->p_vpnext) != vp->v_pages);
	}
	mutex_exit(vphm);
}


/*
 * This routine is used to gather together a page list of the pages
 * which are to be committed on the server.  This routine must not
 * be called if the calling thread holds any locked pages.
 *
 * The calling thread must have set RCOMMIT.  This bit is used to
 * serialize access to the commit structure in the rnode.  As long
 * as the thread has set RCOMMIT, then it can manipulate the commit
 * structure without requiring any other locks.
 */
static void
nfs3_get_commit(vnode_t *vp)
{
	rnode_t *rp;
	page_t *pp;
	kmutex_t *vphm;

	rp = VTOR(vp);

	ASSERT(rp->r_flags & RCOMMIT);

	vphm = page_vnode_mutex(vp);
	mutex_enter(vphm);

	/*
	 * If there are no pages associated with this vnode, then
	 * just return.
	 */
	if ((pp = vp->v_pages) == NULL) {
		mutex_exit(vphm);
		return;
	}

	/*
	 * Step through all of the pages associated with this vnode
	 * looking for pages which need to be committed.
	 */
	do {
		/*
		 * If this page does not need to be committed or is
		 * modified, then just skip it.
		 */
		if (pp->p_fsdata == C_NOCOMMIT || hat_ismod(pp))
			continue;

		/*
		 * Attempt to lock the page.  If we can't, then
		 * someone else is messing with it and we will
		 * just skip it.
		 */
		if (!page_trylock(pp, SE_EXCL))
			continue;

		ASSERT(PP_ISFREE(pp) == 0);

		/*
		 * The page needs to be committed and we locked it.
		 * Update the base and length parameters and add it
		 * to r_pages.
		 */
		if (rp->r_commit.c_pages == NULL) {
			rp->r_commit.c_commbase = (offset3)pp->p_offset;
			rp->r_commit.c_commlen = PAGESIZE;
		} else if (pp->p_offset < rp->r_commit.c_commbase) {
			rp->r_commit.c_commlen = rp->r_commit.c_commbase -
			    (offset3)pp->p_offset + rp->r_commit.c_commlen;
			rp->r_commit.c_commbase = (offset3)pp->p_offset;
		} else if ((rp->r_commit.c_commbase + rp->r_commit.c_commlen)
			    <= pp->p_offset) {
			rp->r_commit.c_commlen = (offset3)pp->p_offset -
			    rp->r_commit.c_commbase + PAGESIZE;
		}
		page_add(&rp->r_commit.c_pages, pp);
	} while ((pp = pp->p_vpnext) != vp->v_pages);

	mutex_exit(vphm);
}

/*
 * This routine is used to gather together a page list of the pages
 * which are to be committed on the server.  This routine must not
 * be called if the calling thread holds any locked pages.
 *
 * The calling thread must have set RCOMMIT.  This bit is used to
 * serialize access to the commit structure in the rnode.  As long
 * as the thread has set RCOMMIT, then it can manipulate the commit
 * structure without requiring any other locks.
 */
static void
nfs3_get_commit_range(vnode_t *vp, u_offset_t soff, size_t len)
{

	rnode_t *rp;
	page_t *pp;
	u_offset_t end;
	u_offset_t off;

	ASSERT(len != 0);

	rp = VTOR(vp);

	ASSERT(rp->r_flags & RCOMMIT);

	/*
	 * If there are no pages associated with this vnode, then
	 * just return.
	 */
	if ((pp = vp->v_pages) == NULL)
		return;

	/*
	 * Calculate the ending offset.
	 */
	end = soff + len;

	for (off = soff; off < end; off += PAGESIZE) {
		/*
		 * Lookup each page by vp, offset.
		 */
		if ((pp = page_lookup_nowait(vp, off, SE_EXCL)) == NULL)
			continue;

		/*
		 * If this page does not need to be committed or is
		 * modified, then just skip it.
		 */
		if (pp->p_fsdata == C_NOCOMMIT || hat_ismod(pp)) {
			page_unlock(pp);
			continue;
		}

		ASSERT(PP_ISFREE(pp) == 0);

		/*
		 * The page needs to be committed and we locked it.
		 * Update the base and length parameters and add it
		 * to r_pages.
		 */
		if (rp->r_commit.c_pages == NULL) {
			rp->r_commit.c_commbase = (offset3)pp->p_offset;
			rp->r_commit.c_commlen = PAGESIZE;
		} else {
			rp->r_commit.c_commlen = (offset3)pp->p_offset -
					rp->r_commit.c_commbase + PAGESIZE;
		}
		page_add(&rp->r_commit.c_pages, pp);
	}
}

#if 0	/* unused */
#ifdef DEBUG
static int
nfs3_no_uncommitted_pages(vnode_t *vp)
{
	page_t *pp;
	kmutex_t *vphm;

	vphm = page_vnode_mutex(vp);
	mutex_enter(vphm);
	if ((pp = vp->v_pages) != NULL) {
		do {
			if (pp->p_fsdata != C_NOCOMMIT) {
				mutex_exit(vphm);
				return (0);
			}
		} while ((pp = pp->p_vpnext) != vp->v_pages);
	}
	mutex_exit(vphm);

	return (1);
}
#endif
#endif

static int
nfs3_putpage_commit(vnode_t *vp, u_offset_t poff, size_t plen, cred_t *cr)
{
	int	   error;
	writeverf3 write_verf;
	rnode_t	   *rp = VTOR(vp);

	/*
	 * Flush the data portion of the file and then commit any
	 * portions which need to be committed.  This may need to
	 * be done twice if the server has changed state since
	 * data was last written.  The data will need to be
	 * rewritten to the server and then a new commit done.
	 *
	 * In fact, this may need to be done several times if the
	 * server is having problems and crashing while we are
	 * attempting to do this.
	 */

top:
	/*
	 * Do a flush based on the poff and plen arguments.  This
	 * will asynchronously write out any modified pages in the
	 * range specified by (poff, plen).  This starts all of the
	 * i/o operations which will be waited for in the next
	 * call to nfs3_putpage
	 */

	mutex_enter(&rp->r_statelock);
	bcopy(&rp->r_verf, &write_verf, sizeof (writeverf3));
	mutex_exit(&rp->r_statelock);

	error = nfs3_putpage(vp, poff, plen, B_ASYNC, cr);

	/*
	 * Do a flush based on the poff and plen arguments.  This
	 * will synchronously write out any modified pages in the
	 * range specified by (poff, plen) and wait until all of
	 * the asynchronous i/o's in that range are done as well.
	 */
	if (!error)
		error = nfs3_putpage(vp, poff, plen, 0, cr);

	if (error)
		return (error);

	mutex_enter(&rp->r_statelock);
	if (bcmp((caddr_t)&rp->r_verf, (caddr_t)&write_verf,
			sizeof (writeverf3)) != 0) {
		mutex_exit(&rp->r_statelock);
		goto top;
	}
	mutex_exit(&rp->r_statelock);

	/*
	 * Now commit any pages which might need to be committed.
	 * If the error, NFS_VERF_MISMATCH, is returned, then
	 * start over with the flush operation.
	 */

	error = nfs3_commit_vp(vp, poff, plen, cr);

	if (error == NFS_VERF_MISMATCH)
		goto top;

	return (error);
}

static int
nfs3_commit_vp(vnode_t *vp, u_offset_t poff, size_t plen, cred_t *cr)
{
	rnode_t *rp;
	page_t *plist;
	offset3 offset;
	count3 len;


	rp = VTOR(vp);

	/*
	 * Set the `commit inprogress' state bit.  We must
	 * first wait until any current one finishes.
	 */
	mutex_enter(&rp->r_statelock);
	while (rp->r_flags & RCOMMIT) {
		rp->r_flags |= RCOMMITWAIT;
		cv_wait(&rp->r_commit.c_cv, &rp->r_statelock);
		rp->r_flags &= ~RCOMMITWAIT;
	}
	rp->r_flags |= RCOMMIT;
	mutex_exit(&rp->r_statelock);

	/*
	 * Gather together all of the pages which need to be
	 * committed.
	 */
	if (plen == 0)
		nfs3_get_commit(vp);
	else
		nfs3_get_commit_range(vp, poff, plen);

	/*
	 * Clear the `commit inprogress' bit and disconnect the
	 * page list which was gathered together in nfs3_get_commit.
	 */
	plist = rp->r_commit.c_pages;
	rp->r_commit.c_pages = NULL;
	offset = rp->r_commit.c_commbase;
	len = rp->r_commit.c_commlen;
	mutex_enter(&rp->r_statelock);
	rp->r_flags &= ~RCOMMIT;
	cv_broadcast(&rp->r_commit.c_cv);
	mutex_exit(&rp->r_statelock);

	/*
	 * If any pages need to be committed, commit them and
	 * then unlock them so that they can be freed some
	 * time later.
	 */
	if (plist != NULL) {
		/*
		 * No error occurred during the flush portion
		 * of this operation, so now attempt to commit
		 * the data to stable storage on the server.
		 *
		 * This will unlock all of the pages on the list.
		 */
		return (nfs3_sync_commit(vp, plist, offset, len, cr));
	}
	return (0);
}

static int
nfs3_sync_commit(vnode_t *vp, page_t *plist, offset3 offset, count3 count,
	cred_t *cr)
{
	int error;
	page_t *pp;

	error = nfs3_commit(vp, offset, count, cr);

	/*
	 * If we got an error, then just unlock all of the pages
	 * on the list.
	 */
	if (error) {
		while (plist != NULL) {
			pp = plist;
			page_sub(&plist, pp);
			page_unlock(pp);
		}
		return (error);
	}
	/*
	 * We've tried as hard as we can to commit the data to stable
	 * storage on the server.  We just unlock the pages and clear
	 * the commit required state.  They will get freed later.
	 */
	while (plist != NULL) {
		pp = plist;
		page_sub(&plist, pp);
		pp->p_fsdata = C_NOCOMMIT;
		page_unlock(pp);
	}

	return (error);
}

static void
nfs3_async_commit(vnode_t *vp, page_t *plist, offset3 offset, count3 count,
	cred_t *cr)
{

	(void) nfs3_sync_commit(vp, plist, offset, count, cr);
}

static int
nfs3_setsecattr(vnode_t *vp, vsecattr_t *vsecattr, int flag, cred_t *cr)
{
	int error;
	mntinfo_t *mi;

	mi = VTOMI(vp);

	if (mi->mi_flags & MI_ACL) {
		error = acl_setacl3(vp, vsecattr, flag, cr);
		if (mi->mi_flags & MI_ACL)
			return (error);
	}

	return (ENOSYS);
}

static int
nfs3_getsecattr(vnode_t *vp, vsecattr_t *vsecattr, int flag, cred_t *cr)
{
	int error;
	mntinfo_t *mi;

	mi = VTOMI(vp);

	if (mi->mi_flags & MI_ACL) {
		error = acl_getacl3(vp, vsecattr, flag, cr);
		if (mi->mi_flags & MI_ACL)
			return (error);
	}

	return (fs_fab_acl(vp, vsecattr, flag, cr));
}

static int
nfs3_shrlock(vnode_t *vp, int cmd, struct shrlock *shr, int flag)
{
	int error;
	struct shrlock nshr;
	struct nfs_owner nfs_owner;
	netobj lm_fh3;

	/*
	 * check for valid cmd parameter
	 */
	if (cmd != F_SHARE && cmd != F_UNSHARE && cmd != F_HASREMOTELOCKS)
		return (EINVAL);

	/*
	 * Check access permissions
	 */
	if ((cmd & F_SHARE) &&
	    (((shr->s_access & F_RDACC) && (flag & FREAD) == 0) ||
	    (shr->s_access == F_WRACC && (flag & FWRITE) == 0)))
		return (EBADF);

	/*
	 * If the filesystem is mounted using local locking, pass the
	 * request off to the local share code.
	 */
	if (VTOMI(vp)->mi_flags & MI_LLOCK)
		return (fs_shrlock(vp, cmd, shr, flag));

	switch (cmd) {
	case F_SHARE:
	case F_UNSHARE:
		lm_fh3.n_len = VTOFH3(vp)->fh3_length;
		lm_fh3.n_bytes = (char *)&(VTOFH3(vp)->fh3_u.data);

		/*
		 * If passed an owner that is too large to fit in an
		 * nfs_owner it is likely a recursive call from the
		 * lock manager client and pass it straight through.  If
		 * it is not a nfs_owner then simply return an error.
		 */
		if (shr->s_own_len > sizeof (nfs_owner.lowner)) {
			if (((struct nfs_owner *)shr->s_owner)->magic !=
			    NFS_OWNER_MAGIC)
				return (EINVAL);

			if (error = lm4_shrlock(vp, cmd, shr, flag, &lm_fh3)) {
				error = set_errno(error);
			}
			return (error);
		}
		/*
		 * Remote share reservations owner is a combination of
		 * a magic number, hostname, and the local owner
		 */
		bzero(&nfs_owner, sizeof (nfs_owner));
		nfs_owner.magic = NFS_OWNER_MAGIC;
		(void) strncpy(nfs_owner.hname, utsname.nodename,
		    sizeof (nfs_owner.hname));
		bcopy(shr->s_owner, nfs_owner.lowner, shr->s_own_len);
		nshr.s_access = shr->s_access;
		nshr.s_deny = shr->s_deny;
		nshr.s_sysid = 0;
		nshr.s_pid = ttoproc(curthread)->p_pid;
		nshr.s_own_len = sizeof (nfs_owner);
		nshr.s_owner = (caddr_t)&nfs_owner;

		if (error = lm4_shrlock(vp, cmd, &nshr, flag, &lm_fh3)) {
			error = set_errno(error);
		}

		break;

	case F_HASREMOTELOCKS:
		/*
		 * NFS client can't store remote locks itself
		 */
		shr->s_access = 0;
		error = 0;
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

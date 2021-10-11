/*
 * Copyright (c) 1989-1997, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tmp_vnops.c	1.130	99/11/05 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/cred.h>
#include <sys/dirent.h>
#include <sys/pathname.h>
#include <sys/vmsystm.h>
#include <sys/fs/tmp.h>
#include <sys/fs/tmpnode.h>
#include <sys/mman.h>
#include <vm/hat.h>
#include <vm/seg_vn.h>
#include <vm/seg_map.h>
#include <vm/seg.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/swap.h>
#include <sys/buf.h>
#include <sys/vm.h>
#include <sys/vtrace.h>
#include <fs/fs_subr.h>

#define	ISVDEV(t) ((t == VCHR) || (t == VBLK) || (t == VFIFO))

static int	tmp_getapage(struct vnode *, u_offset_t, size_t, u_int *,
	page_t **, size_t, struct seg *, caddr_t, enum seg_rw, struct cred *);
static int 	tmp_putapage(struct vnode *, page_t *, u_offset_t *, size_t *,
	int, struct cred *);

/* ARGSUSED1 */
static int
tmp_open(struct vnode **vpp, int flag, struct cred *cred)
{
	/*
	 * swapon to a tmpfs file is not supported so access
	 * is denied on open if VISSWAP is set.
	 */
	if ((*vpp)->v_flag & VISSWAP)
		return (EINVAL);
	return (0);
}

/* ARGSUSED1 */
static int
tmp_close(struct vnode *vp, int flag, int count,
    offset_t offset, struct cred *cred)
{
	cleanlocks(vp, ttoproc(curthread)->p_pid, 0);
	cleanshares(vp, ttoproc(curthread)->p_pid);
	return (0);
}

/*
 * wrtmp does the real work of write requests for tmpfs.
 */
static int
wrtmp(
	struct tmount *tm,
	struct tmpnode *tp,
	struct uio *uio,
	struct cred *cr)
{
	pgcnt_t pageoffset;	/* offset in pages */
	u_long segmap_offset;	/* pagesize byte offset into segmap */
	caddr_t base;		/* base of segmap */
	ssize_t bytes;		/* bytes to uiomove */
	pfn_t pagenumber;	/* offset in pages into tmp file */
	struct vnode *vp;
	int error = 0;
	int	pagecreate;	/* == 1 if we allocated a page */
	int	newpage;
	rlim64_t limit = uio->uio_llimit;
	long oresid = uio->uio_resid;
	timestruc_t now;

	/*
	 * tp->tn_size is incremented before the uiomove
	 * is done on a write.  If the move fails (bad user
	 * address) reset tp->tn_size.
	 * The better way would be to increment tp->tn_size
	 * only if the uiomove succeeds.
	 */
	long tn_size_changed = 0;
	long old_tn_size;

	vp = TNTOV(tp);
	ASSERT(vp->v_type == VREG);

	TRACE_1(TR_FAC_TMPFS, TR_TMPFS_RWTMP_START,
		"tmp_wrtmp_start:vp %p", vp);

	ASSERT(RW_WRITE_HELD(&tp->tn_contents));
	ASSERT(RW_WRITE_HELD(&tp->tn_rwlock));

	if (MANDLOCK(vp, tp->tn_mode)) {
		rw_exit(&tp->tn_contents);
		/*
		 * tmp_getattr ends up being called by chklock
		 */
		error = chklock(vp, FWRITE,
			uio->uio_loffset, uio->uio_resid, uio->uio_fmode);
		rw_enter(&tp->tn_contents, RW_WRITER);
		if (error != 0) {
			TRACE_2(TR_FAC_TMPFS, TR_TMPFS_RWTMP_END,
				"tmp_wrtmp_end:vp %p error %d", vp, error);
			return (error);
		}
	}

	if (uio->uio_loffset < 0)
		return (EINVAL);

	if (limit == RLIM64_INFINITY || limit > MAXOFFSET_T)
		limit = MAXOFFSET_T;

	if (uio->uio_loffset >= limit) {
		psignal(ttoproc(curthread), SIGXFSZ);
		return (EFBIG);
	}

	if (uio->uio_loffset >= MAXOFF_T) {
		TRACE_2(TR_FAC_TMPFS, TR_TMPFS_RWTMP_END,
			"tmp_wrtmp_end:vp %p error %d", vp, EINVAL);
		return (EFBIG);
	}

	if (uio->uio_resid == 0) {
		TRACE_2(TR_FAC_TMPFS, TR_TMPFS_RWTMP_END,
			"tmp_wrtmp_end:vp %p error %d", vp, 0);
		return (0);
	}

	if (limit > MAXOFF_T)
		limit = MAXOFF_T;

	do {
		long	offset;
		long	delta;

		offset = (long)uio->uio_offset;
		pageoffset = offset & PAGEOFFSET;
		/*
		 * A maximum of PAGESIZE bytes of data is transferred
		 * each pass through this loop
		 */
		bytes = MIN(PAGESIZE - pageoffset, uio->uio_resid);

		if (offset + bytes >= limit) {
			if (offset >= limit) {
				error = EFBIG;
				goto out;
			}
			bytes = limit - offset;
		}
		pagenumber = btop(offset);

		/*
		 * delta is the amount of anonymous memory
		 * to reserve for the file.
		 * We always reserve in pagesize increments so
		 * unless we're extending the file into a new page,
		 * we don't need to call tmp_resv.
		 */
		delta = offset + bytes - roundup(tp->tn_size, PAGESIZE);
		if (delta > 0) {
			pagecreate = 1;
			if (tmp_resv(tm, tp, delta, pagecreate)) {
				cmn_err(CE_WARN,
	"%s: File system full, swap space limit exceeded",
				    tm->tm_mntpath);
				error = ENOSPC;
				break;
			}
			tmpnode_growmap(tp, (u_long)offset + bytes);
		}
		/* grow the file to the new length */
		if (offset + bytes > tp->tn_size) {
			tn_size_changed = 1;
			old_tn_size = tp->tn_size;
			tp->tn_size = offset + bytes;
		}
		if (bytes == PAGESIZE) {
			/*
			 * Writing whole page so reading from disk
			 * is a waste
			 */
			pagecreate = 1;
		} else {
			pagecreate = 0;
		}
		/*
		 * If writing past EOF or filling in a hole
		 * we need to allocate an anon slot.
		 */
		if (anon_get_ptr(tp->tn_anon, pagenumber) == NULL) {
			(void) anon_set_ptr(tp->tn_anon, pagenumber,
				anon_alloc(vp, ptob(pagenumber)), ANON_SLEEP);
			pagecreate = 1;
			tp->tn_nblocks++;
		}

		/*
		 * We have to drop the contents lock to prevent the VM
		 * system from trying to reaquire it in tmp_getpage()
		 * should the uiomove cause a pagefault. If we're doing
		 * a pagecreate segmap creates the page without calling
		 * the filesystem so we need to hold onto the lock until
		 * the page is created.
		 */
		if (!pagecreate)
			rw_exit(&tp->tn_contents);

		/* Get offset within the segmap mapping */
		segmap_offset = (offset & PAGEMASK) & MAXBOFFSET;
		base = segmap_getmapflt(segkmap, vp, (offset &  MAXBMASK),
		    PAGESIZE, !pagecreate, S_WRITE);

		newpage = 0;

		if (pagecreate) {
			rw_downgrade(&tp->tn_contents);

			/*
			 * segmap_pagecreate() returns 1 if it calls
			 * page_create_va() to allocate any pages.
			 */
			newpage = segmap_pagecreate(segkmap,
			    base + segmap_offset, (size_t)PAGESIZE, 0);
			rw_exit(&tp->tn_contents);
			/*
			 * Clear from the beginning of the page to the starting
			 * offset of the data.
			 */
			if (pageoffset != 0)
				(void) kzero(base + segmap_offset,
				    (size_t)pageoffset);
		}

		error = uiomove(base + segmap_offset + pageoffset,
			(long)bytes, UIO_WRITE, uio);

		if (pagecreate &&
		    uio->uio_offset < roundup(offset + bytes, PAGESIZE)) {
			long	zoffset; /* zero from offset into page */
			/*
			 * We created pages w/o initializing them completely,
			 * thus we need to zero the part that wasn't set up.
			 * This happens on most EOF write cases and if
			 * we had some sort of error during the uiomove.
			 */
			long nmoved;

			nmoved = uio->uio_offset - offset;
			ASSERT((nmoved + pageoffset) <= PAGESIZE);

			/*
			 * Zero from the end of data in the page to the
			 * end of the page.
			 */
			if ((zoffset = pageoffset + nmoved) < PAGESIZE)
				(void) kzero(base + segmap_offset + zoffset,
				    (size_t)PAGESIZE - zoffset);
		}

		/*
		 * Unlock the pages which have been allocated by
		 * page_create_va() in segmap_pagecreate()
		 */
		if (newpage)
			segmap_pageunlock(segkmap, base + segmap_offset,
			    (size_t)PAGESIZE, S_WRITE);

		if (error) {
			/*
			 * If we failed on a write, we must
			 * be sure to invalidate any pages that may have
			 * been allocated.
			 */
			(void) segmap_release(segkmap, base, SM_INVAL);
		} else {
			error = segmap_release(segkmap, base, 0);
		}

		/*
		 * Re-acquire contents lock.
		 */
		rw_enter(&tp->tn_contents, RW_WRITER);
		/*
		 * If the uiomove failed, fix up tn_size.
		 */
		if (error) {
			if (tn_size_changed) {
				/*
				 * The uiomove failed, and we
				 * allocated blocks,so get rid
				 * of them.
				 */
				(void) tmpnode_trunc(tm, tp,
				    (u_long)old_tn_size);
			}
		} else {
			/*
			 * XXX - Can this be out of the loop?
			 */
			if (cr->cr_uid != 0 && (tp->tn_mode &
			    (S_IXUSR | S_IXGRP | S_IXOTH)) != 0) {
				/*
				 * Clear Set-UID & Set-GID bits on
				 * successful write if not super-user
				 * and at least one of the execute bits
				 * is set.  If we always clear Set-GID,
				 * mandatory file and record locking is
				 * unuseable.
				 */
				tp->tn_mode &= ~(S_ISUID | S_ISGID);
			}
			now = hrestime;
			tp->tn_mtime = now;
			tp->tn_ctime = now;
		}
	} while (error == 0 && uio->uio_resid > 0 && bytes != 0);

out:
	/*
	 * If we've already done a partial-write, terminate
	 * the write but return no error.
	 */
	if (oresid != uio->uio_resid)
		error = 0;
	TRACE_2(TR_FAC_TMPFS, TR_TMPFS_RWTMP_END,
		"tmp_wrtmp_end:vp %p error %d", vp, error);
	return (error);
}

/*
 * rdtmp does the real work of read requests for tmpfs.
 */
static int
rdtmp(
	struct tmount *tm,
	struct tmpnode *tp,
	struct uio *uio)
{
	u_long pageoffset;	/* offset in tmpfs file (uio_offset) */
	u_long segmap_offset;	/* pagesize byte offset into segmap */
	caddr_t base;		/* base of segmap */
	ssize_t bytes;		/* bytes to uiomove */
	struct vnode *vp;
	int error;
	long oresid = uio->uio_resid;

#if defined(lint)
	tm = tm;
#endif
	vp = TNTOV(tp);

	TRACE_1(TR_FAC_TMPFS, TR_TMPFS_RWTMP_START,
		"tmp_rdtmp_start:vp %p", vp);

	ASSERT(RW_LOCK_HELD(&tp->tn_contents));

	if (MANDLOCK(vp, tp->tn_mode)) {
		rw_exit(&tp->tn_contents);
		/*
		 * tmp_getattr ends up being called by chklock
		 */
		error = chklock(vp, FREAD,
			uio->uio_loffset, uio->uio_resid, uio->uio_fmode);
		rw_enter(&tp->tn_contents, RW_READER);
		if (error != 0) {
			TRACE_2(TR_FAC_TMPFS, TR_TMPFS_RWTMP_END,
				"tmp_rdtmp_end:vp %p error %d", vp, error);
			return (error);
		}
	}
	ASSERT(tp->tn_type == VREG);

	if (uio->uio_loffset >= MAXOFF_T) {
		TRACE_2(TR_FAC_TMPFS, TR_TMPFS_RWTMP_END,
			"tmp_rdtmp_end:vp %p error %d", vp, EINVAL);
		return (0);
	}
	if (uio->uio_loffset < 0)
		return (EINVAL);
	if (uio->uio_resid == 0) {
		TRACE_2(TR_FAC_TMPFS, TR_TMPFS_RWTMP_END,
			"tmp_rdtmp_end:vp %p error %d", vp, 0);
		return (0);
	}

	vp = TNTOV(tp);

	do {
		long diff;
		long offset;

		offset = uio->uio_offset;
		pageoffset = offset & PAGEOFFSET;
		bytes = MIN(PAGESIZE - pageoffset, uio->uio_resid);

		diff = tp->tn_size - offset;

		if (diff <= 0) {
			error = 0;
			goto out;
		}
		if (diff < bytes)
			bytes = diff;

		/*
		 * We have to drop the contents lock to prevent the VM
		 * system from trying to reaquire it in tmp_getpage()
		 * should the uiomove cause a pagefault.
		 */
		rw_exit(&tp->tn_contents);

		segmap_offset = (offset & PAGEMASK) & MAXBOFFSET;
		base = segmap_getmapflt(segkmap, vp, offset & MAXBMASK,
		    bytes, 1, S_READ);

		error = uiomove(base + segmap_offset + pageoffset,
		    (long)bytes, UIO_READ, uio);

		if (error)
			(void) segmap_release(segkmap, base, 0);
		else
			error = segmap_release(segkmap, base, 0);

		/*
		 * Re-acquire contents lock.
		 */
		rw_enter(&tp->tn_contents, RW_READER);

	} while (error == 0 && uio->uio_resid > 0);

out:
	tp->tn_atime = hrestime;

	/*
	 * If we've already done a partial read, terminate
	 * the read but return no error.
	 */
	if (oresid != uio->uio_resid)
		error = 0;

	TRACE_2(TR_FAC_TMPFS, TR_TMPFS_RWTMP_END,
		"tmp_rdtmp_end:vp %x error %d", vp, error);
	return (error);
}

/* ARGSUSED2 */
static int
tmp_read(struct vnode *vp, struct uio *uiop, int ioflag, cred_t *cred)
{
	struct tmpnode *tp = (struct tmpnode *)VTOTN(vp);
	struct tmount *tm = (struct tmount *)VTOTM(vp);
	int error;

	/*
	 * We don't currently support reading non-regular files
	 */
	if (vp->v_type != VREG)
		return (EINVAL);
	/*
	 * tmp_rwlock should have already been called from layers above
	 */
	ASSERT(RW_READ_HELD(&tp->tn_rwlock));

	rw_enter(&tp->tn_contents, RW_READER);

	error = rdtmp(tm, tp, uiop);

	rw_exit(&tp->tn_contents);

	return (error);
}

static int
tmp_write(struct vnode *vp, struct uio *uiop, int ioflag, struct cred *cred)
{
	struct tmpnode *tp = (struct tmpnode *)VTOTN(vp);
	struct tmount *tm = (struct tmount *)VTOTM(vp);
	int error;

	/*
	 * We don't currently support writing to non-regular files
	 */
	if (vp->v_type != VREG)
		return (EINVAL);	/* XXX EISDIR? */

	/*
	 * tmp_rwlock should have already been called from layers above
	 */
	ASSERT(RW_WRITE_HELD(&tp->tn_rwlock));

	rw_enter(&tp->tn_contents, RW_WRITER);

	if (ioflag & FAPPEND) {
		/*
		 * In append mode start at end of file.
		 */
		uiop->uio_loffset = tp->tn_size;
	}

	error = wrtmp(tm, tp, uiop, cred);

	rw_exit(&tp->tn_contents);

	return (error);
}

/* ARGSUSED */
static int
tmp_ioctl(struct vnode *vp, int com, intptr_t data, int flag,
    struct cred *cred, int *rvalp)
{
	return (ENOTTY);
}

/* ARGSUSED2 */
static int
tmp_getattr(struct vnode *vp, struct vattr *vap, int flags, struct cred *cred)
{
	struct tmpnode *tp = (struct tmpnode *)VTOTN(vp);

	mutex_enter(&tp->tn_tlock);
	vap->va_mask = tp->tn_mask;
	vap->va_type = vp->v_type;
	vap->va_mode = tp->tn_mode & MODEMASK;
	vap->va_uid = tp->tn_uid;
	vap->va_gid = tp->tn_gid;
	vap->va_fsid = tp->tn_fsid;
	vap->va_nodeid = (ino64_t)tp->tn_nodeid;
	vap->va_nlink = tp->tn_nlink;
	vap->va_size = (u_offset_t)tp->tn_size;
	vap->va_atime = tp->tn_atime;
	vap->va_mtime = tp->tn_mtime;
	vap->va_ctime = tp->tn_ctime;
	vap->va_blksize = PAGESIZE;
	vap->va_rdev = tp->tn_rdev;
	vap->va_vcode = tp->tn_vcode;

	/*
	 * XXX Holes are not taken into account.  We could take the time to
	 * run through the anon array looking for allocated slots...
	 */
	vap->va_nblocks = (fsblkcnt64_t)btodb(ptob(btopr(vap->va_size)));
	mutex_exit(&tp->tn_tlock);
	return (0);
}

static int
tmp_setattr(struct vnode *vp, struct vattr *vap, int flags, struct cred *cred)
{
	struct tmount *tm = (struct tmount *)VTOTM(vp);
	struct tmpnode *tp = (struct tmpnode *)VTOTN(vp);
	int error = 0;
	struct vattr *get;
	register long int mask = vap->va_mask;
	int checksu = 0;

	/*
	 * Cannot set these attributes
	 */
	if (mask & AT_NOSET)
		return (EINVAL);

	mutex_enter(&tp->tn_tlock);

	get = &tp->tn_attr;
	/*
	 * Change file access modes. Must be owner or super-user.
	 */
	if (mask & AT_MODE) {
		if (cred->cr_uid != 0 && get->va_uid != cred->cr_uid) {
			error = EPERM;
			goto out;
		}
		get->va_mode &= S_IFMT;
		get->va_mode |= vap->va_mode & ~S_IFMT;
		if (cred->cr_uid != 0) {
			if (vp->v_type != VDIR)
				get->va_mode &= ~S_ISVTX;
			if (!groupmember((uid_t)get->va_gid, cred))
				get->va_mode &= ~S_ISGID;
		}
		tp->tn_ctime = hrestime;
	}
	if (mask & (AT_UID | AT_GID)) {
		if (cred->cr_uid != get->va_uid)
			checksu = 1;
		else {
			if (rstchown) {
				if (((mask & AT_UID) &&
				    vap->va_uid != get->va_uid) ||
				    ((mask & AT_GID) &&
				    !groupmember(vap->va_gid, cred)))
					checksu = 1;
			}
		}
		if (checksu && !suser(cred)) {
			error = EPERM;
			goto out;
		}
		if (cred->cr_uid != 0)
			get->va_mode &= ~(S_ISUID|S_ISGID);
		if (mask & AT_UID)
			get->va_uid = vap->va_uid;
		if (mask & AT_GID)
			get->va_gid = vap->va_gid;
		tp->tn_ctime = hrestime;
	}
	if (mask & (AT_ATIME | AT_MTIME)) {
		if (cred->cr_uid != get->va_uid && cred->cr_uid != 0) {
			if (flags & ATTR_UTIME)
				error = EPERM;
			else
				error = tmp_taccess(tp, VWRITE, cred);
			if (error)
				goto out;
		}
		if (mask & AT_ATIME) {
			get->va_atime = vap->va_atime;
		}
		if (mask & AT_MTIME) {
			get->va_mtime = vap->va_mtime;
			get->va_ctime = hrestime;
		}
	}
	if (mask & AT_SIZE) {
		if (vp->v_type == VDIR) {
			error =  EISDIR;
			goto out;
		}
		/* Don't support large files. */
		if (vap->va_size > MAXOFF_T) {
			error = EFBIG;
			goto out;
		}
		if (error = tmp_taccess(tp, VWRITE, cred))
			goto out;
		mutex_exit(&tp->tn_tlock);

		rw_enter(&tp->tn_rwlock, RW_WRITER);
		rw_enter(&tp->tn_contents, RW_WRITER);
		error = tmpnode_trunc(tm, tp, (u_long)vap->va_size);
		rw_exit(&tp->tn_contents);
		rw_exit(&tp->tn_rwlock);
		goto out1;
	}
out:
	mutex_exit(&tp->tn_tlock);
out1:
	return (error);
}

/* ARGSUSED2 */
static int
tmp_access(struct vnode *vp, int mode, int flags, struct cred *cred)
{
	struct tmpnode *tp = (struct tmpnode *)VTOTN(vp);
	int error;

	mutex_enter(&tp->tn_tlock);
	error = tmp_taccess(tp, mode, cred);
	mutex_exit(&tp->tn_tlock);
	return (error);
}

/* ARGSUSED3 */
static int
tmp_lookup(
	struct vnode *dvp,
	char *nm,
	struct vnode **vpp,
	struct pathname *pnp,
	int flags,
	struct vnode *rdir,
	struct cred *cred)
{
	struct tmpnode *tp = (struct tmpnode *)VTOTN(dvp);
	struct tmpnode *ntp = NULL;
	int error;

	/*
	 * Null component name is a synonym for directory being searched.
	 */
	if (*nm == '\0') {
		VN_HOLD(dvp);
		*vpp = dvp;
		return (0);
	}
	ASSERT(tp);

	error = tdirlookup(tp, nm, &ntp, cred);

	if (error == 0) {
		ASSERT(ntp);
		*vpp = TNTOV(ntp);
		/*
		 * If vnode is a device return special vnode instead
		 */
		if (ISVDEV((*vpp)->v_type)) {
			struct vnode *newvp;

			newvp = specvp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type,
			    cred);
			VN_RELE(*vpp);
			*vpp = newvp;
		}
	}
	TRACE_4(TR_FAC_TMPFS, TR_TMPFS_LOOKUP,
	    "tmpfs lookup:vp %p name %s vp %p error %d",
	    (void *)dvp, nm, (void *)*vpp, error);
	return (error);
}

/*ARGSUSED7*/
static int
tmp_create(
	struct vnode *dvp,
	char *nm,
	struct vattr *vap,
	enum vcexcl exclusive,
	int mode,
	struct vnode **vpp,
	struct cred *cred,
	int flag)
{
	struct tmpnode *parent;
	struct tmount *tm;
	struct tmpnode *self;
	int error;
	struct tmpnode *oldtp;

again:
	parent = (struct tmpnode *)VTOTN(dvp);
	tm = (struct tmount *)VTOTM(dvp);
	self = NULL;
	error = 0;
	oldtp = NULL;

	if (vap->va_type == VREG) {
		/* Must be super-user to set sticky bit */
		if (cred->cr_uid != 0)
			vap->va_mode &= ~VSVTX;
	} else if (vap->va_type == VNON) {
		return (EINVAL);
	}

	/*
	 * Null component name is a synonym for directory being searched.
	 */
	if (*nm == '\0') {
		VN_HOLD(dvp);
		oldtp = parent;
	} else {
		error = tdirlookup(parent, nm, &oldtp, cred);
	}

	if (error == 0) {	/* name found */
		ASSERT(oldtp);

		rw_enter(&oldtp->tn_rwlock, RW_WRITER);

		/*
		 * if create/read-only an existing
		 * directory, allow it
		 */
		if (exclusive == EXCL)
			error = EEXIST;
		else if ((oldtp->tn_type == VDIR) && (mode & VWRITE))
			error = EISDIR;
		else {
			error = tmp_taccess(oldtp, mode, cred);
		}

		if (error) {
			rw_exit(&oldtp->tn_rwlock);
			tmpnode_rele(oldtp);
			return (error);
		}
		*vpp = TNTOV(oldtp);
		if ((*vpp)->v_type == VREG && (vap->va_mask & AT_SIZE) &&
		    vap->va_size == 0) {
			rw_enter(&oldtp->tn_contents, RW_WRITER);
			(void) tmpnode_trunc(tm, oldtp, 0);
			rw_exit(&oldtp->tn_contents);
		}
		rw_exit(&oldtp->tn_rwlock);
		if (ISVDEV((*vpp)->v_type)) {
			struct vnode *newvp;

			newvp = specvp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type,
			    cred);
			VN_RELE(*vpp);
			if (newvp == NULL) {
				return (ENOSYS);
			}
			*vpp = newvp;
		}
		return (0);
	}

	if (error != ENOENT)
		return (error);

	rw_enter(&parent->tn_rwlock, RW_WRITER);
	error = tdirenter(tm, parent, nm, DE_CREATE,
	    (struct tmpnode *)NULL, (struct tmpnode *)NULL,
	    vap, &self, cred);
	rw_exit(&parent->tn_rwlock);

	if (error) {
		if (self)
			tmpnode_rele(self);

		if (error == EEXIST) {
			/*
			 * This means that the file was created sometime
			 * after we checked and did not find it and when
			 * we went to create it.
			 * Since creat() is supposed to truncate a file
			 * that already exits go back to the begining
			 * of the function. This time we will find it
			 * and go down the tmp_trunc() path
			 */
			goto again;
		}
		return (error);
	}

	*vpp = TNTOV(self);

	if (!error && ISVDEV((*vpp)->v_type)) {
		struct vnode *newvp;

		newvp = specvp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type, cred);
		VN_RELE(*vpp);
		if (newvp == NULL)
			return (ENOSYS);
		*vpp = newvp;
	}
	TRACE_3(TR_FAC_TMPFS, TR_TMPFS_CREATE,
		"tmpfs create:dvp %p nm %s vp %p", dvp, nm, *vpp);
	return (0);
}

static int
tmp_remove(struct vnode *dvp, char *nm, struct cred *cred)
{
	struct tmpnode *parent = (struct tmpnode *)VTOTN(dvp);
	int error;
	struct tmpnode *tp = NULL;

	error = tdirlookup(parent, nm, &tp, cred);
	if (error)
		return (error);

	ASSERT(tp);
	rw_enter(&parent->tn_rwlock, RW_WRITER);
	rw_enter(&tp->tn_rwlock, RW_WRITER);
	if (tp->tn_type == VDIR && !suser(cred)) {
		error = EPERM;
		goto done;
	}

	error = tdirdelete(parent, tp, nm, DR_REMOVE, cred);
done:
	rw_exit(&tp->tn_rwlock);
	rw_exit(&parent->tn_rwlock);
	tmpnode_rele(tp);

	TRACE_3(TR_FAC_TMPFS, TR_TMPFS_REMOVE,
		"tmpfs remove:dvp %p nm %s error %d", dvp, nm, error);
	return (error);
}

static int
tmp_link(struct vnode *dvp, struct vnode *srcvp, char *tnm, struct cred *cred)
{
	struct tmpnode *parent;
	struct tmpnode *from;
	struct tmount *tm = (struct tmount *)VTOTM(dvp);
	int error;
	struct tmpnode *found = NULL;
	struct vnode *realvp;

	if (VOP_REALVP(srcvp, &realvp) == 0)
		srcvp = realvp;

	parent = (struct tmpnode *)VTOTN(dvp);
	from = (struct tmpnode *)VTOTN(srcvp);

	if (srcvp->v_type == VDIR && !suser(cred))
		return (EPERM);

	error = tdirlookup(parent, tnm, &found, cred);
	if (error == 0) {
		ASSERT(found);
		tmpnode_rele(found);
		return (EEXIST);
	}

	if (error != ENOENT)
		return (error);

	rw_enter(&parent->tn_rwlock, RW_WRITER);
	error = tdirenter(tm, parent, tnm, DE_LINK, (struct tmpnode *)NULL,
		from, NULL, (struct tmpnode **)NULL, cred);
	rw_exit(&parent->tn_rwlock);
	return (error);
}

static int
tmp_rename(
	struct vnode *odvp,	/* source parent vnode */
	char *onm,		/* source name */
	struct vnode *ndvp,	/* destination parent vnode */
	char *nnm,		/* destination name */
	struct cred *cred)
{
	struct tmpnode *fromparent;
	struct tmpnode *toparent;
	struct tmpnode *fromtp = NULL;	/* source tmpnode */
	struct tmount *tm = (struct tmount *)VTOTM(odvp);
	int error;
	int samedir = 0;	/* set if odvp == ndvp */
	struct vnode *realvp;

	if (VOP_REALVP(ndvp, &realvp) == 0)
		ndvp = realvp;

	fromparent = (struct tmpnode *)VTOTN(odvp);
	toparent = (struct tmpnode *)VTOTN(ndvp);

	mutex_enter(&tm->tm_renamelck);

	/*
	 * Look up tmpnode of file we're supposed to rename.
	 */
	error = tdirlookup(fromparent, onm, &fromtp, cred);
	if (error) {
		mutex_exit(&tm->tm_renamelck);
		return (error);
	}

	/*
	 * Make sure we can delete the old (source) entry.  This
	 * requires write permission on the containing directory.  If
	 * that directory is "sticky" it requires further checks.
	 */
	if (((error = tmp_taccess(fromparent, VWRITE, cred)) != 0) ||
	    (error = tmp_sticky_remove_access(fromparent, fromtp, cred)) != 0)
		goto done;

	/*
	 * Check for renaming to or from '.' or '..' or that
	 * fromtp == fromparent
	 */
	if ((onm[0] == '.' &&
	    (onm[1] == '\0' || (onm[1] == '.' && onm[2] == '\0'))) ||
	    (nnm[0] == '.' &&
	    (nnm[1] == '\0' || (nnm[1] == '.' && nnm[2] == '\0'))) ||
	    (fromparent == fromtp)) {
		error = EINVAL;
		goto done;
	}

	samedir = (fromparent == toparent);
	/*
	 * Make sure we can search and rename into the new
	 * (destination) directory.
	 */
	if (!samedir) {
		error = tmp_taccess(toparent, VEXEC|VWRITE, cred);
		if (error)
			goto done;
	}

	/*
	 * Link source to new target
	 */
	rw_enter(&toparent->tn_rwlock, RW_WRITER);
	error = tdirenter(tm, toparent, nnm, DE_RENAME,
	    fromparent, fromtp, (struct vattr *)NULL,
	    (struct tmpnode **)NULL, cred);
	rw_exit(&toparent->tn_rwlock);

	if (error) {
		/*
		 * ESAME isn't really an error; it indicates that the
		 * operation should not be done because the source and target
		 * are the same file, but that no error should be reported.
		 */
		if (error == ESAME)
			error = 0;
		goto done;
	}

	/*
	 * Unlink from source.
	 */
	rw_enter(&fromparent->tn_rwlock, RW_WRITER);
	rw_enter(&fromtp->tn_rwlock, RW_WRITER);

	error = tdirdelete(fromparent, fromtp, onm, DR_RENAME, cred);

	/*
	 * The following handles the case where our source tmpnode was
	 * removed before we got to it.
	 *
	 * XXX We should also cleanup properly in the case where tdirdelete
	 * fails for some other reason.  Currently this case shouldn't happen.
	 * (see 1184991).
	 */
	if (error == ENOENT)
		error = 0;

	rw_exit(&fromtp->tn_rwlock);
	rw_exit(&fromparent->tn_rwlock);
done:
	tmpnode_rele(fromtp);
	mutex_exit(&tm->tm_renamelck);

	TRACE_5(TR_FAC_TMPFS, TR_TMPFS_RENAME,
		"tmpfs rename:ovp %p onm %s nvp %p nnm %s error %d",
		odvp, onm, ndvp, nnm, error);
	return (error);
}

static int
tmp_mkdir(
	struct vnode *dvp,
	char *nm,
	struct vattr *va,
	struct vnode **vpp,
	struct cred *cred)
{
	struct tmpnode *parent = (struct tmpnode *)VTOTN(dvp);
	struct tmpnode *self = NULL;
	struct tmount *tm = (struct tmount *)VTOTM(dvp);
	int error;

	/*
	 * Make sure we have write permissions on the current directory
	 * before we allocate anything
	 */
	if (error = tmp_taccess(parent, VWRITE, cred))
		return (error);

	/*
	 * Might be dangling directory.  Catch it here,
	 * because a ENOENT return from tdirlookup() is
	 * an "o.k. return".
	 */
	if (parent->tn_nlink == 0)
		return (ENOENT);

	error = tdirlookup(parent, nm, &self, cred);
	if (error == 0) {
		ASSERT(self);
		tmpnode_rele(self);
		return (EEXIST);
	}
	if (error != ENOENT)
		return (error);

	rw_enter(&parent->tn_rwlock, RW_WRITER);
	error = tdirenter(tm, parent, nm, DE_MKDIR,
		(struct tmpnode *)NULL, (struct tmpnode *)NULL, va,
		&self, cred);
	if (error) {
		rw_exit(&parent->tn_rwlock);
		if (self)
			tmpnode_rele(self);
		return (error);
	}
	rw_exit(&parent->tn_rwlock);
	*vpp = TNTOV(self);
	return (0);
}

static int
tmp_rmdir(
	struct vnode *dvp,
	char *nm,
	struct vnode *cdir,
	struct cred *cred)
{
	struct tmpnode *parent = (struct tmpnode *)VTOTN(dvp);
	struct tmpnode *self = NULL;
	struct vnode *vp;
	int error = 0;

	/*
	 * Return error when removing . and ..
	 */
	if (strcmp(nm, ".") == 0)
		return (EINVAL);
	if (strcmp(nm, "..") == 0)
		return (EEXIST); /* Should be ENOTEMPTY */
	error = tdirlookup(parent, nm, &self, cred);
	if (error)
		return (error);

	rw_enter(&parent->tn_rwlock, RW_WRITER);
	rw_enter(&self->tn_rwlock, RW_WRITER);
	if ((parent->tn_mode & S_ISVTX) && cred->cr_uid != 0 &&
	    cred->cr_uid != parent->tn_uid && self->tn_uid != cred->cr_uid &&
	    (error = tmp_taccess(self, VWRITE, cred) != 0)) {
		goto done1;
	}
	vp = TNTOV(self);
	if (vp == dvp || vp == cdir) {
		error = EINVAL;
		goto done1;
	}
	if (self->tn_type != VDIR) {
		error = ENOTDIR;
		goto done1;
	}

	mutex_enter(&self->tn_tlock);
	if (self->tn_nlink > 2) {
		mutex_exit(&self->tn_tlock);
		error = EEXIST;
		goto done1;
	}
	mutex_exit(&self->tn_tlock);

	if (vn_vfslock(vp)) {
		error = EBUSY;
		goto done1;
	}
	if (vp->v_vfsmountedhere != NULL) {
		error = EBUSY;
		goto done;
	}

	/*
	 * Check for an empty directory
	 * i.e. only includes entries for "." and ".."
	 */
	if (self->tn_dirents > 2) {
		error = EEXIST;		/* SIGH should be ENOTEMPTY */
		/*
		 * Update atime because checking tn_dirents is logically
		 * equivalent to reading the directory
		 */
		self->tn_atime = hrestime;
		goto done;
	}

	error = tdirdelete(parent, self, nm, DR_RMDIR, cred);
done:
	vn_vfsunlock(vp);
done1:
	rw_exit(&self->tn_rwlock);
	rw_exit(&parent->tn_rwlock);
	tmpnode_rele(self);

	return (error);
}

/* ARGSUSED2 */

static int
tmp_readdir(struct vnode *vp, struct uio *uiop, struct cred *cred, int *eofp)
{
	struct tmpnode *tp = (struct tmpnode *)VTOTN(vp);
	struct tdirent *tdp;
	int error;
	size_t namelen;
	register struct dirent64 *dp;
	register u_long offset;
	register u_long total_bytes_wanted;
	register long outcount = 0;
	register long bufsize;
	int reclen;
	caddr_t outbuf;

	if (uiop->uio_loffset >= MAXOFF_T) {
		if (eofp)
			*eofp = 1;
		return (0);
	}
	/*
	 * assuming system call has already called tmp_rwlock
	 */
	ASSERT(RW_READ_HELD(&tp->tn_rwlock));

	if (uiop->uio_iovcnt != 1)
		return (EINVAL);

	if (vp->v_type != VDIR)
		return (ENOTDIR);

	/*
	 * There's a window here where someone could have removed
	 * all the entries in the directory after we put a hold on the
	 * vnode but before we grabbed the rwlock.  Just return.
	 */
	if (tp->tn_dir == NULL) {
		if (tp->tn_nlink)
			cmn_err(CE_PANIC, "empty directory 0x%p", (void *)tp);
		return (0);
	}

	/*
	 * Get space for multiple directory entries
	 */
	total_bytes_wanted = uiop->uio_iov->iov_len;
	bufsize = total_bytes_wanted + sizeof (struct dirent64);
	outbuf = kmem_zalloc(bufsize, KM_SLEEP);

	dp = (struct dirent64 *)outbuf;


	offset = 0;
	tdp = tp->tn_dir;
	while (tdp) {
		namelen = strlen(tdp->td_name) + 1;
		offset = tdp->td_offset;
		if (offset >= uiop->uio_offset) {
			reclen = (int)DIRENT64_RECLEN(namelen);
			if (outcount + reclen > total_bytes_wanted)
				break;
			ASSERT(tdp->td_tmpnode != NULL);
			(void) strcpy(dp->d_name, tdp->td_name);
			dp->d_reclen = (u_short)reclen;
			dp->d_ino = (ino64_t)tdp->td_tmpnode->tn_nodeid;
			dp->d_off = (offset_t)tdp->td_offset + 1;
			dp = (struct dirent64 *)
			    ((uintptr_t)dp + dp->d_reclen);
			outcount += reclen;
			ASSERT(outcount <= bufsize);
		}
		tdp = tdp->td_next;
	}
	error = uiomove(outbuf, outcount, UIO_READ, uiop);
	if (!error) {
		/* If we reached the end of the list our offset */
		/* should now be just past the end. */
		if (!tdp) {
			offset += 1;
			if (eofp)
				*eofp = 1;
		} else if (eofp)
			*eofp = 0;
		uiop->uio_offset = offset;
	}
	tp->tn_atime = hrestime;
	kmem_free(outbuf, bufsize);
	return (error);
}

static int
tmp_symlink(
	struct vnode *dvp,
	char *lnm,
	struct vattr *tva,
	char *tnm,
	struct cred *cred)
{
	struct tmpnode *parent = (struct tmpnode *)VTOTN(dvp);
	struct tmpnode *self = (struct tmpnode *)NULL;
	struct tmount *tm = (struct tmount *)VTOTM(dvp);
	char *cp = NULL;
	int error;
	size_t len;

	error = tdirlookup(parent, lnm, &self, cred);
	if (error == 0) {
		/*
		 * The entry already exists
		 */
		tmpnode_rele(self);
		return (EEXIST);	/* was 0 */
	}

	if (error != ENOENT) {
		if (self != NULL)
			tmpnode_rele(self);
		return (error);
	}

	rw_enter(&parent->tn_rwlock, RW_WRITER);
	error = tdirenter(tm, parent, lnm, DE_CREATE, (struct tmpnode *)NULL,
	    (struct tmpnode *)NULL, tva, &self, cred);
	rw_exit(&parent->tn_rwlock);

	if (error) {
		if (self)
			tmpnode_rele(self);
		return (error);
	}
	len = strlen(tnm) + 1;
	cp = tmp_memalloc(len, 0);
	if (cp == NULL) {
		tmpnode_rele(self);
		return (ENOSPC);
	}
	(void) strcpy(cp, tnm);

	self->tn_symlink = cp;
	self->tn_size = len - 1;
	tmpnode_rele(self);
	return (error);
}

/* ARGSUSED2 */
static int
tmp_readlink(struct vnode *vp, struct uio *uiop, struct cred *cred)
{
	struct tmpnode *tp = (struct tmpnode *)VTOTN(vp);
	int error = 0;

	if (vp->v_type != VLNK)
		return (EINVAL);

	rw_enter(&tp->tn_rwlock, RW_READER);
	rw_enter(&tp->tn_contents, RW_READER);
	error = uiomove(tp->tn_symlink, tp->tn_size, UIO_READ, uiop);
	tp->tn_atime = hrestime;
	rw_exit(&tp->tn_contents);
	rw_exit(&tp->tn_rwlock);
	return (error);
}

/* ARGSUSED */
static int
tmp_fsync(struct vnode *vp, int syncflag, struct cred *cred)
{
	return (0);
}

/* ARGSUSED */
static void
tmp_inactive(struct vnode *vp, struct cred *cred)
{
	struct tmpnode *tp = (struct tmpnode *)VTOTN(vp);
	struct tmount *tm = (struct tmount *)VFSTOTM(vp->v_vfsp);

	rw_enter(&tp->tn_rwlock, RW_WRITER);
top:
	mutex_enter(&tp->tn_tlock);
	mutex_enter(&vp->v_lock);
	ASSERT(vp->v_count >= 1);

	/*
	 * If we don't have the last hold or the link count is non-zero,
	 * there's little to do -- just drop our hold.
	 */
	if (vp->v_count > 1 || tp->tn_nlink != 0) {
		vp->v_count--;
		mutex_exit(&vp->v_lock);
		mutex_exit(&tp->tn_tlock);
		rw_exit(&tp->tn_rwlock);
		return;
	}

	/*
	 * We have the last hold *and* the link count is zero, so this
	 * tmpnode is dead from the filesystem's viewpoint.  However,
	 * if the tmpnode has any pages associated with it (i.e. if it's
	 * a normal file with non-zero size), the tmpnode can still be
	 * discovered by pageout or fsflush via the page vnode pointers.
	 * In this case we must drop all our locks, truncate the tmpnode,
	 * and try the whole dance again.
	 */
	if (tp->tn_size != 0) {
		if (tp->tn_type == VREG) {
			mutex_exit(&vp->v_lock);
			mutex_exit(&tp->tn_tlock);
			rw_enter(&tp->tn_contents, RW_WRITER);
			(void) tmpnode_trunc(tm, tp, 0);
			rw_exit(&tp->tn_contents);
			ASSERT(tp->tn_size == 0);
			ASSERT(tp->tn_nblocks == 0);
			goto top;
		}
		if (tp->tn_type == VLNK)
			tmp_memfree(tp->tn_symlink, tp->tn_size + 1);
	}

	mutex_exit(&vp->v_lock);
	mutex_exit(&tp->tn_tlock);
	mutex_enter(&tm->tm_contents);
	if (tp->tn_forw == NULL)
		tm->tm_rootnode->tn_back = tp->tn_back;
	else
		tp->tn_forw->tn_back = tp->tn_back;
	tp->tn_back->tn_forw = tp->tn_forw;
	mutex_exit(&tm->tm_contents);
	rw_exit(&tp->tn_rwlock);
	rw_destroy(&tp->tn_rwlock);
	mutex_destroy(&tp->tn_tlock);
	mutex_destroy(&vp->v_lock);
	tmp_memfree(tp, sizeof (struct tmpnode));
}

static int
tmp_fid(struct vnode *vp, struct fid *fidp)
{
	struct tmpnode *tp = (struct tmpnode *)VTOTN(vp);
	struct tfid *tfid;

	if (fidp->fid_len < (sizeof (struct tfid) - sizeof (u_short))) {
		fidp->fid_len = sizeof (struct tfid) - sizeof (u_short);
		return (ENOSPC);
	}

	tfid = (struct tfid *)fidp;
	bzero(tfid, sizeof (struct tfid));
	tfid->tfid_len = (int)sizeof (struct tfid) - sizeof (u_short);

	tfid->tfid_ino = tp->tn_nodeid;
	tfid->tfid_gen = tp->tn_gen;

	return (0);
}


/*
 * Return all the pages from [off..off+len] in given file
 */
static int
tmp_getpage(
	struct vnode *vp,
	offset_t off,
	size_t len,
	u_int *protp,
	page_t *pl[],
	size_t plsz,
	struct seg *seg,
	caddr_t addr,
	enum seg_rw rw,
	struct cred *cr)
{
	int err = 0;
	struct tmpnode *tp = VTOTN(vp);
	anoff_t toff = (anoff_t)off;
	size_t tlen = len;
	u_offset_t tmpoff;
	timestruc_t now;

	rw_enter(&tp->tn_contents, RW_READER);

	if (off + len  > tp->tn_size + PAGEOFFSET) {
		err = EFAULT;
		goto out;
	}
	/*
	 * Look for holes (no anon slot) in faulting range. If there are
	 * holes we have to switch to a write lock and fill them in. Swap
	 * space for holes was already reserved when the file was grown.
	 */
	tmpoff = toff;
	if (non_anon(tp->tn_anon, btop(off), &tmpoff, &tlen)) {
		if (!rw_tryupgrade(&tp->tn_contents)) {
			rw_exit(&tp->tn_contents);
			rw_enter(&tp->tn_contents, RW_WRITER);
			/* Size may have changed when lock was dropped */
			if (off + len  > tp->tn_size + PAGEOFFSET) {
				err = EFAULT;
				goto out;
			}
		}
		for (toff = (anoff_t)off; toff < (anoff_t)off + len;
		    toff += PAGESIZE) {
			if (anon_get_ptr(tp->tn_anon, btop(toff)) == NULL) {
				/* XXX - may allocate mem w. write lock held */
				(void) anon_set_ptr(tp->tn_anon, btop(toff),
						anon_alloc(vp, toff),
						ANON_SLEEP);
				tp->tn_nblocks++;
			}
		}
		rw_downgrade(&tp->tn_contents);
	}


	if (len <= PAGESIZE)
		err = tmp_getapage(vp, (u_offset_t)off, len, protp, pl, plsz,
		    seg, addr, rw, cr);
	else
		err = pvn_getpages(tmp_getapage, vp, (u_offset_t)off, len,
		    protp, pl, plsz, seg, addr, rw, cr);

	now = hrestime;
	tp->tn_atime = now;
	if (rw == S_WRITE)
		tp->tn_mtime = now;

out:
	rw_exit(&tp->tn_contents);
	return (err);
}

/*
 * Called from pvn_getpages or swap_getpage to get a particular page.
 */
/*ARGSUSED*/
static int
tmp_getapage(
	struct vnode *vp,
	u_offset_t off,
	size_t len,
	u_int *protp,
	page_t *pl[],
	size_t plsz,
	struct seg *seg,
	caddr_t addr,
	enum seg_rw rw,
	struct cred *cr)
{
	struct page *pp;
	int flags;
	int err = 0;
	struct vnode *pvp;
	u_offset_t poff;

	if (protp != NULL)
		*protp = PROT_ALL;
again:
	if (pp = page_lookup(vp, off, rw == S_CREATE ? SE_EXCL : SE_SHARED)) {
		if (pl) {
			pl[0] = pp;
			pl[1] = NULL;
		} else {
			page_unlock(pp);
		}
	} else {
		pp = page_create_va(vp, off, PAGESIZE,
		    PG_WAIT | PG_EXCL, seg, addr);
		/*
		 * Someone raced in and created the page after we did the
		 * lookup but before we did the create, so go back and
		 * try to look it up again.
		 */
		if (pp == NULL)
			goto again;
		/*
		 * Fill page from backing store, if any. If none, then
		 * either this is a newly filled hole or page must have
		 * been unmodified and freed so just zero it out.
		 */
		err = swap_getphysname(vp, off, &pvp, &poff);
		if (err) {
			panic("tmp_getapage: no anon slot vp %p "
			    "off %llx pp %p\n", (void *)vp, off, (void *)pp);
		}
		if (pvp) {
			flags = (pl == NULL ? B_ASYNC|B_READ : B_READ);
			err = VOP_PAGEIO(pvp, pp, (u_offset_t)poff, PAGESIZE,
			    flags, cr);
			if (flags & B_ASYNC)
				pp = NULL;
		} else if (rw != S_CREATE) {
			pagezero(pp, 0, PAGESIZE);
		}
		if (err && pp)
			pvn_read_done(pp, B_ERROR);
		if (err == 0) {
			if (pl)
				pvn_plist_init(pp, pl, plsz, off, PAGESIZE, rw);
			else
				pvn_io_done(pp);
		}
	}
	return (err);
}


/*
 * Flags are composed of {B_INVAL, B_DIRTY B_FREE, B_DONTNEED}.
 * If len == 0, do from off to EOF.
 */
static int tmp_nopage = 0;	/* Don't do tmp_putpage's if set */

/* ARGSUSED */
int
tmp_putpage(
	register struct vnode *vp,
	offset_t off,
	size_t len,
	int flags,
	struct cred *cr)
{
	register page_t *pp;
	u_offset_t io_off;
	size_t io_len = 0;
	int err = 0;
	struct tmpnode *tp = VTOTN(vp);
	int dolock;

	if (tmp_nopage)
		return (0);

	ASSERT(vp->v_count != 0);

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	/*
	 * This being tmpfs, we don't ever do i/o unless we really
	 * have to (when we're low on memory and pageout calls us
	 * with B_ASYNC | B_FREE or the user explicitly asks for it with
	 * B_DONTNEED).
	 * XXX to approximately track the mod time like ufs we should
	 * update the times here. The problem is, once someone does a
	 * store we never clear the mod bit and do i/o, thus fsflush
	 * will keep calling us every 30 seconds to do the i/o and we'll
	 * continually update the mod time. At least we update the mod
	 * time on the first store because this results in a call to getpage.
	 */
	if (flags != (B_ASYNC | B_FREE) && (flags & B_INVAL) == 0 &&
		(flags & B_DONTNEED) == 0)
		return (0);
	/*
	 * If this thread owns the lock, i.e., this thread grabbed it
	 * as writer somewhere above, then we don't need to grab the
	 * lock as reader in this routine.
	 */
	dolock = (rw_owner(&tp->tn_contents) != curthread);

	/*
	 * If this is pageout don't block on the lock as you could deadlock
	 * when freemem == 0 (another thread has the read lock and is blocked
	 * creating a page, and a third thread is waiting to get the writers
	 * lock - waiting writers priority blocks us from getting the read
	 * lock). Of course, if the only freeable pages are on this tmpnode
	 * we're hosed anyways. A better solution might be a new lock type.
	 * Note: ufs has the same problem.
	 */
	if (curproc == proc_pageout) {
		if (!rw_tryenter(&tp->tn_contents, RW_READER))
			return (ENOMEM);
	} else if (dolock)
		rw_enter(&tp->tn_contents, RW_READER);

	if (vp->v_pages == NULL)
		goto out;

	if (len == 0) {
		if (curproc == proc_pageout)
			cmn_err(CE_PANIC, "tmp: pageout can't block");

		/* Search the entire vp list for pages >= off. */
		err = pvn_vplist_dirty(vp, (u_offset_t)off, tmp_putapage,
		    flags, cr);
	} else {
		u_offset_t eoff;

		/*
		 * Loop over all offsets in the range [off...off + len]
		 * looking for pages to deal with.
		 */
		eoff = MIN(off + len, tp->tn_size);
		for (io_off = off; io_off < eoff; io_off += io_len) {
			/*
			 * If we are not invalidating, synchronously
			 * freeing or writing pages use the routine
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
				err = tmp_putapage(vp, pp, &io_off, &io_len,
				    flags, cr);
				if (err != 0)
					break;
			}
		}
	}
	/* If invalidating, verify all pages on vnode list are gone. */
	if (err == 0 && off == 0 && len == 0 &&
	    (flags & B_INVAL) && vp->v_pages != NULL) {
		cmn_err(CE_PANIC,
		    "tmp_putpage: B_INVAL, pages not gone");
	}
out:
	if ((curproc == proc_pageout) || dolock)
		rw_exit(&tp->tn_contents);
	/*
	 * Only reason putapage is going to give us SE_NOSWAP as error
	 * is when we ask a page to be written to physical backing store
	 * and there is none. Ignore this because we might be dealing
	 * with a swap page which does not have any backing store
	 * on disk. In any other case we won't get this error over here.
	 */
	if (err == SE_NOSWAP)
		err = 0;
	return (err);
}

long tmp_putpagecnt, tmp_pagespushed;

/*
 * Write out a single page.
 * For tmpfs this means choose a physical swap slot and write the page
 * out using VOP_PAGEIO. For performance, we attempt to kluster; i.e.,
 * we try to find a bunch of other dirty pages adjacent in the file
 * and a bunch of contiguous swap slots, and then write all the pages
 * out in a single i/o.
 */
/*ARGSUSED*/
static int
tmp_putapage(
	struct vnode *vp,
	page_t *pp,
	u_offset_t *offp,
	size_t *lenp,
	int flags,
	struct cred *cr)
{
	int err;
	u_long klstart, kllen;
	page_t *pplist, *npplist;
	extern int klustsize;
	long tmp_klustsize;
	struct tmpnode *tp;
	size_t pp_off, pp_len;
	u_offset_t io_off;
	size_t io_len;
	struct vnode *pvp;
	u_offset_t pstart;
	u_offset_t offset;
	u_offset_t tmpoff;

	ASSERT(PAGE_LOCKED(pp));

	/* Kluster in tmp_klustsize chunks */
	tp = VTOTN(vp);
	tmp_klustsize = klustsize;
	offset = pp->p_offset;
	klstart = (offset / tmp_klustsize) * tmp_klustsize;
	kllen = MIN(tmp_klustsize, tp->tn_size - klstart);

	/* Get a kluster of pages */
	pplist =
	    pvn_write_kluster(vp, pp, &tmpoff, &pp_len, klstart, kllen, flags);

	pp_off = (size_t)tmpoff;

	/*
	 * Get a cluster of physical offsets for the pages; the amount we
	 * get may be some subrange of what we ask for (io_off, io_len).
	 */
	io_off = pp_off;
	io_len = pp_len;
	err = swap_newphysname(vp, offset, &io_off, &io_len, &pvp, &pstart);
	ASSERT(err != SE_NOANON); /* anon slot must have been filled */
	if (err) {
		pvn_write_done(pplist, B_ERROR | B_WRITE | flags);
		/*
		 * If this routine is called as a result of segvn_sync
		 * operation and we have no physical swap then we can get an
		 * error here. In such case we would return SE_NOSWAP as error.
		 * At this point, we expect only SE_NOSWAP.
		 */
		ASSERT(err == SE_NOSWAP);
		if (flags & B_INVAL)
			err = ENOMEM;
		goto out;
	}
	ASSERT(pp_off <= io_off && io_off + io_len <= pp_off + pp_len);
	ASSERT(io_off <= offset && offset < io_off + io_len);

	/* Toss pages at front/rear that we couldn't get physical backing for */
	if (io_off != pp_off) {
		npplist = NULL;
		page_list_break(&pplist, &npplist, btop(io_off - pp_off));
		ASSERT(pplist->p_offset == pp_off);
		ASSERT(pplist->p_prev->p_offset == io_off - PAGESIZE);
		pvn_write_done(pplist, B_ERROR | B_WRITE | flags);
		pplist = npplist;
	}
	if (io_off + io_len < pp_off + pp_len) {
		npplist = NULL;
		page_list_break(&pplist, &npplist, btop(io_len));
		ASSERT(npplist->p_offset == io_off + io_len);
		ASSERT(npplist->p_prev->p_offset == pp_off + pp_len - PAGESIZE);
		pvn_write_done(npplist, B_ERROR | B_WRITE | flags);
	}

	ASSERT(pplist->p_offset == io_off);
	ASSERT(pplist->p_prev->p_offset == io_off + io_len - PAGESIZE);
	ASSERT(btopr(io_len) <= btopr(kllen));

	/* Do i/o on the remaining kluster */
	err = VOP_PAGEIO(pvp, pplist, (u_offset_t)pstart, io_len,
	    B_WRITE | flags, cr);

	if ((flags & B_ASYNC) == 0) {
		pvn_write_done(pplist, ((err) ? B_ERROR : 0) | B_WRITE | flags);
	}
out:
	if (!err) {
		if (offp)
			*offp = io_off;
		if (lenp)
			*lenp = io_len;
		tmp_putpagecnt++;
		tmp_pagespushed += btop(io_len);
	}
	if (err && err != ENOMEM && err != SE_NOSWAP)
		cmn_err(CE_WARN, "tmp_putapage: err %d\n", err);
	return (err);
}

static int
tmp_map(
	struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t *addrp,
	size_t len,
	u_char prot,
	u_char maxprot,
	u_int flags,
	struct cred *cred)
{
	struct segvn_crargs vn_a;
	struct tmpnode *tp = (struct tmpnode *)VTOTN(vp);
	int error;

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	if (off > MAXOFF_T)
		return (EFBIG);

	if ((long)off < 0 || (long)(off + len) < 0)
		return (EINVAL);

	if (vp->v_type != VREG)
		return (ENODEV);

	/*
	 * Don't allow mapping to locked file
	 */
	if (vp->v_filocks != NULL && MANDLOCK(vp, tp->tn_mode)) {
		return (EAGAIN);
	}

	as_rangelock(as);
	if ((flags & MAP_FIXED) == 0) {
		map_addr(addrp, len, (offset_t)off, 1, flags);
		if (*addrp == NULL) {
			as_rangeunlock(as);
			return (ENOMEM);
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
	vn_a.flags = flags & ~MAP_TYPE;
	vn_a.cred = cred;
	vn_a.amp = NULL;

	error = as_map(as, *addrp, len, segvn_create, &vn_a);
	as_rangeunlock(as);
	return (error);
}

/*
 * tmp_addmap and tmp_delmap can't be called since the vp
 * maintained in the segvn mapping is NULL.
 */
/* ARGSUSED */
static int
tmp_addmap(
	struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t len,
	u_char prot,
	u_char maxprot,
	u_int flags,
	struct cred *cred)
{
	return (0);
}

/* ARGSUSED */
static int
tmp_delmap(
	struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t len,
	u_int prot,
	u_int maxprot,
	u_int flags,
	struct cred *cred)
{
	return (0);
}

static int
tmp_freesp(struct vnode *vp, struct flock64 *lp, int flag)
{
	register int i;
	register struct tmpnode *tp = VTOTN(vp);
	int error;

	ASSERT(vp->v_type == VREG);
	ASSERT(lp->l_start >= 0);

	if (lp->l_len != 0)
		return (EINVAL);

	rw_enter(&tp->tn_rwlock, RW_WRITER);
	if (tp->tn_size == lp->l_start) {
		rw_exit(&tp->tn_rwlock);
		return (0);
	}

	/*
	 * Check for any mandatory locks on the range
	 */
	if (MANDLOCK(vp, tp->tn_mode)) {
		long save_start;

		save_start = lp->l_start;

		if (tp->tn_size < lp->l_start) {
			/*
			 * "Truncate up" case: need to make sure there
			 * is no lock beyond current end-of-file. To
			 * do so, we need to set l_start to the size
			 * of the file temporarily.
			 */
			lp->l_start = tp->tn_size;
		}
		lp->l_type = F_WRLCK;
		lp->l_sysid = 0;
		lp->l_pid = ttoproc(curthread)->p_pid;
		i = (flag & (FNDELAY|FNONBLOCK)) ? 0 : SLPFLCK;
		if ((i = reclock(vp, lp, i, 0, lp->l_start)) != 0 ||
		    lp->l_type != F_UNLCK) {
			rw_exit(&tp->tn_rwlock);
			return (i ? i : EAGAIN);
		}

		lp->l_start = save_start;
	}
	VFSTOTM(vp->v_vfsp);

	rw_enter(&tp->tn_contents, RW_WRITER);
	error = tmpnode_trunc((struct tmount *)VFSTOTM(vp->v_vfsp),
	    tp, (u_long)lp->l_start);
	rw_exit(&tp->tn_contents);
	rw_exit(&tp->tn_rwlock);
	return (error);
}

/* ARGSUSED */
static int
tmp_space(
	struct vnode *vp,
	int cmd,
	struct flock64 *bfp,
	int flag,
	offset_t offset,
	struct cred *cred)
{
	int error;

	if (cmd != F_FREESP)
		return (EINVAL);
	if ((error = convoff(vp, bfp, 0, (offset_t)offset)) == 0) {
		if ((bfp->l_start > MAXOFF_T) || (bfp->l_len > MAXOFF_T))
			return (EFBIG);
		error = tmp_freesp(vp, bfp, flag);
	}
	return (error);
}

/* ARGSUSED */
static int
tmp_seek(struct vnode *vp, offset_t ooff, offset_t *noffp)
{
	return ((*noffp < 0 || *noffp > MAXOFFSET_T) ? EINVAL : 0);
}

static void
tmp_rwlock(struct vnode *vp, int write_lock)
{
	struct tmpnode *tp = VTOTN(vp);

	if (write_lock) {
		rw_enter(&tp->tn_rwlock, RW_WRITER);
	} else {
		rw_enter(&tp->tn_rwlock, RW_READER);
	}
}

/* ARGSUSED1 */
static void
tmp_rwunlock(struct vnode *vp, int write_lock)
{
	struct tmpnode *tp = VTOTN(vp);

	rw_exit(&tp->tn_rwlock);
}


struct vnodeops tmp_vnodeops = {
	tmp_open,
	tmp_close,
	tmp_read,
	tmp_write,
	tmp_ioctl,
	fs_setfl,
	tmp_getattr,
	tmp_setattr,
	tmp_access,
	tmp_lookup,
	tmp_create,
	tmp_remove,
	tmp_link,
	tmp_rename,
	tmp_mkdir,
	tmp_rmdir,
	tmp_readdir,
	tmp_symlink,
	tmp_readlink,
	tmp_fsync,
	tmp_inactive,
	tmp_fid,
	tmp_rwlock,
	tmp_rwunlock,
	tmp_seek,
	fs_cmp,
	fs_frlock,
	tmp_space,
	fs_nosys,	/* realvp */
	tmp_getpage,
	tmp_putpage,
	tmp_map,
	tmp_addmap,
	tmp_delmap,
	fs_poll,
	fs_nosys,	/* dump */
	fs_pathconf,
	fs_nosys,	/* pageio */
	fs_nosys,	/* dumpctl */
	fs_dispose,
	fs_nosys,	/* setsecattr */
	fs_fab_acl,	/* getsecattr */
	fs_shrlock	/* shrlock */
};

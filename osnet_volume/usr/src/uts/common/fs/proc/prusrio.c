/*
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)prusrio.c	1.35	99/05/07 SMI"	/* from SVr4.0 1.12 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/inline.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/sysmacros.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/cpuvar.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>

#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <vm/seg_spt.h>

#include <fs/proc/prdata.h>
#if defined(sparc) || defined(__sparc)
#include <sys/stack.h>
#endif

/*
 * Perform I/O to/from an address space
 */

static int
pr_page_exists(struct seg *seg, caddr_t addr)
{
	struct segvn_data *svd;
	vnode_t *vp;
	vattr_t vattr;
	extern struct seg_ops segdev_ops;	/* needs a header file */
	extern struct seg_ops segspt_shmops;	/* needs a header file */

	/*
	 * Fail if the page doesn't map to a page in the underlying
	 * mapped file, if an underlying mapped file exists.
	 */
	vattr.va_mask = AT_SIZE;
	if (seg->s_ops == &segvn_ops &&
	    SEGOP_GETVP(seg, addr, &vp) == 0 &&
	    vp != NULL && vp->v_type == VREG &&
	    VOP_GETATTR(vp, &vattr, 0, CRED()) == 0) {
		u_offset_t size = vattr.va_size;
		u_offset_t offset = SEGOP_GETOFFSET(seg, addr);
		if (size < offset)
			size = 0;
		else
			size -= offset;
		size = roundup(size, (u_offset_t)PAGESIZE);
		if (size > (u_offset_t)seg->s_size)
			size = (u_offset_t)seg->s_size;
		if (addr >= seg->s_base + size)
			return (0);
	}

	/*
	 * Fail if this is an ISM shared segment and the address is
	 * not within the real size of the spt segment that backs it.
	 */
	if (seg->s_ops == &segspt_shmops &&
	    addr >= seg->s_base + spt_realsize(seg))
		return (0);

	/*
	 * Fail if the segment is mapped from /dev/null.
	 * The key is that the mapping comes from segdev and the
	 * type is neither MAP_SHARED nor MAP_PRIVATE.
	 */
	if (seg->s_ops == &segdev_ops &&
	    SEGOP_GETTYPE(seg, addr) == 0)
		return (0);

	/*
	 * Fail if the page is a MAP_NORESERVE page that has
	 * not actually materialized.
	 * We cheat by knowing that segvn is the only segment
	 * driver that supports MAP_NORESERVE.
	 */
	if (seg->s_ops == &segvn_ops &&
	    (svd = (struct segvn_data *)seg->s_data) != NULL &&
	    (svd->vp == NULL || svd->vp->v_type != VREG) &&
	    (svd->flags & MAP_NORESERVE)) {
		/*
		 * Guilty knowledge here.  We know that
		 * segvn_incore returns more than just the
		 * low-order bit that indicates the page is
		 * actually in memory.  If any bits are set,
		 * then there is backing store for the page.
		 */
		char incore = 0;
		(void) SEGOP_INCORE(seg, addr, PAGESIZE, &incore);
		if (incore == 0)
			return (0);
	}
	return (1);
}

static int
pr_uread(struct as *as, struct uio *uiop, void *bp, int old)
{
	caddr_t addr;
	caddr_t page;
	caddr_t vaddr;
	struct seg *seg;
	int error = 0;
	int err = 0;
	u_int prot;
	int protchanged;
	size_t len;
	ssize_t total = uiop->uio_resid;

	/*
	 * Fault in the necessary pages one at a time, map them into
	 * kernel space, copy to our local buffer, and copy out.
	 */
	while (uiop->uio_resid != 0) {
		/*
		 * Locate segment containing address of interest.
		 */
		addr = (caddr_t)uiop->uio_offset;
		page = (caddr_t)(((uintptr_t)addr) & PAGEMASK);

		AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);

		if ((seg = as_segat(as, page)) == NULL ||
		    !pr_page_exists(seg, page)) {
			AS_LOCK_EXIT(as, &as->a_lock);
			err = error = EIO;
			break;
		}
		SEGOP_GETPROT(seg, page, 0, &prot);

		protchanged = 0;
		if ((prot & PROT_READ) == 0) {
			protchanged = 1;
			if (SEGOP_SETPROT(seg, page, PAGESIZE,
			    prot|PROT_READ)) {
				AS_LOCK_EXIT(as, &as->a_lock);
				err = error = EIO;
				break;
			}
		}

		if (SEGOP_FAULT(as->a_hat, seg, page, PAGESIZE,
		    F_SOFTLOCK, S_READ)) {
			if (protchanged)
				(void) SEGOP_SETPROT(seg, page, PAGESIZE, prot);
			AS_LOCK_EXIT(as, &as->a_lock);
			err = error = EIO;
			break;
		}
		CPU_STAT_ADD_K(cpu_vminfo.softlock, 1);

		/*
		 * Map in the locked page, copy to our local buffer,
		 * then map the page out and unlock it.
		 */
		vaddr = prmapin(as, addr, 0);
		len = MIN(uiop->uio_resid, (page + PAGESIZE) - addr);
		bcopy(vaddr, bp, len);
		prmapout(as, addr, vaddr, 0);
		(void) SEGOP_FAULT(as->a_hat, seg, page, PAGESIZE,
		    F_SOFTUNLOCK, S_READ);

		if (protchanged)
			(void) SEGOP_SETPROT(seg, page, PAGESIZE, prot);

		/*
		 * Drop the address space lock before the uiomove().
		 * We don't want to hold a lock while taking a page fault.
		 */
		AS_LOCK_EXIT(as, &as->a_lock);
		if ((error = uiomove(bp, len, UIO_READ, uiop)) != 0)
			break;
	}

	/*
	 * If the I/O was truncated because a page didn't exist,
	 * don't return an error.  Also don't return an error
	 * for a read that started at an invalid address unless
	 * we are conforming to old /proc semantics.
	 */
	if (err && (total != uiop->uio_resid || !old))
		error = 0;

	return (error);
}

static int
pr_uwrite(struct as *as, struct uio *uiop, void *bp)
{
	caddr_t addr;
	caddr_t page;
	caddr_t vaddr;
	struct seg *seg;
	int error = 0;
	int err = 0;
	u_int prot;
	int protchanged;
	size_t len;
	ssize_t total = uiop->uio_resid;

	/*
	 * Fault in the necessary pages one at a time, map them into
	 * kernel space, and copy from our local buffer.
	 */
	while (uiop->uio_resid != 0) {
		/*
		 * Locate segment containing address of interest.
		 */
		addr = (caddr_t)uiop->uio_offset;
		page = (caddr_t)(((uintptr_t)addr) & PAGEMASK);

		/*
		 * Don't grab the address space until we do the uiomove().
		 * We don't want to hold a lock while taking a page fault.
		 */
		len = MIN(uiop->uio_resid, (page + PAGESIZE) - addr);
		if ((error = uiomove(bp, len, UIO_WRITE, uiop)) != 0)
			break;

		AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);

		if ((seg = as_segat(as, page)) == NULL ||
		    !pr_page_exists(seg, page)) {
			AS_LOCK_EXIT(as, &as->a_lock);
			err = error = EIO;
			uiop->uio_resid += len;
			uiop->uio_loffset -= len;
			break;
		}
		SEGOP_GETPROT(seg, page, 0, &prot);

		protchanged = 0;
		if ((prot & PROT_WRITE) == 0) {
			protchanged = 1;
			if (SEGOP_SETPROT(seg, page, PAGESIZE,
			    prot|PROT_WRITE)) {
				AS_LOCK_EXIT(as, &as->a_lock);
				err = error = EIO;
				uiop->uio_resid += len;
				uiop->uio_loffset -= len;
				break;
			}
		}

		if (SEGOP_FAULT(as->a_hat, seg, page, PAGESIZE,
		    F_SOFTLOCK, S_WRITE)) {
			if (protchanged)
				(void) SEGOP_SETPROT(seg, page, PAGESIZE, prot);
			AS_LOCK_EXIT(as, &as->a_lock);
			err = error = EIO;
			uiop->uio_resid += len;
			uiop->uio_loffset -= len;
			break;
		}
		CPU_STAT_ADD_K(cpu_vminfo.softlock, 1);

		/*
		 * Map in the locked page, copy from our local buffer,
		 * then map the page out and unlock it.
		 */
		vaddr = prmapin(as, addr, 1);
		bcopy(bp, vaddr, len);
		if (prot & PROT_EXEC)	/* Keeps i-$ consistent */
			sync_icache(vaddr, (u_int)len);
		prmapout(as, addr, vaddr, 1);
		(void) SEGOP_FAULT(as->a_hat, seg, page, PAGESIZE,
		    F_SOFTUNLOCK, S_WRITE);

		if (protchanged)
			(void) SEGOP_SETPROT(seg, page, PAGESIZE, prot);

		AS_LOCK_EXIT(as, &as->a_lock);
	}

	/*
	 * If the I/O was truncated because a page didn't exist,
	 * don't return an error.
	 */
	if (err && (total != uiop->uio_resid))
		error = 0;

	return (error);
}

#define	STACK_BUF_SIZE	64	/* power of 2 */

int
prusrio(proc_t *p, enum uio_rw rw, struct uio *uiop, int old)
{
	/* longlong-aligned short buffer */
	longlong_t buffer[STACK_BUF_SIZE / sizeof (longlong_t)];
	int error;
	void *bp;
	int allocated;

	/* for short reads/writes, use the on-stack buffer */
	if (uiop->uio_resid <= STACK_BUF_SIZE) {
		bp = buffer;
		allocated = 0;
	} else {
		bp = kmem_alloc(PAGESIZE, KM_SLEEP);
		allocated = 1;
	}

#if defined(sparc) || defined(__sparc)
	if (p == curproc)
		(void) flush_user_windows_to_stack(NULL);
#endif

	switch (rw) {
	case UIO_READ:
		error = pr_uread(p->p_as, uiop, bp, old);
		break;
	case UIO_WRITE:
		error = pr_uwrite(p->p_as, uiop, bp);
		break;
	default:
		cmn_err(CE_PANIC,
		    "prusrio: rw=%d neither UIO_READ not UIO_WRITE", rw);
		/* NOTREACHED */
		error = ENOTSUP;
		break;
	}

	if (allocated)
		kmem_free(bp, PAGESIZE);

	return (error);
}

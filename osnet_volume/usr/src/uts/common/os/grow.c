/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1989-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)grow.c	1.65	99/08/31 SMI"	/* from SVr4.0 1.35 */

#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/var.h>
#include <sys/proc.h>
#include <sys/tuneable.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/vm.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/vmparam.h>
#include <sys/fcntl.h>
#include <sys/lwpchan_impl.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_dev.h>
#include <vm/seg_vn.h>

int
brk(caddr_t nva)
{
	caddr_t ova;			/* current break address */
	size_t size;
	int	error;
	struct proc *p = curproc;
	struct as *as = p->p_as;

	/*
	 * Serialize brk operations on an address space.
	 */
	as_rangelock(as);

	size = nva - p->p_brkbase;
	nva = (caddr_t)roundup((uintptr_t)nva, PAGESIZE);
	ova = (caddr_t)roundup((uintptr_t)(p->p_brkbase + p->p_brksize),
		PAGESIZE);

	if ((nva < p->p_brkbase) || (size > p->p_brksize &&
	    size > (size_t)P_CURLIMIT(p, RLIMIT_DATA))) {
		as_rangeunlock(as);
		return (set_errno(ENOMEM));
	}

	if (nva > ova) {
		/*
		 * Add new zfod mapping to extend UNIX data segment
		 */
		error = as_map(as, ova, (size_t)(nva - ova),
				segvn_create, zfod_argsp);
		if (error) {
			as_rangeunlock(as);
			return (set_errno(error));
		}

	} else if (nva < ova) {
		/*
		 * Release mapping to shrink UNIX data segment.
		 */
		(void) as_unmap(as, nva, (size_t)(ova - nva));
	}

	p->p_brksize = size;
	as_rangeunlock(as);
	return (0);
}

/*
 * Grow the stack to include sp.  Return 1 if successful, 0 otherwise.
 * This routine assumes that the stack grows downward.
 */
int
grow(caddr_t sp)
{
	struct proc *p = curproc;
	struct as *as = p->p_as;
	size_t newsize = p->p_usrstack - sp;
	size_t oldsize;
	int error;

	ASSERT(sp < p->p_usrstack);

	if (newsize > (size_t)P_CURLIMIT(p, RLIMIT_STACK))
		return (0);

	/*
	 * Serialize brk operations on an address space.
	 * This also serves as the lock protecting p_stksize.
	 */
	as_rangelock(as);

	oldsize = p->p_stksize;
	newsize = P2ROUNDUP(newsize, PAGESIZE);
	ASSERT(P2PHASE(oldsize, PAGESIZE) == 0);

	if (newsize <= oldsize) {	/* prevent the stack from shrinking */
		as_rangeunlock(as);
		return (0);
	}
	if ((error = as_map(as, p->p_usrstack - newsize, newsize - oldsize,
	    segvn_create, (p->p_stkprot & PROT_EXEC) ?
	    stack_exec_argsp : stack_noexec_argsp)) != 0) {
		as_rangeunlock(as);
		if (error == EAGAIN)
			cmn_err(CE_WARN, "Sorry, no swap space to grow stack "
			    "for pid %d (%s)", p->p_pid, u.u_comm);
		return (0);
	}
	p->p_stksize = newsize;

	as_rangeunlock(as);

	/*
	 * Set up translations so the process doesn't have to fault in
	 * the stack pages we just gave it.
	 */
	(void) as_fault(as->a_hat, as,
	    p->p_usrstack - newsize, newsize - oldsize, F_INVAL, S_WRITE);

	return (1);
}

/*
 * Used for MAP_ANON - fast way to get anonymous pages
 */
static int
zmap(struct as *as, caddr_t *addrp, size_t len, uint_t uprot, int flags,
    offset_t pos)
{
	struct segvn_crargs vn_a;

	if (((PROT_ALL & uprot) != uprot))
		return (EACCES);

	if ((flags & MAP_FIXED) != 0) {
		caddr_t userlimit;

		/*
		 * Use the user address.  First verify that
		 * the address to be used is page aligned.
		 * Then make some simple bounds checks.
		 */
		if (((uintptr_t)*addrp & PAGEOFFSET) != 0)
			return (EINVAL);

		userlimit = flags & _MAP_LOW32 ?
		    (caddr_t)USERLIMIT32 : as->a_userlimit;
		switch (valid_usr_range(*addrp, len, uprot, as, userlimit)) {
		case RANGE_OKAY:
			break;
		case RANGE_BADPROT:
			return (ENOTSUP);
		case RANGE_BADADDR:
		default:
			return (ENOMEM);
		}
		(void) as_unmap(as, *addrp, len);
	} else {
		/*
		 * No need to worry about vac alignment for anonymous
		 * pages since this is a "clone" object that doesn't
		 * yet exist.
		 */
		map_addr(addrp, len, pos, 0, flags);
		if (*addrp == NULL)
			return (ENOMEM);
	}

	/*
	 * Use the seg_vn segment driver; passing in the NULL amp
	 * gives the desired "cloning" effect.
	 */
	vn_a.vp = NULL;
	vn_a.offset = 0;
	vn_a.type = flags & MAP_TYPE;
	vn_a.prot = uprot;
	vn_a.maxprot = PROT_ALL;
	vn_a.flags = flags & ~MAP_TYPE;
	vn_a.cred = CRED();
	vn_a.amp = NULL;

	return (as_map(as, *addrp, len, segvn_create, &vn_a));
}

static int
smmap_common(caddr_t *addrp, size_t len,
    int prot, int flags, struct file *fp, offset_t pos)
{
	struct vnode *vp;
	struct as *as = ttoproc(curthread)->p_as;
	uint_t uprot, maxprot, type;
	int error;

	if ((flags & ~(MAP_SHARED | MAP_PRIVATE | MAP_FIXED | _MAP_NEW |
	    _MAP_LOW32 | MAP_NORESERVE | MAP_ANON)) != 0) {
		/* | MAP_RENAME */	/* not implemented, let user know */
		return (EINVAL);
	}

#ifdef sparc
	/*
	 * See if this is an "old mmap call".  If so, remember this
	 * fact and convert the flags value given to mmap to indicate
	 * the specified address in the system call must be used.
	 * _MAP_NEW is turned set by all new uses of mmap.
	 */
	if ((flags & _MAP_NEW) == 0)
		flags |= MAP_FIXED;
#endif
	flags &= ~_MAP_NEW;

	type = flags & MAP_TYPE;
	if (type != MAP_PRIVATE && type != MAP_SHARED)
		return (EINVAL);

	/*
	 * Check for bad lengths and file position.
	 * We let the VOP_MAP routine check for negative lengths
	 * since on some vnode types this might be appropriate.
	 */
	if (len == 0 || (pos & (u_offset_t)PAGEOFFSET) != 0)
		return (EINVAL);

#ifdef _ILP32
	/*
	 * We do have segment sizes > 2GB supported now, but
	 * the changes done to VM are not complete by this
	 * support. So limit the length to be < 2GB here.
	 */
	if (len > MAXOFF_T)
		return (ENOMEM);
#endif
	maxprot = PROT_ALL;		/* start out allowing all accesses */
	uprot = prot | PROT_USER;

	if (fp == NULL) {
		ASSERT(flags & MAP_ANON);
		as_rangelock(as);
		error = zmap(as, addrp, len, uprot, flags, pos);
		as_rangeunlock(as);
		return (error);
	} else if ((flags & MAP_ANON) != 0)
		return (EINVAL);

	vp = fp->f_vnode;

	/*
	 * These checks were added as part of large files.
	 */
	if (vp->v_type == VREG) {
		if (pos < (offset_t)0)
			return (EINVAL);
		if (len > (OFFSET_MAX(fp) - pos))
			return (EOVERFLOW);
	}

	if (type == MAP_SHARED && (fp->f_flag & FWRITE) == 0) {
		/* no write access allowed */
		maxprot &= ~PROT_WRITE;
	}

	/*
	 * XXX - Do we also adjust maxprot based on protections
	 * of the vnode?  E.g. if no execute permission is given
	 * on the vnode for the current user, maxprot probably
	 * should disallow PROT_EXEC also?  This is different
	 * from the write access as this would be a per vnode
	 * test as opposed to a per fd test for writability.
	 */

	/*
	 * Verify that the specified protections are not greater than
	 * the maximum allowable protections.  Also test to make sure
	 * that the file descriptor does allows for read access since
	 * "write only" mappings are hard to do since normally we do
	 * the read from the file before the page can be written.
	 */
	if (((maxprot & uprot) != uprot) || (fp->f_flag & FREAD) == 0)
		return (EACCES);

	/*
	 * If the user specified an address, do some simple checks here
	 */
	if ((flags & MAP_FIXED) != 0) {
		caddr_t userlimit;

		/*
		 * Use the user address.  First verify that
		 * the address to be used is page aligned.
		 * Then make some simple bounds checks.
		 */
		if (((uintptr_t)*addrp & PAGEOFFSET) != 0)
			return (EINVAL);

		userlimit = flags & _MAP_LOW32 ?
		    (caddr_t)USERLIMIT32 : as->a_userlimit;
		switch (valid_usr_range(*addrp, len, uprot, as, userlimit)) {
		case RANGE_OKAY:
			break;
		case RANGE_BADPROT:
			return (ENOTSUP);
		case RANGE_BADADDR:
		default:
			return (ENOMEM);
		}
	}

	/*
	 * Ok, now let the vnode map routine do its thing to set things up.
	 */
	error = VOP_MAP(vp, pos, as,
	    addrp, len, uprot, maxprot, flags, fp->f_cred);

	if (error == 0 && vp->v_type == VREG && (prot & PROT_EXEC) != 0) {
		/*
		 * Mark this as an executable vnode
		 */
		mutex_enter(&vp->v_lock);
		vp->v_flag |= VVMEXEC;
		mutex_exit(&vp->v_lock);
	}

	return (error);
}

#ifdef _LP64
/*
 * LP64 mmap(2) system call: 64-bit offset, 64-bit address.
 *
 * The "large file" mmap routine mmap64(2) is also mapped to this routine
 * by the 64-bit version of libc.
 *
 * Eventually, this should be the only version, and have smmap_common()
 * folded back into it again.  Some day.
 */
caddr_t
smmap64(caddr_t addr, size_t len, int prot, int flags, int fd, off_t pos)
{
	struct file *fp;
	int error;

	if (flags & _MAP_LOW32)
		error = EINVAL;
	else if (fd == -1 && (flags & MAP_ANON) != 0)
		error = smmap_common(&addr, len, prot, flags,
		    NULL, (offset_t)pos);
	else if ((fp = getf(fd)) != NULL) {
		error = smmap_common(&addr, len, prot, flags,
		    fp, (offset_t)pos);
		releasef(fd);
	} else
		error = EBADF;

	return (error ? (caddr_t)set_errno(error) : addr);
}
#endif	/* _LP64 */

#if defined(_SYSCALL32_IMPL) || defined(_ILP32)

/*
 * ILP32 mmap(2) system call: 32-bit offset, 32-bit address.
 */
caddr_t
smmap32(caddr32_t addr, size32_t len, int prot, int flags, int fd, off32_t pos)
{
	struct file *fp;
	int error;
	caddr_t a = (caddr_t)addr;

	if (flags & _MAP_LOW32)
		error = EINVAL;
	else if (fd == -1 && (flags & MAP_ANON) != 0)
		error = smmap_common(&a, (size_t)len, prot,
		    flags | _MAP_LOW32, NULL, (offset_t)pos);
	else if ((fp = getf(fd)) != NULL) {
		error = smmap_common(&a, (size_t)len, prot,
		    flags | _MAP_LOW32, fp, (offset_t)pos);
		releasef(fd);
	} else
		error = EBADF;

	ASSERT(error != 0 || (uintptr_t)(a + len) < (uintptr_t)UINT32_MAX);

	return (error ? (caddr_t)set_errno(error) : a);
}

/*
 * ILP32 mmap64(2) system call: 64-bit offset, 32-bit address.
 *
 * Now things really get ugly because we can't use the C-style
 * calling convention for more than 6 args, and 64-bit parameter
 * passing on 32-bit systems is less than clean.
 */

struct mmaplf32a {
	caddr_t addr;
	size_t len;
#ifdef _LP64
	/*
	 * 32-bit contents, 64-bit cells
	 */
	uint64_t prot;
	uint64_t flags;
	uint64_t fd;
	uint64_t offhi;
	uint64_t offlo;
#else
	/*
	 * 32-bit contents, 32-bit cells
	 */
	uint32_t prot;
	uint32_t flags;
	uint32_t fd;
	uint32_t offhi;
	uint32_t offlo;
#endif
};

int
smmaplf32(struct mmaplf32a *uap, rval_t *rvp)
{
	struct file *fp;
	int error;
	caddr_t a = uap->addr;
	int flags = (int)uap->flags;
	int fd = (int)uap->fd;
#ifdef _BIG_ENDIAN
	offset_t off = ((u_offset_t)uap->offhi << 32) | (u_offset_t)uap->offlo;
#else
	offset_t off = ((u_offset_t)uap->offlo << 32) | (u_offset_t)uap->offhi;
#endif

	if (flags & _MAP_LOW32)
		error = EINVAL;
	else if (fd == -1 && (flags & MAP_ANON) != 0)
		error = smmap_common(&a, uap->len, (int)uap->prot,
		    flags | _MAP_LOW32, NULL, off);
	else if ((fp = getf(fd)) != NULL) {
		error = smmap_common(&a, uap->len, (int)uap->prot,
		    flags | _MAP_LOW32, fp, off);
		releasef(fd);
	} else
		error = EBADF;

	if (error == 0)
		rvp->r_val1 = (uintptr_t)a;
	return (error);
}

#endif	/* _SYSCALL32_IMPL || _ILP32 */

int
munmap(caddr_t addr, size_t len)
{
	struct as *as = ttoproc(curthread)->p_as;
	struct proc *p = ttoproc(curthread);

	if (((uintptr_t)addr & PAGEOFFSET) != 0 || len == 0)
		return (set_errno(EINVAL));

	if (valid_usr_range(addr, len, 0, as, as->a_userlimit) != RANGE_OKAY)
		return (set_errno(EINVAL));

	/*
	 * discard lwpchan mappings.
	 */
	mutex_enter(&p->p_lcp_mutexinitlock);
	if (p->p_lcp)
		lwpchan_delete_mapping(p->p_lcp, addr, addr + len);

	if (as_unmap(as, addr, len) != 0) {
		mutex_exit(&p->p_lcp_mutexinitlock);
		return (set_errno(EINVAL));
	}
	mutex_exit(&p->p_lcp_mutexinitlock);

	return (0);
}

int
mprotect(caddr_t addr, size_t len, int prot)
{
	struct as *as = ttoproc(curthread)->p_as;
	uint_t uprot = prot | PROT_USER;
	int error;

	if (((uintptr_t)addr & PAGEOFFSET) != 0 || len == 0)
		return (set_errno(EINVAL));

	switch (valid_usr_range(addr, len, prot, as, as->a_userlimit)) {
	case RANGE_OKAY:
		break;
	case RANGE_BADPROT:
		return (set_errno(ENOTSUP));
	case RANGE_BADADDR:
	default:
		return (set_errno(ENOMEM));
	}

	error = as_setprot(as, addr, len, uprot);
	if (error)
		return (set_errno(error));
	return (0);
}

#define	MC_CACHE	128			/* internal result buffer */
#define	MC_QUANTUM	(MC_CACHE * PAGESIZE)	/* addresses covered in loop */

int
mincore(caddr_t addr, size_t len, char *vecp)
{
	struct as *as = ttoproc(curthread)->p_as;
	caddr_t ea;			/* end address of loop */
	size_t rl;			/* inner result length */
	char vec[MC_CACHE];		/* local vector cache */
	int error;
	model_t model;
	long	llen;

	model = get_udatamodel();
	/*
	 * Validate form of address parameters.
	 */
	if (model == DATAMODEL_NATIVE) {
		llen = (long)len;
	} else {
		llen = (int32_t)(size32_t)len;
	}
	if (((uintptr_t)addr & PAGEOFFSET) != 0 || llen <= 0)
		return (set_errno(EINVAL));

	if (valid_usr_range(addr, len, 0, as, as->a_userlimit) != RANGE_OKAY)
		return (set_errno(ENOMEM));

	/*
	 * Loop over subranges of interval [addr : addr + len), recovering
	 * results internally and then copying them out to caller.  Subrange
	 * is based on the size of MC_CACHE, defined above.
	 */
	for (ea = addr + len; addr < ea; addr += MC_QUANTUM) {
		error = as_incore(as, addr,
		    (size_t)MIN(MC_QUANTUM, ea - addr), vec, &rl);
		if (rl != 0) {
			rl = (rl + PAGESIZE - 1) / PAGESIZE;
			if (copyout(vec, vecp, rl) != 0)
				return (set_errno(EFAULT));
			vecp += rl;
		}
		if (error != 0)
			return (set_errno(ENOMEM));
	}
	return (0);
}

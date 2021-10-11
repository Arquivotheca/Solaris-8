/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)seg_kmem.c	1.12	99/12/04 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/tuneable.h>
#include <sys/systm.h>
#include <sys/vm.h>
#include <sys/kmem.h>
#include <sys/vmem.h>
#include <sys/mman.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/dumphdr.h>
#include <sys/bootconf.h>
#include <vm/seg_kmem.h>
#include <vm/hat.h>
#include <vm/page.h>
#include <vm/faultcode.h>

char *kernelheap;		/* start of primary kernel heap */
char *ekernelheap;		/* end of primary kernel heap */
struct seg kvseg;		/* primary kernel heap segment */
vmem_t *heap_arena;		/* primary kernel heap vmem descriptor */
struct seg kvseg32;		/* 32-bit kernel heap segment */
vmem_t *heap32_arena;		/* 32-bit kernel heap vmem descriptor */
struct as kas;			/* kernel address space */
struct vnode kvp;		/* vnode for all segkmem pages */

/*
 * Freed pages accumulate on a garbage list until segkmem is ready,
 * at which point we call segkmem_gc() to free it all.
 */
typedef struct segkmem_gc_list {
	struct segkmem_gc_list	*gc_next;
	vmem_t			*gc_arena;
	size_t			gc_size;
} segkmem_gc_list_t;

static segkmem_gc_list_t *segkmem_gc_list;

/*
 * Initialize kernel heap boundaries.
 */
void
kernelheap_init(void *heap_start, void *heap_end, void *first_avail)
{
	kernelheap = heap_start;
	ekernelheap = heap_end;

	/*
	 * The ordering is a bit strange here: vmem_seg_arena cannot be
	 * created until the heap exists, but we can't create the initial
	 * heap span until vmem_seg_arena exists.  Therefore we create
	 * heap_arena empty, then vmem_init(), then add the heap span.
	 */
	heap_arena = vmem_create("heap", NULL, 0, PAGESIZE,
	    NULL, NULL, NULL, 0, VM_SLEEP);

	vmem_init();

	(void) vmem_add(heap_arena, kernelheap, ekernelheap - kernelheap,
	    VM_SLEEP);
	(void) vmem_xalloc(heap_arena, (caddr_t)first_avail - kernelheap,
	    PAGESIZE, 0, 0, kernelheap, first_avail,
	    VM_NOSLEEP | VM_BESTFIT | VM_PANIC);
#ifdef __sparcv9
	heap32_arena = vmem_create("heap32", (void *)SYSBASE32,
	    SYSLIMIT32 - SYSBASE32, PAGESIZE, NULL, NULL, NULL, 0, VM_SLEEP);
#else
	heap32_arena = heap_arena;
#endif
}

#if defined(__ia64)
/*
 * We allocate from 1 Meg chunks of physical memory, BOP_ALLOC()d as needed.
 */
struct bootspace {
	size_t		bs_size;	/* size of this chunk */
	struct bootspace *bs_next;	/* next chunk, if any */
};
static struct bootspace *bootspace = NULL;
#endif	/* __ia64 */

static void
boot_mapin(caddr_t addr, size_t size)
{
	caddr_t eaddr;

	if (page_resv(btop(size), KM_NOSLEEP) == 0)
		panic("boot_alloc() failed, page_resv()");
	for (eaddr = addr + size; addr < eaddr; addr += PAGESIZE) {
		page_t *pp = page_numtopp(va_to_pfn(addr), SE_EXCL);
		if (pp == NULL || PP_ISFREE(pp))
			panic("boot_alloc: pp is NULL or free");
		(void) page_hashin(pp, &kvp, (u_offset_t)(uintptr_t)addr, NULL);
		page_downgrade(pp);
	}
}

/*
 * Get pages from boot and hash them into the kernel's vp.
 * Used after page structs have been allocated, but before segkmem is ready.
 */
void *
boot_alloc(void *inaddr, size_t size, uint_t align)
{
	caddr_t addr = inaddr;
#if defined(__ia64)
	struct bootspace *bs, *nbs, **pbs;

	ASSERT(size != 0);
	ASSERT(align <= PAGESIZE);
	size = ptob(btopr(size));
	for (pbs = &bootspace; /* empty */; pbs = &bs->bs_next) {
		if ((bs = *pbs) == NULL) {	/* need more */
			size_t newsize = 1024 * 1024;
			if (newsize < size)
				newsize = size;
			bs = (struct bootspace *)BOP_ALLOC(bootops,
				(caddr_t)PHYS_TO_R6(0), newsize, BO_NO_ALIGN);
			if (bs == NULL)
				panic("BOP_ALLOC failed");
			boot_mapin((caddr_t)bs, newsize);
			bs->bs_size = newsize;
			bs->bs_next = NULL;
			*pbs = bs;
		}
		if (size <= bs->bs_size) {	/* first fit */
			addr = (caddr_t)bs;
			if (size != bs->bs_size) {
				nbs = (struct bootspace *)(addr + size);
				nbs->bs_size = bs->bs_size - size;
				nbs->bs_next = bs->bs_next;
			} else {
				nbs = bs->bs_next;
			}
			*pbs = nbs;
			break;
		}
	}
#else	/* __ia64 */
	size = ptob(btopr(size));
	if (BOP_ALLOC(bootops, addr, size, align) != addr)
		panic("BOP_ALLOC failed");
	boot_mapin((caddr_t)addr, size);
#endif	/* __ia64 */
	return (addr);
}

/*ARGSUSED*/
static faultcode_t
segkmem_fault(struct hat *hat, struct seg *seg, caddr_t addr, size_t size,
	enum fault_type type, enum seg_rw rw)
{
	ASSERT(RW_READ_HELD(&seg->s_as->a_lock));

	if (seg->s_as != &kas || size > seg->s_size ||
	    addr < seg->s_base || addr + size > seg->s_base + seg->s_size)
		panic("segkmem_fault: bad args");

	switch (type) {
	case F_SOFTLOCK:	/* lock down already-loaded translations */
		if (rw == S_OTHER) {
			hat_reserve(seg->s_as, addr, size);
			return (0);
		}
		/*FALLTHROUGH*/
	case F_SOFTUNLOCK:
		if (rw == S_READ || rw == S_WRITE)
			return (0);
	}
	return (FC_NOSUPPORT);
}

static int
segkmem_setprot(struct seg *seg, caddr_t addr, size_t size, uint_t prot)
{
	ASSERT(RW_LOCK_HELD(&seg->s_as->a_lock));

	if (seg->s_as != &kas || size > seg->s_size ||
	    addr < seg->s_base || addr + size > seg->s_base + seg->s_size)
		panic("segkmem_setprot: bad args");

	if (prot == 0)
		hat_unload(kas.a_hat, addr, size, HAT_UNLOAD);
	else
		hat_chgprot(kas.a_hat, addr, size, prot);
	return (0);
}

static void
segkmem_hole(void *arg, void *start, size_t size)
{
	caddr_t *hole = arg;

	if (size > hole[1] - hole[0]) {
		hole[0] = (caddr_t)start;
		hole[1] = (caddr_t)start + size;
	}
	dump_timeleft = dump_timeout;
}

static void
segkmem_dump(struct seg *seg)
{
	caddr_t addr;
	caddr_t hole[2];
	caddr_t sstart, send;

	sstart = seg->s_base;
	send = sstart + seg->s_size;
	hole[0] = sstart;
	hole[1] = sstart;

	/*
	 * The kernel heap arena is a very large VA space, most of which
	 * is typically unused.  To speed up dumping, we find the largest
	 * hole (i.e. free segment) and bypass it; we know there can't be
	 * any mappings at unused virtual addresses.  (Better still would
	 * be to walk kvseg using vmem_walk() on the allocated segments,
	 * but we can't do that: the arena lock is held for the duration
	 * of the walk, and dump_addpage() calls dumpvp_write(), which
	 * may ultimately need to allocate virtual memory from the heap.)
	 */
	if (seg == &kvseg)
		vmem_walk(heap_arena, VMEM_FREE, segkmem_hole, hole);

	/*
	 * Walk pages from segment start to hole start
	 */
	for (addr = sstart; addr < hole[0]; addr += PAGESIZE) {
		pfn_t pfn = hat_getkpfnum(addr);
		if (pfn != PFN_INVALID && pfn <= physmax && pf_is_memory(pfn))
			dump_addpage(seg->s_as, addr, pfn);
		dump_timeleft = dump_timeout;
	}

	/*
	 * Walk pages from hole end to segment end
	 */
	for (addr = hole[1]; addr < send; addr += PAGESIZE) {
		pfn_t pfn = hat_getkpfnum(addr);
		if (pfn != PFN_INVALID && pfn <= physmax && pf_is_memory(pfn))
			dump_addpage(seg->s_as, addr, pfn);
		dump_timeleft = dump_timeout;
	}
}

/*ARGSUSED*/
static int
segkmem_pagelock(struct seg *seg, caddr_t addr, size_t size,
	page_t ***ppp, enum lock_type type, enum seg_rw rw)
{
	return (ENOTSUP);
}

static void
segkmem_badop()
{
	panic("segkmem_badop");
}

#define	SEGKMEM_BADOP(t)	(t(*)())segkmem_badop

static struct seg_ops segkmem_ops = {
	SEGKMEM_BADOP(int),		/* dup */
	SEGKMEM_BADOP(int),		/* unmap */
	SEGKMEM_BADOP(void),		/* free */
	segkmem_fault,
	SEGKMEM_BADOP(faultcode_t),	/* faulta */
	segkmem_setprot,
	SEGKMEM_BADOP(int),		/* checkprot */
	SEGKMEM_BADOP(int),		/* kluster */
	SEGKMEM_BADOP(size_t),		/* swapout */
	SEGKMEM_BADOP(int),		/* sync */
	SEGKMEM_BADOP(size_t),		/* incore */
	SEGKMEM_BADOP(int),		/* lockop */
	SEGKMEM_BADOP(int),		/* getprot */
	SEGKMEM_BADOP(u_offset_t),	/* getoffset */
	SEGKMEM_BADOP(int),		/* gettype */
	SEGKMEM_BADOP(int),		/* getvp */
	SEGKMEM_BADOP(int),		/* advise */
	segkmem_dump,
	segkmem_pagelock,
	SEGKMEM_BADOP(int),		/* getmemid */
};

int
segkmem_create(struct seg *seg)
{
	ASSERT(seg->s_as == &kas && RW_WRITE_HELD(&kas.a_lock));
	seg->s_ops = &segkmem_ops;
	seg->s_data = NULL;
	kas.a_size += seg->s_size;
	return (0);
}

/*ARGSUSED*/
page_t *
segkmem_page_create(void *addr, size_t size, int vmflag, void *arg)
{
	struct seg kseg;

	kseg.s_as = &kas;

	return (page_create_va(&kvp, (u_offset_t)(uintptr_t)addr, size,
	    PG_NORELOC | PG_EXCL | ((vmflag & VM_NOSLEEP) ? 0 : PG_WAIT),
	    &kseg, addr));
}

/*
 * Allocate pages to back the virtual address range [addr, addr + size).
 * If addr is NULL, allocate the virtual address space as well.
 */
void *
segkmem_xalloc(vmem_t *vmp, void *inaddr, size_t size, int vmflag, uint_t attr,
	page_t *(*page_create_func)(void *, size_t, int, void *), void *pcarg)
{
	page_t *ppl;
	caddr_t addr = inaddr;
	pgcnt_t npages = btopr(size);

	if (inaddr == NULL && (addr = vmem_alloc(vmp, size, vmflag)) == NULL)
		return (NULL);

	ASSERT(((uintptr_t)addr & PAGEOFFSET) == 0);

	if (page_resv(npages, vmflag & VM_KMFLAGS) == 0) {
		if (inaddr == NULL)
			vmem_free(vmp, addr, size);
		return (NULL);
	}

	ppl = page_create_func(addr, size, vmflag & VM_NOSLEEP, pcarg);
	if (ppl == NULL) {
		if (inaddr == NULL)
			vmem_free(vmp, addr, size);
		page_unresv(npages);
		return (NULL);
	}

	while (ppl != NULL) {
		page_t *pp = ppl;
		page_sub(&ppl, pp);
		ASSERT(page_iolock_assert(pp));
		page_io_unlock(pp);
		page_downgrade(pp);
		hat_memload(kas.a_hat, (caddr_t)pp->p_offset, pp,
		    (PROT_ALL & ~PROT_USER) | HAT_NOSYNC | attr, HAT_LOAD_LOCK);
	}

	return (addr);
}

void *
segkmem_alloc(vmem_t *vmp, size_t size, int vmflag)
{
	if (kvseg.s_base == NULL) {
		void *addr;
#if defined(__ia64)
		addr = boot_alloc(NULL, size, BO_NO_ALIGN);
#else	/* __ia64 */
		addr = vmem_alloc(vmp, size, vmflag | VM_PANIC);
		if (boot_alloc(addr, size, BO_NO_ALIGN) != addr)
			panic("segkmem_alloc: boot_alloc failed");
#endif	/* __ia64 */
		return (addr);
	}
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    segkmem_page_create, NULL));
}

void
segkmem_free(vmem_t *vmp, void *inaddr, size_t size)
{
	page_t *pp;
	caddr_t addr = inaddr;
	caddr_t eaddr;
	pgcnt_t npages = btopr(size);

	ASSERT(((uintptr_t)addr & PAGEOFFSET) == 0);

#if defined(__ia64)
	if (kvseg.s_base == NULL) {
		struct bootspace *bs, **pbs;
		size = ptob(npages);
		ASSERT(size != 0);
		for (pbs = &bootspace; (bs = *pbs) != NULL; pbs = &bs->bs_next)
			;
		bs = (struct bootspace *)inaddr;
		bs->bs_size = size;
		bs->bs_next = NULL;
		*pbs = bs;
		return;
	}
	ASSERT(IS_VRN_ADDR(7, addr));
#else	/* __ia64 */
	if (kvseg.s_base == NULL) {
		segkmem_gc_list_t *gc = inaddr;
		gc->gc_arena = vmp;
		gc->gc_size = size;
		gc->gc_next = segkmem_gc_list;
		segkmem_gc_list = gc;
		return;
	}
#endif	/* __ia64 */

	hat_unload(kas.a_hat, addr, size, HAT_UNLOAD_UNLOCK);

	for (eaddr = addr + size; addr < eaddr; addr += PAGESIZE) {
		/*
		 * Use page_find() instead of page_lookup() to find the page
		 * since we know that it is hashed and has a shared lock.
		 */
		pp = page_find(&kvp, (u_offset_t)(uintptr_t)addr);
		if (pp == NULL)
			panic("segkmem_free: page not found");
		if (!page_tryupgrade(pp)) {
			/*
			 * Some other thread has it locked shared too --
			 * most likely a /dev/kmem reader.  Drop our shared
			 * lock and wait until we can get exclusive access.
			 */
			page_unlock(pp);
			pp = page_lookup(&kvp, (u_offset_t)(uintptr_t)addr,
			    SE_EXCL);
			if (pp == NULL)
				panic("segkmem_free: page already freed");
		}
		page_destroy(pp, 0);
	}
	page_unresv(npages);

	if (vmp != NULL)
		vmem_free(vmp, inaddr, size);
}

void
segkmem_gc(void)
{
	ASSERT(kvseg.s_base != NULL);
	while (segkmem_gc_list != NULL) {
		segkmem_gc_list_t *gc = segkmem_gc_list;
		segkmem_gc_list = gc->gc_next;
		segkmem_free(gc->gc_arena, gc, gc->gc_size);
	}
}

/*
 * Legacy entry points from here to end of file.
 */
void
segkmem_mapin(struct seg *seg, void *addr, size_t size, uint_t vprot,
    pfn_t pfn, uint_t flags)
{
	hat_unload(seg->s_as->a_hat, addr, size, HAT_UNLOAD_UNLOCK);
	hat_devload(seg->s_as->a_hat, addr, size, pfn, vprot,
	    flags | HAT_LOAD_LOCK);
}

void
segkmem_mapout(struct seg *seg, void *addr, size_t size)
{
	hat_unload(seg->s_as->a_hat, addr, size, HAT_UNLOAD_UNLOCK);
}

void *
kmem_getpages(pgcnt_t npages, int kmflag)
{
	return (kmem_alloc(ptob(npages), kmflag));
}

void
kmem_freepages(void *addr, pgcnt_t npages)
{
	kmem_free(addr, ptob(npages));
}

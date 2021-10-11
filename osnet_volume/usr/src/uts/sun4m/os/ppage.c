/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ppage.c	1.35	99/04/14 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/systm.h>
#include <sys/vmem.h>
#include <sys/mman.h>
#include <sys/vm.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/cmn_err.h>
#include <sys/devaddr.h>
#include <vm/as.h>
#include <vm/hat_srmmu.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <sys/kmem.h>
#include <vm/seg_kmem.h>
#include <sys/debug.h>
#include <vm/mach_page.h>

/*
 * External Routines:
 */
extern struct hatops srmmu_hatops;
extern void locked_pgcopy(caddr_t from, caddr_t to, size_t count);
extern void locked_bzero(void *addr, size_t count);

/*
 * External Data:
 */

extern int	do_pg_coloring;
extern int	use_cache;	/* control ppmap usage */
extern int	vac_size;	/* cache size in bytes */
extern u_int	vac_mask;	/* VAC alignment consistency mask */
extern int	mxcc;		/* binary: mxcc (hw block copy) available? */
extern int	vac_shift;	/* log2(vac_size) for ppmapout() */

/*
 * A quick way to generate a cache consistent address
 * to map in a page.
 * users: ppcopy, pagezero, /proc, dev/mem
 *
 * The space available for cache consistant address is 512K.
 * We would like to maintain an array, ppmap_vaddrs, to keep these addresses.
 *
 */
#define	VACSZPG		64	/* Default for 256K VAC */
#define	PPMAPSHIFT	18	/* Default for 256K VAC */
#define	NSETS		2	/* Default for 256K VAC */

int nsets = NSETS;
int ppmapshift = PPMAPSHIFT;
int vacszpg = VACSZPG;

#define	PINDEX(X, Y)	(X + (Y * vacszpg))
static caddr_t	*ppmap_vaddrs;
static kmutex_t ppmap_lock;
static long	ppmap_last = 0;
static long	ppmap_pages = 0;	/* generate align mask */
static int	ppmap_shift = 0;	/* set selector */

static size_t	ppmapsize;	/* size of virtual area reserved */
static caddr_t	ppmap_start;	/* beginning address of virtual memory */
static caddr_t	ppmap_end;	/* ending address of virtual memory */

#ifdef PPDEBUG
static int	ppalloc_noslot = 0;
static int	nset_hits[NSETS+1];
static int	align_hits[VACSZPG+1];
static int	align_miss[VACSZPG+1];
#endif PPDEBUG

/*
 * Allocate and initialize ppmap_vaddrs array based upon installed
 * cache size & type.
 *
 * For physical caches, we initialize ppmap_vaddrs as if we had
 * a 256K cache with two sets, and then just pick any address
 * available.
 *
 * For a 64K VAC, as in Ross 605, we were using 4 sets (why 4 sets).
 * On the HyperSparc, the cache size can be 128K, 256K, 512K, or 1M.
 * This means the normal PPMAPSIZE area is too small for the largest
 * caches.  If a large cache size is detected, we preallocate a
 * larger kernel virtual area.
 */
void
ppmapinit(void)
{
	long align, nset, npgs, i;
	caddr_t va;

	if ((cache & CACHE_VAC) && (PPMAPSIZE < vac_size)) {
		/*
		 * Cache is larger than PPMAPBASE area, we must use
		 * kernel heap virtual memory.
		 */
		ppmapsize = vac_size * NSETS;
		va = vmem_alloc(heap_arena, ppmapsize, VM_SLEEP);
	} else {
		/*
		 * Use normal PPMAPBASE area
		 */
		ppmapsize = PPMAPSIZE;
		va = (caddr_t)PPMAPBASE;
	}

	ppmap_start = va;
	ppmap_end = (caddr_t)(va + ppmapsize - 1);

	if (cache & CACHE_VAC) {
		ppmap_pages = vacszpg = vac_size/MMU_PAGESIZE;
		nsets = ppmapsize/vac_size;
		ppmap_shift = vac_shift;
	} else {
		ppmap_pages = vacszpg;
		nsets = PPMAPSIZE/mmu_ptob(vacszpg);
		ppmap_shift = ppmapshift;
	}

	npgs = ppmap_pages * nsets;
	ASSERT(npgs <= mmu_btop(ppmapsize));
	ppmap_vaddrs = kmem_alloc(npgs * sizeof (caddr_t), KM_SLEEP);
	for (i = 0; i < npgs; i++) {
		nset = ((uintptr_t)va >> ppmap_shift) & (nsets - 1);
		align = ((uintptr_t)va >> MMU_PAGESHIFT) &
				(ppmap_pages - 1);
		ppmap_vaddrs[PINDEX(align, nset)] = va;
		va += MMU_PAGESIZE;
	}
}

/*
 * Allocate a cache consistent virtual address to map a page, pp,
 * with protection, vprot; and map it in the MMU, using the most
 * efficient means possible.  The argument avoid is a virtual address
 * hint which when masked yields an offset into a virtual cache
 * that should be avoided when allocating an address to map in a
 * page.  An avoid arg of -1 means you don't care, for instance pagezero.
 *
 * machine dependent, depends on virtual address space layout,
 * understands that all kernel addresses have bit 31 set.
 */
caddr_t
ppmapin(page_t *pp, u_int vprot, caddr_t avoid)
{
	int nset;
	long align, av_align;
	caddr_t va = NULL;

	extern int fd_page_vac_align(page_t *pp);

	ASSERT(ppmap_vaddrs);
	mutex_enter(&ppmap_lock);
	if (cache & CACHE_VAC) {
		ohat_mlist_enter(pp);
		align = fd_page_vac_align(pp);
		ohat_mlist_exit(pp);
		if (align != -1) {
			/*
			 * Find an unused cache consistent slot to map
			 * the page in.
			 */
			for (nset = 0; nset < nsets; nset++) {
			    if ((long)ppmap_vaddrs[PINDEX(align, nset)] < 0) {
#ifdef PPDEBUG
					nset_hits[nset]++;
					align_hits[align]++;
#endif PPDEBUG
					va = (caddr_t)ppmap_vaddrs[
							PINDEX(align, nset)];
					break;
			    }
			}
		} else {
			/*
			 * The page has no mappings, common COW case.
			 * Avoid using a mapping that collides with
			 * the avoid address parameter in the cache.
			 * We can pick any alignment except the one we are
			 * supposed to avoid, if the first pick isn't
			 * avaliable we try others.
			 */
			if (do_pg_coloring)
				align = ((machpage_t *)pp)->p_vcolor;
			else
				align = ppmap_last++;
			if (ppmap_last >= ppmap_pages)
				ppmap_last = 0;
			av_align = ((uintptr_t)avoid & vac_mask) >>
					MMU_PAGESHIFT;
			if (align == av_align) {
				if (++align >= ppmap_pages)
					align = 0;
			}
		}
	} else {
		/*
		 * For physical caches, we can pick any address we want.
		 */
		align = ppmap_last;
	}

	if (va == NULL) {
		do {
			for (nset = 0; nset < nsets; nset++)
			    if ((long)ppmap_vaddrs[PINDEX(align, nset)] < 0) {
					va = (caddr_t)ppmap_vaddrs[
							PINDEX(align, nset)];
					goto found;
			    }
			/*
			 * first pick didn't succeed, try another
			 */
			if (++align == ppmap_pages)
				align = 0;
		} while (align != ppmap_last);
	found:
		ppmap_last = align;
	}

	if (va == NULL) {
#ifdef PPDEBUG
		ppalloc_noslot++;
		align_miss[align]++;
#endif PPDEBUG
		mutex_exit(&ppmap_lock);

		/*
		 * No free slots; get a random one from the kernel heap.
		 */
		va = vmem_alloc(heap_arena, PAGESIZE, VM_SLEEP);

		hat_memload(kas.a_hat, va, pp, HAT_NOSYNC | vprot,
			HAT_LOAD_LOCK);
	} else {
		/*
		 * use the hat layer to make mappings
		 */
		ppmap_vaddrs[PINDEX(align, nset)] = (caddr_t)1;
		mutex_exit(&ppmap_lock);
		hat_memload(kas.a_hat, va, pp, HAT_NOSYNC | vprot,
			HAT_LOAD_LOCK);
	}
	return (va);
}

void
ppmapout(caddr_t va)
{
	long align, nset;

	if (((va >= kernelheap) && (va < ekernelheap)) &&
	    (va < ppmap_start || va > ppmap_end)) {
		/*
		 * Space came from kernel heap, flush the page and
		 * return the space.
		 */
		hat_unload(kas.a_hat, va, PAGESIZE,
			(HAT_UNLOAD_NOSYNC | HAT_UNLOAD_UNLOCK));
		vmem_free(heap_arena, va, PAGESIZE);
	} else {
		/*
		 * Space came from ppmap_vaddrs[], give it back.
		 */
		align = ((uintptr_t)va >> MMU_PAGESHIFT) & (ppmap_pages - 1);
		nset = ((uintptr_t)va >> ppmap_shift) & (nsets - 1);
		if (cache & CACHE_VAC)
			ASSERT(align < ppmap_pages);
		if ((long)ppmap_vaddrs[PINDEX(align, nset)] == 1) {
			hat_unload(kas.a_hat, va, PAGESIZE,
				(HAT_UNLOAD_NOSYNC | HAT_UNLOAD_UNLOCK));
		} else
			panic("ppmapout");

		mutex_enter(&ppmap_lock);
		ppmap_vaddrs[PINDEX(align, nset)] = va;
		mutex_exit(&ppmap_lock);
	}
}

/*
 * Copy the data from the physical page represented by "frompp" to
 * that represented by "topp".
 */
void
ppcopy(page_t *frompp, page_t *topp)
{
	caddr_t from_va, to_va;

	ASSERT(PAGE_LOCKED(frompp));
	ASSERT(PAGE_LOCKED(topp));

	from_va = ppmapin(frompp, PROT_READ, (caddr_t)-1);
	to_va   = ppmapin(topp, PROT_READ | PROT_WRITE, from_va);

	locked_pgcopy(from_va, to_va, PAGESIZE);
	ppmapout(from_va);
	ppmapout(to_va);
}

/*
 * Zero the physical page from off to off + len given by `pp'
 * without changing the reference and modified bits of page.
 */
void
pagezero(page_t *pp, u_int off, u_int len)
{
	caddr_t va;

	ASSERT((int)len > 0 && (int)off >= 0 && off + len <= PAGESIZE);
	ASSERT(PAGE_LOCKED(pp));

	va = ppmapin(pp, PROT_READ | PROT_WRITE, (caddr_t)-1);
	locked_bzero(va + off, len);
	ppmapout(va);
}

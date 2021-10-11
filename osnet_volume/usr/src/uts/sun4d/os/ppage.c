/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ppage.c	1.14	99/04/14 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/systm.h>
#include <sys/vmem.h>
#include <sys/mman.h>
#include <sys/vm.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/bcopy_if.h>
#include <vm/as.h>
#include <vm/hat_srmmu.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <sys/debug.h>
#include <vm/mach_page.h>

/*
 * Architecures with a vac use this routine to quickly make
 * a vac-aligned mapping.  We don't have a vac, so we don't
 * care about that - just make this simple.
 */
caddr_t
ppmapin(page_t *pp, u_int vprot, caddr_t avoid)
{
	caddr_t va;
#ifdef lint
	avoid = avoid;
#endif

	va = vmem_alloc(heap_arena, PAGESIZE, VM_SLEEP);

	hat_memload(kas.a_hat, va, pp, vprot | HAT_NOSYNC, HAT_LOAD_LOCK);

	return (va);
}

void
ppmapout(caddr_t va)
{
	hat_unload(kas.a_hat, va, PAGESIZE, HAT_UNLOAD_UNLOCK);
	vmem_free(heap_arena, va, PAGESIZE);
}

/*
 * Copy the data from the physical page represented by "frompp" to
 * that represented by "topp".
 */
void
ppcopy(page_t *frompp, page_t *topp)
{
	pfn_t spfn, dpfn;
	u_int cacheable;

	ASSERT(PAGE_LOCKED(frompp));
	ASSERT(PAGE_LOCKED(topp));

	cacheable = (1 << (36 - MMU_STD_FIRSTSHIFT));
	spfn = ((machpage_t *)frompp)->p_pagenum | cacheable;
	dpfn = ((machpage_t *)topp)->p_pagenum | cacheable;

	ASSERT(spfn != (ulong)-1);
	ASSERT(dpfn != (ulong)-1);

	hwpage_copy(spfn, dpfn);
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

	if (off == 0 && len == PAGESIZE) {
		u_int cacheable = (1 << (36 - MMU_STD_FIRSTSHIFT));
		pfn_t pfn = ((machpage_t *)pp)->p_pagenum | cacheable;

		hwpage_zero(pfn);
	} else {
		va = ppmapin(pp, PROT_READ | PROT_WRITE, (caddr_t)-1);
		bzero(va + off, len);
		ppmapout(va);
	}
}

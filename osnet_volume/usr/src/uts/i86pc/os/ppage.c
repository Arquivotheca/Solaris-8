/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ppage.c	1.23	99/04/14 SMI"

#include <sys/t_lock.h>
#include <sys/vmem.h>
#include <sys/mman.h>
#include <sys/vm.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/systm.h>
#include <vm/as.h>
#include <vm/hat.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

/*
 * ppcopy() and pagezero() have moved to i86/vm/vm_machdep.c
 */

/*
 * Architecures with a vac use this routine to quickly make
 * a vac-aligned mapping.  We don't have a vac, so we don't
 * care about that - just make this simple.
 */
/* ARGSUSED2 */
caddr_t
ppmapin(page_t *pp, u_int vprot, caddr_t avoid)
{
	caddr_t va;

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
 * Map the page pointed to by pp into the kernel virtual address space.
 * This routine is used by the rootnexus.
 */
void
i86_pp_map(page_t *pp, caddr_t kaddr)
{
	hat_devload(kas.a_hat, kaddr, MMU_PAGESIZE, page_pptonum(pp),
	    HAT_STORECACHING_OK | PROT_READ | PROT_WRITE | HAT_NOSYNC,
	    HAT_LOAD_LOCK);
}

/*
 * Map the page containing the virtual address into the kernel virtual address
 * space.  This routine is used by the rootnexus.
 */
void
i86_va_map(caddr_t vaddr, struct as *asp, caddr_t kaddr)
{
	pfn_t pfnum;

	pfnum = hat_getpfnum(asp->a_hat, vaddr);
	hat_devload(kas.a_hat, kaddr, MMU_PAGESIZE, pfnum,
	    HAT_STORECACHING_OK | PROT_READ | PROT_WRITE | HAT_NOSYNC,
	    HAT_LOAD_LOCK);
}

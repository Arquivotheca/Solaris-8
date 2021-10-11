/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bp_map.c	1.1	99/04/14 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/buf.h>
#include <sys/vmem.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/machparam.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <vm/seg_kmem.h>

#ifdef sun4u
#include <sys/cpu_module.h>
#define	BP_FLUSH(addr, size)	flush_instr_mem((void *)addr, size);
#else
#define	BP_FLUSH(addr, size)
#endif

static vmem_t *bp_map_arena;
static size_t bp_align;
static uint_t bp_devload_flags = PROT_READ | PROT_WRITE | HAT_NOSYNC;
int bp_max_cache = 1 << 17;		/* 128K default; tunable */

static void *
bp_vmem_alloc(vmem_t *vmp, size_t size, int vmflag)
{
	return (vmem_xalloc(vmp, size, bp_align, 0, 0, NULL, NULL, vmflag));
}

void
bp_init(size_t align, uint_t devload_flags)
{
	bp_align = MAX(align, PAGESIZE);
	bp_devload_flags |= devload_flags;

	if (bp_align <= bp_max_cache)
		bp_map_arena = vmem_create("bp_map", NULL, 0, bp_align,
		    bp_vmem_alloc, vmem_free, heap_arena,
		    MIN(8 * bp_align, bp_max_cache), VM_SLEEP);
}

/*
 * Convert bp for pageio/physio to a kernel addressable location.
 */
void
bp_mapin(struct buf *bp)
{
	struct as *as;
	pfn_t pfnum;
	page_t *pp = NULL;
	page_t **pplist = NULL;
	caddr_t kaddr;
	caddr_t addr = (caddr_t)bp->b_un.b_addr;
	uintptr_t off = (uintptr_t)addr & PAGEOFFSET;
	size_t size = P2ROUNDUP(bp->b_bcount + off, PAGESIZE);
	pgcnt_t npages = btop(size);
	int color;

	if (!(bp->b_flags & (B_PAGEIO | B_PHYS)) || (bp->b_flags & B_REMAPPED))
		return;		/* no pageio/physio or already mapped in */

	ASSERT((bp->b_flags & (B_PAGEIO | B_PHYS)) != (B_PAGEIO | B_PHYS));

	/*
	 * Allocate kernel virtual space for remapping.
	 */
	color = bp_color(bp);
	ASSERT(color < bp_align);

	if (bp_map_arena != NULL)
		kaddr = (caddr_t)vmem_alloc(bp_map_arena,
		    P2ROUNDUP(color + size, bp_align), VM_SLEEP) + color;
	else
		kaddr = vmem_xalloc(heap_arena, size, bp_align, color,
		    0, NULL, NULL, VM_SLEEP);

	ASSERT(P2PHASE((uintptr_t)kaddr, bp_align) == color);

	/*
	 * Map bp into the virtual space we just allocated.
	 */
	if (bp->b_flags & B_PAGEIO) {
		pp = bp->b_pages;
	} else if (bp->b_flags & B_SHADOW) {
		pplist = bp->b_shadow;
	} else {
		if (bp->b_proc == NULL || (as = bp->b_proc->p_as) == NULL)
			as = &kas;
	}

	bp->b_flags |= B_REMAPPED;
	bp->b_un.b_addr = kaddr + off;

	while (npages-- != 0) {
		if (pp) {
			pfnum = ((machpage_t *)pp)->p_pagenum;
			pp = pp->p_next;
		} else if (pplist == NULL) {
			pfnum = hat_getpfnum(as->a_hat, addr - off);
			addr += PAGESIZE;
		} else {
			pfnum = ((machpage_t *)*pplist)->p_pagenum;
			pplist++;
		}

		hat_devload(kas.a_hat, kaddr, PAGESIZE, pfnum,
		    bp_devload_flags, HAT_LOAD_LOCK);

		kaddr += PAGESIZE;
	}
}

/*
 * Release all the resources associated with a previous bp_mapin() call.
 */
void
bp_mapout(struct buf *bp)
{
	if (bp->b_flags & B_REMAPPED) {
		uintptr_t addr = (uintptr_t)bp->b_un.b_addr;
		uintptr_t off = addr & PAGEOFFSET;
		uintptr_t base = addr - off;
		uintptr_t color = P2PHASE(base, bp_align);
		size_t size = P2ROUNDUP(bp->b_bcount + off, PAGESIZE);
		bp->b_un.b_addr = (caddr_t)off;		/* debugging aid */
		BP_FLUSH(base, size);
		hat_unload(kas.a_hat, (void *)base, size,
		    HAT_UNLOAD_NOSYNC | HAT_UNLOAD_UNLOCK);
		if (bp_map_arena != NULL)
			vmem_free(bp_map_arena, (void *)(base - color),
			    P2ROUNDUP(color + size, bp_align));
		else
			vmem_free(heap_arena, (void *)base, size);
		bp->b_flags &= ~B_REMAPPED;
	}
}

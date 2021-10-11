/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_alloc.c	1.17	97/06/30 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * This allocator has SMCC-OBP-like semantics associated with it.
 * Specifically, the alignment value specifies both a physical
 * and virtual alignment. If virthint is zero, a suitable virt
 * is chosen. In either case, align is not ignored.
 *
 * This routine returns NULL on failure.
 * This routine is suitable for (the given semantics) machines with
 * a 2-cell physical address.
 *
 * Memory allocated with prom_alloc can be freed with prom_free.
 *
 * The generic allocator is prom_malloc.
 *
 */

caddr_t
prom_alloc(caddr_t virthint, size_t size, u_int align)
{

	caddr_t virt = virthint;
	unsigned long long physaddr;

	if (align == 0)
		align = (u_int)1;

	/*
	 * First, allocate or claim the virtual address space.
	 * In either case, after this code, "virt" is the chosen address.
	 */
	if (virthint == 0) {
		virt = prom_allocate_virt(align, size);
		if (virt == (caddr_t)-1)
			return ((caddr_t)0);
	} else {
		if (prom_claim_virt(size, virthint) == (caddr_t)-1)
			return ((caddr_t)0);
	}

	/*
	 * Next, allocate the physical address space, at the specified
	 * physical alignment (or 1 byte alignment, if none specified)
	 */

	if (prom_allocate_phys(size, align, &physaddr) == -1) {

		/*
		 * Request failed, free virtual address space and return.
		 */
		prom_free_virt(size, virt);
		return ((caddr_t)0);
	}

	/*
	 * Next, create a mapping from the physical to virtual address,
	 * using a default "mode".
	 */

	if (prom_map_phys(-1, size, virt, physaddr) == -1)  {

		/*
		 * The call failed; release the physical and virtual
		 * addresses allocated or claimed, and return.
		 */

		prom_free_virt(size, virt);
		prom_free_phys(size, physaddr);
		return ((caddr_t)0);
	}
	return (virt);
}

/*
 * This is the generic client interface to "claim" memory.
 * These two routines belong in the common directory.
 */
caddr_t
prom_malloc(caddr_t virt, size_t size, u_int align)
{
	cell_t ci[7];
	int rv;

	ci[0] = p1275_ptr2cell("claim");	/* Service name */
	ci[1] = (cell_t)3;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_ptr2cell(virt);		/* Arg1: virt */
	ci[4] = p1275_size2cell(size);		/* Arg2: size */
	ci[5] = p1275_uint2cell(align);		/* Arg3: align */

	promif_preprom();
	rv = p1275_cif_handler(&ci);
	promif_postprom();

	if (rv == 0)
		return ((caddr_t)p1275_cell2ptr(ci[6])); /* Res1: base */
	return ((caddr_t)-1);
}


void
prom_free(caddr_t virt, size_t size)
{
	cell_t ci[5];

	ci[0] = p1275_ptr2cell("release");	/* Service name */
	ci[1] = (cell_t)2;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */
	ci[3] = p1275_ptr2cell(virt);		/* Arg1: virt */
	ci[4] = p1275_size2cell(size);		/* Arg2: size */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();
}

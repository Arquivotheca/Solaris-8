/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_map.c	1.15	95/10/25 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Mapping routines suitable for implementations using 2-cell physical
 * address formats.  Use of these routines makes the caller
 * platform-dependent.  The implementation of these routines is
 * a bit sun-hardware centric, for historical use by SunOS and standalones.
 */

caddr_t
prom_map(caddr_t virthint, unsigned long long physaddr, u_int size)
{
	caddr_t virt;

	/*
	 * If no virthint, allocate it; otherwise claim it,
	 * the physical address is assumed to be a device or
	 * already claimed, or not appearing in a resource list.
	 */
	if (virthint == (caddr_t)0)  {
		if ((virt = prom_allocate_virt((u_int)1, size)) == 0)
			return ((caddr_t)0);
	} else {
		virt = virthint;
		if (prom_claim_virt(size, virt) != virt)
			return ((caddr_t)0);
	}

	if (prom_map_phys(-1, size, virt, physaddr) != 0) {
		/*
		 * The map operation failed, free the virtual
		 * addresses we allocated or claimed.
		 */
		(void) prom_free_virt(size, virt);
		return ((caddr_t)0);
	}
	return (virt);
}

void
prom_unmap(caddr_t virt, u_int size)
{
	(void) prom_unmap_virt(size, virt);
	prom_free_virt(size, virt);
}

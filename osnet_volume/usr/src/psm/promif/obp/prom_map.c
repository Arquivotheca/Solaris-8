/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_map.c	1.10	99/10/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

caddr_t
prom_map(caddr_t virthint, uint_t space, uint_t phys, uint_t size)
{
	caddr_t addr;

	switch (obp_romvec_version)  {
	case OBP_V0_ROMVEC_VERSION:
		return ((char *)0);

	default:
		promif_preprom();
		addr = OBP_V2_MAP(virthint, space, phys, size);
		promif_postprom();
		return (addr);
	}
}

void
prom_unmap(caddr_t virthint, uint_t size)
{
	switch (obp_romvec_version)  {
	case OBP_V0_ROMVEC_VERSION:
		return;

	default:
		OBP_V2_UNMAP(virthint, size);
		return;
	}
}

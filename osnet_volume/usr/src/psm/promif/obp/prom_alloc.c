/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_alloc.c	1.13	97/06/30 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

caddr_t
prom_alloc(caddr_t virthint, size_t size, u_int align)
{
	caddr_t	rv;

	switch (obp_romvec_version)  {

	case OBP_V0_ROMVEC_VERSION:
		return ((caddr_t)0);

	case OBP_V2_ROMVEC_VERSION:
		promif_preprom();
		rv = OBP_V2_ALLOC(virthint, size);
		promif_postprom();
		break;

	case OBP_V3_ROMVEC_VERSION:
	default:
		promif_preprom();
		if (align && prom_aligned_allocator)
			rv = OBP_V3_ALLOC(virthint, size, align);
		else
			rv = OBP_V2_ALLOC(virthint, size);
		promif_postprom();
		break;
	}

	return (rv);
}

void
prom_free(caddr_t virt, size_t size)
{
	switch (obp_romvec_version)  {

	case OBP_V0_ROMVEC_VERSION:
#if !defined(lint)
		PROMIF_DEBUG(("prom_free on V0?\n"));
#endif
		break;			/* panic? */

	case OBP_V2_ROMVEC_VERSION:
	case OBP_V3_ROMVEC_VERSION:
	default:
		promif_preprom();
		OBP_V2_FREE(virt, size);
		promif_postprom();
		break;
	}
}

/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_phandle.c	1.7	96/02/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

phandle_t
prom_getphandle(ihandle_t i)
{
	phandle_t ph;

	switch (obp_romvec_version)  {
	case OBP_V0_ROMVEC_VERSION:
		return ((phandle_t)(OBP_BADNODE));

	default:
		promif_preprom();
		ph = OBP_V2_PHANDLE(i);
		promif_postprom();
		return (ph);
	}
}

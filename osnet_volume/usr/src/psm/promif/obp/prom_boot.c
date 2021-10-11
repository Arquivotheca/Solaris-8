/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_boot.c	1.7	96/02/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

char *
prom_bootargs(void)
{
	switch (obp_romvec_version) {

	case OBP_V0_ROMVEC_VERSION:
		return ((char *)0);

	default:
	case OBP_V2_ROMVEC_VERSION:
	case OBP_V3_ROMVEC_VERSION:
		return (OBP_V2_BOOTARGS);
	}
}


struct bootparam *
prom_bootparam(void)
{
	switch (obp_romvec_version)  {

	case OBP_V0_ROMVEC_VERSION:
		return (OBP_V0_BOOTPARAM);

	default:
		PROMIF_DPRINTF(("prom_bootparam on V2 or greater?\n"));
		return ((struct bootparam *)0);
	}
}

static char bootpath[OBP_MAXPATHLEN];

char *
prom_bootpath(void)
{
	switch (obp_romvec_version) {

	case OBP_V0_ROMVEC_VERSION:
		return ((char *)0);

	default:
	case OBP_V2_ROMVEC_VERSION:
	case OBP_V3_ROMVEC_VERSION:
		if (bootpath[0] == (char)0)  {
			(void) prom_strcpy(bootpath, OBP_V2_BOOTPATH);
			prom_pathname(bootpath);
		}
		return (bootpath);
	}
}

/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_putchar.c	1.7	96/02/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

int
prom_mayput(char c)
{
	int i;

	promif_preprom();

	switch (obp_romvec_version)  {

	case OBP_V0_ROMVEC_VERSION:
		i = OBP_V0_MAYPUT(c);
		break;

	default:
		i = OBP_V2_WRITE(OBP_V2_STDOUT, &c, 1) == 1 ? 0 : -1;
		break;
	}

	promif_postprom();

	return (i);
}

void
prom_putchar(char c)
{
	while (prom_mayput(c) == -1)
		;
}

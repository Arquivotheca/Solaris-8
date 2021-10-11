/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_getchar.c	1.7	96/02/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

u_char
prom_getchar(void)
{
	int c;

	while ((c = prom_mayget()) == -1)
		;

	return ((u_char)c);
}


int
prom_mayget(void)
{
	char c;
	int rv;

	promif_preprom();

	switch (obp_romvec_version)  {

	case OBP_V0_ROMVEC_VERSION:
		rv = OBP_V0_MAYGET();
		break;

	default:
		rv = OBP_V2_READ(OBP_V2_STDIN, &c, 1) == 1 ?
		    (int) c : -1;
		break;
	}

	promif_postprom();

	return (rv);
}

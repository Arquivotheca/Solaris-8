/*
 * Copyright (c) 1991-1993 by Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <sys/module.h>

#pragma ident	"@(#)module_tsu.c	1.5	94/01/15 SMI"
	/* From SunOS 4.1.1 */

/*
 * Support for modules based on the TI Tsunami chipset.
 */

int
tsu_module_identify(u_int mcr)
{
	u_int psr = getpsr();

	if (((psr >> 24) & 0xff) == 0x41 &&
	    ((mcr >> 24) & 0xff) != 0x00) /* covers viking 2.x case */
		return (1);

	return (0);
}

/*ARGSUSED*/
void
tsu_module_setup(mcr)
	int	mcr;
{
	/* No setup needed for kadb. */
}

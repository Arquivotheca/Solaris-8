/*
 * Copyright (c) 1991-1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)module_vik.c	1.4	93/03/22 SMI"
	/* From SunOS 4.1.1 */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/mmu.h>

/*
 * Support for modules based on the TI Viking chipset.
 */

int mxcc = 0;
int viking = 0;

int
vik_module_identify(u_int mcr)
{
	u_int psr = getpsr();

	/* 1.2 or 3.X */
	if (((psr >> 24) & 0xff) == 0x40)
		viking = 1;

	/* 2.X */
	if (((psr >> 24) & 0xff) == 0x41 &&
	    ((mcr >> 24) & 0xff) == 0x00)
		viking = 1;

	if (!viking)
		return (0);


	/* Find out if we have a MXCC. */
	if (!(mcr & CPU_VIK_MB))
		mxcc = 1;

	return (1);
}

/*ARGSUSED*/
void
vik_module_setup(mcr)
	int	mcr;
{
	/* No setup needed for kadb. */
}

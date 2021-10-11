/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_reboot.c	1.8	94/11/12 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * There should be no return from this function
 */
void
prom_reboot(char *bootstr)
{
	cell_t ci[4];

	ci[0] = p1275_ptr2cell("boot");		/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */
	ci[3] = p1275_ptr2cell(bootstr);	/* Arg1: bootspec */
	(void) p1275_cif_handler(&ci);
	prom_panic("prom_reboot: reboot call returned!");
}

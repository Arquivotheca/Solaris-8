/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_power_off.c	1.1	95/05/11 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * This interface allows the client to power off the machine.
 * There's no return from this service.
 */
void
prom_power_off()
{
	cell_t ci[3];

	ci[0] = p1275_ptr2cell("SUNW,power-off");	/* Service name */
	ci[1] = (cell_t) 0;				/* #argument cells */
	ci[2] = (cell_t) 0;				/* #result cells */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();
}

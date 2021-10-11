/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_gettime.c	1.11	95/07/19 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * This prom entry point cannot be used once the kernel is up
 * and running (after stop_mon_clock is called) since the kernel
 * takes over level 14 interrupt handling which the PROM depends
 * on to update the time.
 */

u_int
prom_gettime(void)
{
	cell_t ci[4];

	ci[0] = p1275_ptr2cell("milliseconds");	/* Service name */
	ci[1] = (cell_t)0;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #return cells */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (p1275_cell2uint(ci[3]));	/* Res0: time in ms. */
}

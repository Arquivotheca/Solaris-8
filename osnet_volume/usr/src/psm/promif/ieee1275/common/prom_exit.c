/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_exit.c	1.9	94/11/14 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * There should be no return from this function
 */
void
prom_exit_to_mon(void)
{
	cell_t ci[3];

	ci[0] = p1275_ptr2cell("exit");		/* Service name */
	ci[1] = (cell_t)0;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #return cells */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();
}

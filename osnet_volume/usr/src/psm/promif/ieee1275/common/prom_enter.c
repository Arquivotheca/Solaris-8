/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_enter.c	1.11	94/11/14 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

void
prom_enter_mon(void)
{
	cell_t ci[3];

	ci[0] = p1275_ptr2cell("enter");	/* Service name */
	ci[1] = (cell_t)0;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #return cells */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();
}

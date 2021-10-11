/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_set_traptable.c	1.2	95/03/01 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * This interface allows the client to safely take over the %tba by
 * the prom's service. The prom will take care of the quiescence of
 * interrupts and handle any pending soft interrupts.
 */
void
prom_set_traptable(void *tba_addr)
{
	cell_t ci[4];

	ci[0] = p1275_ptr2cell("SUNW,set-trap-table");	/* Service name */
	ci[1] = (cell_t) 1;			/* #argument cells */
	ci[2] = (cell_t) 0;			/* #result cells */
	ci[3] = p1275_ptr2cell(tba_addr);	/* Arg1: tba address */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();
}

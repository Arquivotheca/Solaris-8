/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)prom_handler.c	1.10	95/02/21 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

void *
prom_set_callback(void *handler)
{
	cell_t ci[5];

	ci[0] = p1275_ptr2cell("set-callback");	/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #return cells */
	ci[3] = p1275_ptr2cell(handler);	/* Arg1: New handler */
	ci[4] = (cell_t)-1;			/* Res1: Prime result */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (p1275_cell2ptr(ci[4]));		/* Res1: Old handler */
}

void
prom_set_symbol_lookup(void *sym2val, void *val2sym)
{
	cell_t ci[5];

	ci[0] = p1275_ptr2cell("set-symbol-lookup");	/* Service name */
	ci[1] = (cell_t)2;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #return cells */
	ci[3] = p1275_ptr2cell(sym2val);	/* Arg1: s2v handler */
	ci[4] = p1275_ptr2cell(val2sym);	/* Arg1: v2s handler */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();
}

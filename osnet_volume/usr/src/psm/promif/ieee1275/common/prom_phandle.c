/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_phandle.c	1.10	94/11/16 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

phandle_t
prom_getphandle(ihandle_t i)
{
	cell_t ci[5];

	ci[0] = p1275_ptr2cell("instance-to-package");	/* Service name */
	ci[1] = (cell_t)1;				/* #argument cells */
	ci[2] = (cell_t)1;				/* #result cells */
	ci[3] = p1275_ihandle2cell(i);			/* Arg1: instance */
	ci[4] = p1275_dnode2cell(OBP_BADNODE);		/* Res1: Prime result */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (p1275_cell2phandle(ci[4]));		/* Res1: package */
}

/*
 * Copyright (c) 1994-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_sunfire.c	1.2	98/10/21 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/* SunFire only */
int
prom_sunfire_cpu_off(void)
{
	cell_t ci[3];

	/* Service name */
	ci[0] = p1275_ptr2cell("SUNW,Ultra-Enterprise,cpu-off");
	ci[1] = (cell_t)0;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */

	/*
	 * This is an unlocked entry into the prom.
	 *
	 * promif_preprom(), ... will be called by the controlling
	 * cpu.  The controlling cpu handles prom contention
	 * while the victim is in transition.
	 */
	(void) p1275_cif_handler(&ci);

	return (0);
}

/*
 * These interfaces allow the client to attach/detach board.
 */
int
prom_sunfire_attach_board(u_int board)
{
	cell_t ci[5];
	int rv;

	ci[0] = p1275_ptr2cell("SUNW,Ultra-Enterprise,add-brd"); /* name */
	ci[1] = (cell_t)1;				/* #argument cells */
	ci[2] = (cell_t)1;				/* #result cells */
	ci[3] = p1275_uint2cell(board);

	promif_preprom();
	rv = p1275_cif_handler(&ci);
	promif_postprom();

	if (rv != 0)
		return (rv);
	if (p1275_cell2int(ci[4]) != 0)			/* Res1: Catch result */
		return (-1);

	return (0);
}

int
prom_sunfire_detach_board(u_int board)
{
	cell_t ci[5];
	int rv;

	ci[0] = p1275_ptr2cell("SUNW,Ultra-Enterprise,rm-brd");	/* name */
	ci[1] = (cell_t)1;				/* #argument cells */
	ci[2] = (cell_t)1;				/* #result cells */
	ci[3] = p1275_uint2cell(board);

	promif_preprom();
	rv = p1275_cif_handler(&ci);
	promif_postprom();

	if (rv != 0)
		return (rv);
	if (p1275_cell2int(ci[4]) != 0)			/* Res1: Catch result */
		return (-1);

	return (0);
}

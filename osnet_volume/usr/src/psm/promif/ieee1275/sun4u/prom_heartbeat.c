/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_heartbeat.c	1.5	95/02/21 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Provide 10 millisecond heartbeat for the PROM. A client that has taken over
 * the trap table and clock interrupts, but is not quite ready to take over the
 * function of polling the input-device for an abort sequence (L1/A or BREAK)
 * may use this function to instruct the PROM to poll the keyboard. If used,
 * this function should be called every 10 milliseconds.
 */
int
prom_heartbeat(int msecs)
{
	cell_t ci[5];

	ci[0] = p1275_ptr2cell("SUNW,heartbeat");	/* Service name */
	ci[1] = (cell_t)1;				/* #argument cells */
	ci[2] = (cell_t)1;				/* #result cells */
	ci[3] = p1275_int2cell(msecs);			/* Arg1: msecs */
	ci[4] = (cell_t)0;				/* Prime the result */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (p1275_cell2int(ci[4]));			/* Res1: abort-flag */
}

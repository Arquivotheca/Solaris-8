/*
 * Copyright (c) 1994-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_cpuctl.c	1.23	98/01/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

int
prom_idle_self(void)
{
	cell_t ci[3];

	ci[0] = p1275_ptr2cell("SUNW,idle-self");	/* Service name */
	ci[1] = (cell_t)0;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */
	(void) p1275_cif_handler(&ci);		/* Do NOT Lock here */
	return (0);
}

int
prom_resumecpu(dnode_t node)
{
	cell_t ci[4];

	ci[0] = p1275_ptr2cell("SUNW,resume-cpu");	/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */
	ci[3] = p1275_dnode2cell(node);		/* Arg1: nodeid to start */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (0);
}

int
prom_stop_self(void)
{
	cell_t ci[3];

	ci[0] = p1275_ptr2cell("SUNW,stop-self");	/* Service name */
	ci[1] = (cell_t)0;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */
	(void) p1275_cif_handler(&ci);		/* Do NOT lock */
	return (0);
}


int
prom_startcpu(dnode_t node, caddr_t pc, int arg)
{
	cell_t ci[6];

	ci[0] = p1275_ptr2cell("SUNW,start-cpu");	/* Service name */
	ci[1] = (cell_t)3;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */
	ci[3] = p1275_dnode2cell(node);		/* Arg1: nodeid to start */
	ci[4] = p1275_ptr2cell(pc);		/* Arg2: pc */
	ci[5] = p1275_int2cell(arg);		/* Arg3: cpuid */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (0);
}

int
prom_wakeupcpu(dnode_t node)
{
	cell_t ci[5];
	int	rv;

	ci[0] = p1275_ptr2cell("SUNW,wakeup-cpu");	/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_dnode2cell(node);		/* Arg1: nodeid to wakeup */

	promif_preprom();
	rv = p1275_cif_handler(&ci);
	promif_postprom();

	if (rv != 0)
		return (rv);
	else
		return (p1275_cell2int(ci[4]));	/* Res1: Catch result */
}

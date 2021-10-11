/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_cpuctl.c	1.10	96/02/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

int
prom_idlecpu(dnode_t node)
{
	switch (obp_romvec_version)  {

	case OBP_V0_ROMVEC_VERSION:
	case OBP_V2_ROMVEC_VERSION:
		prom_panic("prom_idlecpu");
		/*NOTREACHED*/

	default:
		/* XXX: flush_windows? */
		return (OBP_V3_IDLECPU(node));
	}
}


int
prom_resumecpu(dnode_t node)
{
	int rv;

	switch (obp_romvec_version)  {
	case OBP_V0_ROMVEC_VERSION:
	case OBP_V2_ROMVEC_VERSION:
		prom_panic("prom_resumecpu");
		/*NOTREACHED*/

	default:
		promif_preprom();
		rv = OBP_V3_RESUMECPU(node);
		promif_postprom();
		return (rv);
	}
}


int
prom_stopcpu(dnode_t node)
{
	switch (obp_romvec_version)  {
	case OBP_V0_ROMVEC_VERSION:
	case OBP_V2_ROMVEC_VERSION:
		prom_panic("prom_stopcpu");
		/*NOTREACHED*/

	default:
		/* XXX: flush_windows? */
		return (OBP_V3_STOPCPU(node));
	}
}

int
prom_startcpu(dnode_t node, struct prom_reg *context, int whichcontext,
    caddr_t pc)
{
	int rv;

	switch (obp_romvec_version)  {

	case OBP_V0_ROMVEC_VERSION:
	case OBP_V2_ROMVEC_VERSION:
		prom_panic("prom_startcpu");
		/*NOTREACHED*/

	default:
		promif_preprom();
		rv = OBP_V3_STARTCPU(node, context, whichcontext, pc);
		promif_postprom();
		return (rv);
	}
}

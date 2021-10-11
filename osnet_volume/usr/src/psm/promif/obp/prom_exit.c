/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_exit.c	1.7	96/02/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * There should be no return from this function
 */
void
prom_exit_to_mon(void)
{
	promif_preprom();
	(void) prom_montrap(OBP_EXIT_TO_MON);
	promif_postprom();
	/* NOTREACHED */
}

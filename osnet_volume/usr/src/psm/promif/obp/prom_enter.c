/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_enter.c	1.9	96/02/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

void
prom_enter_mon(void)
{
	promif_preprom();
	(*OBP_ENTER_MON)();
	promif_postprom();
}

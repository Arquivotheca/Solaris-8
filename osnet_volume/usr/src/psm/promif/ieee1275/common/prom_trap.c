/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_trap.c	1.6	94/11/12 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

void
prom_montrap(void (*funcptr)())
{
	((*funcptr)());
}

/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_trap.c	1.5	94/06/10 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Not for the kernel.  Dummy replacement for kernel montrap.
 */
void
prom_montrap(void (*funcptr)())
{
	((*funcptr)());
}

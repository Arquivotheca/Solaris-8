/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)module_ross625.c	1.1	95/07/19 SMI"

#include <sys/types.h>
#include <sys/module_ross625.h>

/*
 * ross625_module_identify
 *
 * Return 1 if _mcr_ argument indicates an RT625-based module, 0 otherwise.
 */
int
ross625_module_identify(u_int mcr)
{
	return ((mcr & RT625_CTL_IDMASK) == RT625_CTL_ID);
}

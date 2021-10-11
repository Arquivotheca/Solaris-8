/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)module_ross.c	1.1	95/07/18 SMI"

#include <sys/types.h>

#define	VERSION_MASK	0x0F000000
#define	HYP_VERSION	0x07000000

int
ross_module_identify(u_int mcr)
{
	if (((mcr >> 24) & 0xf0) == 0x10 &&
	    ((mcr & VERSION_MASK) != HYP_VERSION))
		return (1);

	return (0);
}


/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)arch.c	1.1	99/01/11 SMI"

#include <stdio.h>
#include <string.h>

#include "list.h"

int
assign_arch(const char *architecture)
{
	int	arch = 0;

#if defined(sparc)
	if (strcmp(architecture, "sparc") == 0)
		arch = P_SPARC;
	else if (strcmp(architecture, "ISA") == 0)
		arch = P_SPARC;
	else if (strcmp(architecture, "all") == 0)
		arch = P_SPARC;
	else if (strcmp(architecture, "sparc.sun4") == 0)
		arch = P_SUN4;
	else if (strcmp(architecture, "sparc.sun4c") == 0)
		arch = P_SUN4c;
	else if (strcmp(architecture, "sparc.sun4u") == 0)
		arch = P_SUN4u;
	else if (strcmp(architecture, "sparc.sun4d") == 0)
		arch = P_SUN4d;
	else if (strcmp(architecture, "sparc.sun4e") == 0)
		arch = P_SUN4e;
	else if (strcmp(architecture, "sparc.sun4m") == 0)
		arch = P_SUN4m;
#elif defined(i386)
	if (strcmp(architecture, "i386") == 0)
		arch = P_I386;
	else if (strcmp(architecture, "ISA") == 0)
		arch = P_I386;
	else if (strcmp(architecture, "all") == 0)
		arch = P_I386;
	else if (strcmp(architecture, "i386.i86pc") == 0)
		arch = P_I86PC;
#elif defined(__ppc)
	if (strcmp(architecture, "ppc") == 0)
		arch = P_PPC;
	else if (strcmp(architecture, "ISA") == 0)
		arch = P_PPC;
	else if (strcmp(architecture, "all") == 0)
		arch = P_PPC;
	else if (strcmp(architecture, "ppc.prep") == 0)
		arch = P_PREP;
#else
#error "Unknown instruction set"
#endif

	return (arch);
}

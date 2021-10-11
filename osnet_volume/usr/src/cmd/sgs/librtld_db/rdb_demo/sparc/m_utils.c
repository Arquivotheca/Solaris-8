
/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)m_utils.c	1.2	96/09/10 SMI"

#include <stdio.h>
#include <string.h>

#include "rdb.h"


void
print_mach_varstring(struct ps_prochandle * ph, const char * varname)
{
	if (strcmp(varname, "ins") == 0) {
		display_in_regs(ph, 0);
		return;
	}
	if (strcmp(varname, "globs") == 0) {
		display_global_regs(ph, 0);
		return;
	}
	if (strcmp(varname, "outs") == 0) {
		display_out_regs(ph, 0);
		return;
	}
	if (strcmp(varname, "locs") == 0) {
		display_local_regs(ph, 0);
		return;
	}
	if (strcmp(varname, "specs") == 0) {
		display_special_regs(ph, 0);
		return;
	}
	printf("print: unknown variable given ($%s)\n", varname);
}

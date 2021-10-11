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
#pragma ident	"@(#)rdb_mach.c	1.2	96/09/10 SMI"


#include	<stdlib.h>
#include	<unistd.h>
#include	"rdb.h"

void *
get_bptinstr(int * bpsz)
{
	static unsigned char	bpt_instr = BPINSTR;
	if (bpsz)
		*bpsz = BPINSTRSZ;
	return (&bpt_instr);
}

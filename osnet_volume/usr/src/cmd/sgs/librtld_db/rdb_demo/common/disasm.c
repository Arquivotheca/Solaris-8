
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
#pragma ident	"@(#)disasm.c	1.2	98/03/18 SMI"

#include <stdio.h>

#include "rdb.h"


typedef char *(*FUNCPTR)();


/*
 * stub routine until I can figure out what to plug in here.
 */
/* ARGSUSED 1 */
char *
disassemble(unsigned int instr, unsigned long pc,
	FUNCPTR prtAddress, unsigned int next, unsigned int prev,
	int vers)
{
	static char	buf[256];
	sprintf(buf, "0x%x", instr);
	return (buf);
}

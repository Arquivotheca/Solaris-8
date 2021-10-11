/* LINTLIBRARY */
/* PROTOLIB1 */

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc. 
 * All rights reserved. 
 */ 

#pragma ident	"@(#)lintsup.c	1.1	98/09/04 SMI"

/*
 * Supplimental Pseudo-code to get lint to consider
 * these symbols used.
 */

#include <stdio.h>
#include "debug.h"
#include "msg.h"

void
foo()
{
	char * x = realloc(0, dbg_mask);
	dbg_print(_ld_msg((Msg)&__ld_msg[0]), x);
	free(x);
}

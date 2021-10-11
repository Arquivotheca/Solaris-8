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

#include "msg.h"

void
foo()
{
	(void) _elfdump_msg((Msg)&__elfdump_msg[0]);
}

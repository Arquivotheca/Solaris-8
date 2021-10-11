/* LINTLIBRARY */
/* PROTOLIB1 */

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lintsup.c	1.1	99/08/13 SMI"

/*
 * Supplimental Pseudo-code to get lint to consider
 * these symbols used.
 */
#include "msg.h"

void
foo()
{
	(void) _crle_msg((Msg)&__crle_msg[0]);
}

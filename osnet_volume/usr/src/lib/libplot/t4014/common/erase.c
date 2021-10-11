/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)erase.c	1.7	97/10/01 SMI"	/* SVr4.0 1.2	*/
/*LINTLIBRARY*/

#include <unistd.h>
#include "con.h"

void
erase(void)
{
	putch(033);
	putch(014);
	ohiy = -1;
	ohix = -1;
	oextra = -1;
	oloy = -1;
	(void) sleep(2);
}

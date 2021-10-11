/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)line.c	1.7	97/10/01 SMI"	/* SVr4.0 1.2	*/
/*LINTLIBRARY*/

#include <stdio.h>
#include "con.h"

void
line(short x0, short y0, short x1, short y1)
{
	(void) putc('l', stdout);
	putsi(x0);
	putsi(y0);
	putsi(x1);
	putsi(y1);
}

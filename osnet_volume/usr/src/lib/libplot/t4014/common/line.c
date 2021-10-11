/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)line.c	1.8	97/10/29 SMI"	/* SVr4.0 1.2	*/
/*LINTLIBRARY*/

#include <plot.h>

void
line(short x0, short y0, short x1, short y1)
{
	move(x0, y0);
	cont(x1, y1);
}

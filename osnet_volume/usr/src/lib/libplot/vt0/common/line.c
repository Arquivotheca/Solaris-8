/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)line.c	1.8	97/10/29 SMI"	/* SVr4.0 1.2	*/
/*LINTLIBRARY*/

#include <unistd.h>
#include <plot.h>
#include "con.h"

int xnow, ynow;

void
line(short x0, short y0, short x1, short y1)
{
	struct {char x, c; short x0, y0, x1, y1; } p;
	p.c = 3;
	p.x0 = xsc(x0);
	p.y0 = ysc(y0);
	p.x1 = xnow = xsc(x1);
	p.y1 = ynow = ysc(y1);
	(void) write(vti, &p.c, 9);
}

void
cont(short x0, short y0)
{
	line(xnow, ynow, xsc(x0), ysc(y0));
}

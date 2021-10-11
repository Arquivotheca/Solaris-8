/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)dot.c	1.9	97/10/29 SMI"	/* SVr4.0 1.2	*/
/*LINTLIBRARY*/

#include <unistd.h>
#include "con.h"

/*ARGSUSED*/
void
dot(short xi, short yi, short dx, short n, int pat[])
{
	struct {char pad, c; short xi, yi, dx; } p;
	p.c = 7;
	(void) write(vti, &p.c, 7);
	(void) write(vti, pat, n?n&0377:256);
}

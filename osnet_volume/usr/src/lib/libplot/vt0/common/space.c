/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)space.c	1.9	97/10/29 SMI"	/* SVr4.0 1.2	*/
/*LINTLIBRARY*/

#include "con.h"

float deltx, delty;

void
space(short x0, short y0, short x1, short y1)
{
	botx = -2047.;
	boty = -2047;
	obotx = x0;
	oboty = y0;
	scalex = deltx / (x1 - x0);
	scaley = delty / (y1 - y0);
}

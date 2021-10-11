/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)scale.c	1.8	97/10/01 SMI"	/* SVr4.0 1.2	*/
/*LINTLIBRARY*/

#include "con.h"

void
scale(char i, float x, float y)
{
	switch (i) {
	default:
		return;
	case 'c':
		x *= 2.54;
		y *= 2.54;
		/*FALLTHRU*/
	case 'i':
		x /= 200;
		y /= 200;
		/*FALLTHRU*/
	case 'u':
		scalex = 1 / x;
		scaley = 1 / y;
		/*FALLTHRU*/
	}
	scaleflag = 1;
}

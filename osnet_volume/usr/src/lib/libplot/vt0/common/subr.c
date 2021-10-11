/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)subr.c	1.8	97/10/29 SMI"	/* SVr4.0 1.2	*/
/*LINTLIBRARY*/

float obotx, botx, scalex;
float oboty, boty, scaley;

int
xsc(int xi)
{
	int xa;
	xa = (xi - obotx) * scalex + botx;
	return (xa);
}

int
ysc(int yi)
{
	int ya;
	ya = (yi - oboty) * scaley + boty;
	return (ya);
}

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)circle.c	1.8	97/10/29 SMI"	/* SVr4.0 1.3	*/
/*LINTLIBRARY*/

#include <plot.h>

void
circle(short x, short y, short r)
{
	arc(x, y, x + r, y, x + r, y);
}

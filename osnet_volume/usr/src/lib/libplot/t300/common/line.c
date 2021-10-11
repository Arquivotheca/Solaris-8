/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)line.c	1.9	97/10/29 SMI"	/* SVr4.0 1.2	*/
/*LINTLIBRARY*/

#include <stdlib.h>
#include <math.h>
#include "con.h"

short xnow, ynow;

static void
iline(short cx0, short cy0, short cx1, short cy1)
{
	int maxp, tt;
	short j;
	char chx, chy;
	float xd, yd;

		movep(cx0, cy0);
		maxp = sqrt(dist2(cx0, cy0, cx1, cy1)) / 2.;
		xd = cx1 - cx0;
		yd = cy1 - cy0;
		if (xd >= 0)
			chx = RIGHT;
		else chx = LEFT;
		if (yd >= 0)
			chy = UP;
		else chy = DOWN;
		if (maxp == 0) {
			xd = 0;
			yd = 0;
		} else {
			xd /= maxp;
			yd /= maxp;
		}
		inplot();
		for (tt = 0; tt <= maxp; tt++) {
			j = cx0 + xd * tt - xnow;
			xnow += j;
			j = abs(j);
			while (j-- > 0)
				spew(chx);
			j = cy0 + yd * tt - ynow;
			ynow += j;
			j = abs(j);
			while (j-- > 0)
				spew(chy);
			spew('.');
		}
		outplot();
}

void
line(short x0, short y0, short x1, short y1)
{
	iline(xconv(xsc(x0)), yconv(ysc(y0)),
		xconv(xsc(x1)), yconv(ysc(y1)));
}


void
cont(short x0, short y0)
{
	iline(xnow, ynow, xconv(xsc(x0)), yconv(ysc(y0)));
}

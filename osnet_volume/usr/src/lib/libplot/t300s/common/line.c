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

int ynow = 0;

static void
iline(int cx0, int cy0, int cx1, int cy1) {
	int maxp, tt;
	char chx, chy, command;
	float xd, yd;
	movep(cx0, cy0);
	maxp = sqrt(dist2(cx0, cy0, cx1, cy1)) / 2.;
	xd = cx1 - cx0;
	yd = cy1 - cy0;
	command = COM|((xd < 0) << 1)|(yd < 0);
	if (maxp == 0) {
		xd = 0;
		yd = 0;
	} else {
		xd /= maxp;
		yd /= maxp;
	}
	inplot();
	spew(command);
	for (tt = 0; tt <= maxp; tt++) {
		chx = cx0 + xd * tt - xnow;
		xnow += chx;
		chx = abs(chx);
		chy = cy0 + yd * tt - ynow;
		ynow += chy;
		chy = abs(chy);
		spew(ADDR|chx<<3|chy);
	}
	outplot();
}

void
line(short x0, short y0, short x1, short y1) {
	iline(xconv(xsc(x0)), yconv(ysc(y0)), xconv(xsc(x1)), yconv(ysc(y1)));
}

void
cont(short x0, short y0) {
	iline(xnow, ynow, xconv(xsc(x0)), yconv(ysc(y0)));
}

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)subr.c	1.9	97/10/30 SMI"	/* SVr4.0 1.2	*/
/*LINTLIBRARY*/

#include <stdlib.h>
#include <stdio.h>
#include "con.h"

float obotx = 0.0;
float oboty = 0.0;
float botx = 0.0;
float boty = 0.0;
float scalex = 1.0;
float scaley = 1.0;
int scaleflag = 0;
int oloy = -1;
int ohiy = -1;
int ohix = -1;
int oextra = -1;


void
putch(char c)
{
	(void) putc(c, stdout);
}

void
cont(short x, short y)
{
	short hix, hiy, lox, loy, extra;
	short n;
	x = (x - obotx) * scalex + botx;
	y = (y - oboty) * scaley + boty;
	hix = (x>>7) & 037;
	hiy = (y>>7) & 037;
	lox = (x>>2) & 037;
	loy = (y>>2) & 037;
	extra = x & 03 + (y<<2) & 014;
	n = (abs(hix - ohix) + abs(hiy - ohiy) + 6) / 12;
	if (hiy != ohiy) {
		putch(hiy|040);
		ohiy = hiy;
	}
	if (hix != ohix) {
		if (extra != oextra) {
			putch(extra|0140);
			oextra = extra;
		}
		putch(loy|0140);
		putch(hix|040);
		ohix = hix;
		oloy = loy;
	} else {
		if (extra != oextra) {
			putch(extra|0140);
			putch(loy|0140);
			oextra = extra;
			oloy = loy;
		} else if (loy != oloy) {
			putch(loy|0140);
			oloy = loy;
		}
	}
	putch(lox|0100);
	while (n--)
		putch(0);
}

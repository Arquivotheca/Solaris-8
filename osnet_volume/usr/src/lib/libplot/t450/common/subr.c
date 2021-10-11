/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)subr.c	1.8	97/10/30 SMI"	/* SVr4.0 1.3	*/
/*LINTLIBRARY*/

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <plot.h>
#include "con.h"

int xnow, ynow, OUTF;

int
xconv(int xp)
{
	/*
	 * x position input is -2047 to +2047,
	 * output must be 0 to PAGSIZ*HORZRES
	*/
	xp += 2048;
	/* the computation is newx = xp*(PAGSIZ*HORZRES)/4096 */
	return (xoffset + xp / xscale);
}

int
yconv(int yp)
{
	/* see description of xconv */
	yp += 2048;
	return (yp / yscale);
}

void
inplot(void)
{
	spew(ESC);
	spew(PLOTIN);
}

void
outplot(void)
{
	spew(ESC);
	spew(PLOTOUT);
	(void) fflush(stdout);
}

void
spew(char ch)
{
	if (ch == UP) {
		(void) putc(ESC, stdout);
		ch = DOWN;
	}
	(void) putc(ch, stdout);
}

void
tobotleft(void)
{
	move(-2048, -2048);
}

void
reset(void)
{
	(void) signal(SIGINT, SIG_IGN);
	outplot();
	(void) ioctl(OUTF, TCSETAW, &ITTY);
	_exit(0);
}

float
dist2(int x1, int y1, int x2, int y2)
{
	float t, v;
	t = x2 - x1;
	v = y1 - y2;
	return (t * t + v * v);
}

void
swap(int *pa, int *pb)
{
	int t;
	t = *pa;
	*pa = *pb;
	*pb = t;
}

void
movep(int xg, int yg)
{
	int i;
	char ch;
	if ((xg == xnow) && (yg == ynow))
		return;
	/* if we need to go to left margin, just CR */
	if (xg < xnow / 2) {
		spew(CR);
		xnow = 0;
	}
	i = (xg - xnow) / HORZRES;
	if (xnow < xg)
		ch = RIGHT;
	else ch = LEFT;
	xnow += i * HORZRES;
	i = abs(i);
	while (i--)
		spew(ch);
	i = abs(xg - xnow);
	inplot();
	while (i--)
		spew(ch);
	outplot();
	i = (yg - ynow) / VERTRES;
	if (ynow < yg)
		ch = UP;
	else ch = DOWN;
	ynow += i * VERTRES;
	i = abs(i);
	while (i--)
		spew(ch);
	i = abs(yg - ynow);
	inplot();
	while (i--)
		spew(ch);
	outplot();
	xnow = xg; ynow = yg;
}

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
	ya = (yi - oboty) *scaley +boty;
	return (ya);
}

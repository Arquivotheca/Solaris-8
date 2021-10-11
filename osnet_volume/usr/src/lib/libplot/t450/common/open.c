/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)open.c	1.9	97/10/29 SMI"	/* SVr4.0 1.3	*/
/*LINTLIBRARY*/

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include "con.h"


float botx = 0.0, boty = 0.0, obotx = 0.0, oboty = 0.0;
float scalex = 1.0, scaley = 1.0;
static float HEIGHT = 6.0, WIDTH = 6.0, OFFSET = 0.0;
int xscale = 0, xoffset = 0, yscale = 0;
struct termio ITTY, PTTY;

void
openpl(void)
{
	int xnow = 0, ynow = 0;
	int OUTF = 1;
	(void) printf("\r");
	(void) ioctl(OUTF, TCGETA, &ITTY);
	(void) signal(SIGINT, (void (*)(int))reset);
	PTTY = ITTY;
	PTTY.c_oflag &= ~(ONLCR|OCRNL|ONOCR|ONLRET);
	PTTY.c_cflag |= CSTOPB;
	(void) ioctl(OUTF, TCSETAW, &PTTY);
	/* set vert and horiz spacing to 6 and 10 */
	(void) putchar(ESC);	/* set vert spacing to 6 lpi */
	(void) putchar(RS);
	(void) putchar(HT);
	(void) putchar(ESC);	/* set horiz spacing to 10 cpi */
	(void) putchar(US);
	(void) putchar(CR);
	/* initialize constants */
	xscale  = 4096./(HORZRESP * WIDTH);
	yscale = 4096 /(VERTRESP * HEIGHT);
	xoffset = OFFSET * HORZRESP;
}

void
openvt(void)
{
	openpl();
}

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

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include "con.h"


static float HEIGHT = 6.0, WIDTH = 6.0, OFFSET = 0.0;
float botx = 0.0, boty = 0.0, obotx = 0.0, oboty = 0.0;
float scalex = 1.0, scaley = 1.0;
int OUTF, xscale, yscale, xoffset;
struct termio ITTY, PTTY;

void
openpl(void)
{
	xnow = ynow = 0;
	OUTF = 1;
	(void) printf("\r");
	(void) ioctl(OUTF, TCGETA, &ITTY);
	(void) signal(SIGINT, (void (*)(int))reset);
	PTTY = ITTY;
	PTTY.c_oflag &= ~(ONLCR|OCRNL|ONOCR|ONLRET);
	PTTY.c_cflag |= CSTOPB;
	(void) ioctl(OUTF, TCSETAW, &PTTY);
	/* initialize constants */
	xscale = 4096./(HORZRESP * WIDTH);
	yscale = 4096 /(VERTRESP * HEIGHT);
	xoffset = OFFSET * HORZRESP;
}

void
openvt(void)
{
	openpl();
}

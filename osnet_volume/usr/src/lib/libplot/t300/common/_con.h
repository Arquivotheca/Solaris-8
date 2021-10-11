/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)con.h	1.9	97/10/29 SMI"	/* SVr4.0 1.2	*/

#include <termio.h>

/* gsi plotting output routines */
#define	DOWN 012
#define	UP 013
#define	LEFT 010
#define	RIGHT 040
#define	BEL 007
#define	ESC 033
#define	ACK 006
#define	CR 015
#define	FF 014
#define	VERTRESP 48
#define	HORZRESP 60.
#define	VERTRES 8.
#define	HORZRES 6.
/*
 * down is line feed, up is reverse line feed,
 * left is backspace, right is space.  48 points per inch
 * vertically, 60 horizontally
*/

extern struct termio ITTY, PTTY;
extern float botx, boty, obotx, oboty, scalex, scaley;
extern int xscale, xoffset, yscale;
extern int OUTF;
extern void movep(short, short);
extern void spew(char);
extern void inplot(void);
extern void outplot(void);
extern void reset(void);
extern float dist2(int, int, int, int);
extern int xsc(short);
extern int ysc(short);
extern short xconv(short);
extern short yconv(short);
extern short xnow, ynow;

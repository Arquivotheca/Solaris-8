/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)label.c	1.7	97/10/29 SMI"	/* SVr4.0 1.2	*/
/*LINTLIBRARY*/

#include "con.h"

#define	N 0104
#define	E 0101
#define	NE 0105
#define	S 0110
#define	W 0102
#define	SW 0112

/*
 *	arrange by incremental plotting that an initial
 *	character such as +, X, *, etc will fall
 *	right on the point, and undo it so that further
 *	labels will fall properly in place
 */


void
label(char *s)
{
	char lbl_mv[] = {036, 040, S, S, S, S, S, S, SW, SW,
	SW, SW, SW, SW, SW, SW, SW, SW, 037, 0};

	char lbl_umv[] = {036, 040, N, N, N, N, N, N, NE, NE,
	NE, NE, NE, NE, NE, NE, NE, NE, 037, 0};

	int i;
	char c;

	/* LINTED */
	for (i = 0; c = lbl_mv[i]; i++)
		putch(c);
	/* LINTED */
	for (i = 0; c = s[i]; i++)
		putch(c);
	/* LINTED */
	for (i = 0; c = lbl_umv[i]; i++)
		putch(c);
}

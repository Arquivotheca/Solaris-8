/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)winchnstr.c	1.9	97/08/22 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
 * Read in ncols worth of data from window win and assign the
 * chars to string. NULL terminate string upon completion.
 * Return the number of chtypes copied.
 */

int
winchnstr(WINDOW *win, chtype *string, int ncols)
{
	chtype	*ptr = &(win->_y[win->_cury][win->_curx]);
	int	counter = 0;
	int	maxcols = win->_maxx - win->_curx;
	int	eucw, scrw, s;
	chtype	rawc, attr, wc;

	if (ncols < 0)
		ncols = MAXINT;

	while (ISCBIT(*ptr)) {
		ptr--;
		maxcols++;
	}

	while ((counter < ncols) && maxcols > 0) {
		eucw = mbeucw((int) RBYTE(*ptr));
		scrw = mbscrw((int) RBYTE(*ptr));

		if (counter + eucw > ncols)
			break;
		for (s = 0; s < scrw; s++, maxcols--, ptr++) {
			attr = _ATTR(*ptr);
			rawc = _CHAR(*ptr);
			if ((wc = RBYTE(rawc)) == MBIT)
				continue;
			*string++ = wc | attr;
			counter++;
			if ((wc = LBYTE(rawc) | MBIT) == MBIT)
				continue;
			*string++ = wc | attr;
			counter++;
		}
	}
	if (counter < ncols)
		*string = (chtype) 0;
	return (counter);
}

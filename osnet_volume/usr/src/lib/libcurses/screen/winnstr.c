/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)winnstr.c	1.9	97/08/22 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
 * Copy n chars in window win from current cursor position to end
 * of window into char buffer str.  Return the number of chars copied.
 */

int
winnstr(WINDOW *win, char *str, int ncols)
{
	int	counter = 0;
	int	cy = win->_cury;
	chtype	*ptr = &(win->_y[cy][win->_curx]),
		*pmax = &(win->_y[cy][win->_maxx]);
	chtype	wc;
	int	eucw, scrw, s;


	while (ISCBIT(*ptr))
		ptr--;

	if (ncols < -1)
		ncols = MAXINT;

	while (counter < ncols) {
		scrw = mbscrw((int) RBYTE(*ptr));
		eucw = mbeucw((int) RBYTE(*ptr));
		if (counter + eucw > ncols)
			break;

		for (s = 0; s < scrw; s++, ptr++) {
			if ((wc = RBYTE(*ptr)) == MBIT)
				continue;
			/* LINTED */
			*str++ = (char) wc;
			counter++;
			if ((wc = LBYTE(*ptr) | MBIT) == MBIT)
				continue;
			/* LINTED */
			*str++ = (char) wc;
			counter++;
		}

		if (ptr >= pmax) {
			if (++cy == win->_maxy)
				break;

			ptr = &(win->_y[cy][0]);
			pmax = ptr + win->_maxx;
		}
	}
	if (counter < ncols)
		*str = '\0';

	return (counter);
}

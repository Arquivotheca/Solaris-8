/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)winwchnstr.c 1.4 97/08/22 SMI"

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
 * Read in ncols worth of data from window win and assign the
 * chars to string. NULL terminate string upon completion.
 * Return the number of chtypes copied.
 */

int
winwchnstr(WINDOW *win, chtype *string, int ncols)
{
	chtype	*ptr = &(win->_y[win->_cury][win->_curx]);
	int	counter = 0;
	int	maxcols = win->_maxx - win->_curx;
	int	scrw, s, wc;
	char	*mp, mbbuf[CSMAX+1];
	wchar_t	wch;
	chtype	rawc;
	chtype	attr;

	if (ncols < 0)
		ncols = MAXINT;

	while (ISCBIT(*ptr)) {
		ptr--;
		maxcols++;
	}

	while ((counter < ncols) && maxcols > 0) {
		attr = *ptr & A_WATTRIBUTES;
		rawc = *ptr & A_WCHARTEXT;
		(void) mbeucw((int)RBYTE(rawc));
		scrw = mbscrw((int)RBYTE(rawc));
		for (mp = mbbuf, s = 0; s < scrw; s++, maxcols--, ptr++) {
			if ((wc = (int)RBYTE(rawc)) == MBIT)
				continue;
			/* LINTED */
			*mp++ = (char) wc;
			if ((wc = (int)(LBYTE(rawc) | MBIT)) == MBIT)
				continue;
			/* LINTED */
			*mp++ = (char) wc;
		}
		*mp = '\0';
		if (_curs_mbtowc(&wch, mbbuf, CSMAX) <= 0)
			break;
		*string++ = wch | attr;
		counter++;
	}
	if (counter < ncols)
		*string = (chtype) 0;
	return (counter);
}

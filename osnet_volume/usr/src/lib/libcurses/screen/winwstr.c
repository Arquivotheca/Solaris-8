/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)winwstr.c 1.4 97/08/22 SMI"

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

int
winwstr(WINDOW *win, wchar_t *wstr)
{
	int	counter = 0;
	int	cy = win->_cury;
	chtype	*ptr = &(win->_y[cy][win->_curx]),
		*pmax = &(win->_y[cy][win->_maxx]);
	chtype	*p1st = &(win->_y[cy][0]);
	wchar_t	wc;
	int	sw, s;
	char	*cp, cbuf[CSMAX+1];

	while (ISCBIT(*ptr) && (p1st < ptr))
		ptr--;

	while (ptr < pmax) {
		wc = RBYTE(*ptr);
		sw = mbscrw((int)wc);
		(void) mbeucw((int)wc);

		cp = cbuf;
		for (s = 0; s < sw; s++, ptr++) {
			if ((wc = RBYTE(*ptr)) == MBIT)
				continue;
			/* LINTED */
			*cp++ = (char) wc;
			if ((wc = LBYTE(*ptr) | MBIT) == MBIT)
				continue;
			/* LINTED */
			*cp++ = (char) wc;
		}
		*cp = '\0';

		if (_curs_mbtowc(&wc, cbuf, CSMAX) <= 0)
			break;

		*wstr++ = wc;
	}

	*wstr = (wchar_t)0;

	return (counter);
}

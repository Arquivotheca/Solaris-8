/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)waddwchnstr.c 1.4 97/08/14 SMI"

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
 * Add ncols worth of data to win, using string as input.
 * Return the number of chtypes copied.
 * Note: chtype contains 32/16 bit process code.
 */
int
waddwchnstr(WINDOW *win, chtype *string, int ncols)
{
	int		my_x = win->_curx;
	int		my_y = win->_cury;
	short		my_maxx;
	int		counter;
	chtype		*ptr = &(win->_y[my_y][my_x]);
	chtype		*sptr = ptr;
	char		mbbuf[CSMAX+1];
	int		mp, s, scrw;
	chtype		rawc;
	chtype		attr;
	short		my_x1 = win->_curx;


	while (ISCBIT(*ptr)) {
		ptr--;
		my_x1--;
	}
	while (ptr < sptr)
		*ptr++ = win->_bkgd;

	if (ncols == -1)
		ncols = MAXINT;

	counter = win->_maxx - my_x;
	while ((ncols > 0) && (*string) && (counter > 0)) {
		attr = *string & A_WATTRIBUTES;
		rawc = *string & A_WCHARTEXT;

		/* conver wchar_t to mbuti byte string */
		for (mp = 0; mp < sizeof (mbbuf); mp++)
			mbbuf[mp] = '\0';
		if (_curs_wctomb(mbbuf, rawc) <= 0)
			goto out;

		/* if there are no cols on screen, end */
		if ((scrw = wcscrw(rawc)) > counter)
			goto out;

		if (rawc & WCHAR_CSMASK) {
			/* store multi-byte string into chtype */
			for (s = 0, mp = 0; s < scrw; s++, mp += 2) {
				*ptr = _CHAR(RBYTE(mbbuf[mp]) |
				    RBYTE(mbbuf[mp + 1]) << 8) | CBIT;
				SETMBIT(*ptr);
				if (mp > 0)
					SETCBIT(*ptr);
				else
					CLRCBIT(*ptr);
				*ptr |= attr;
				ptr++;
			}
		} else {
		/* store single-byte string into chtype */
			*ptr = mbbuf[0];
			*ptr |= attr;
			ptr++;
		}

		ncols--;
		string++;
		counter -= scrw;
	}
out :

	while (ISCBIT(*ptr))
		*ptr++ = win->_bkgd;

	/* LINTED */
	my_maxx = (short) (ptr - sptr + my_x);

	if (my_x1 < win->_firstch[my_y])
		win->_firstch[my_y] = my_x1;

	if (my_maxx > win->_lastch[my_y])
		win->_lastch[my_y] = my_maxx;

	win->_flags |= _WINCHANGED;

	/* sync with ancestor structures */
	if (win->_sync)
		wsyncup(win);

	return (win->_immed ? wrefresh(win) : OK);
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)wclrtoeol.c	1.10	97/08/14 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	<stdlib.h>
#include	"curses_inc.h"

/* This routine clears up to the end of line. */

int
wclrtoeol(WINDOW *win)
{
	int	y = win->_cury;
	int	x = win->_curx;
	int	maxx = win->_maxx;
	int			cx;
	chtype		wc;

	if (win != curscr) {
		win->_nbyte = -1;
		if (_scrmax > 1) {
			if (ISMBIT(win->_y[y][x])) {
				win->_insmode = TRUE;
				if (_mbvalid(win) == ERR)
					return (ERR);
				x = win->_curx;
			}
			if (ISMBIT(win->_y[y][maxx - 1])) {
				for (cx = maxx - 1; cx >= x; --cx)
					if (!ISCBIT(win->_y[y][cx]))
						break;
				wc = RBYTE(win->_y[y][cx]);
				if (cx + _curs_scrwidth[TYPE(wc)] > maxx)
					maxx = cx - 1;
			}
		}
	}

	memSset(&win->_y[y][x], win->_bkgd, maxx - x);
	maxx = win->_maxx;

#ifdef	_VR3_COMPAT_CODE
	if (_y16update)
		(*_y16update)(win, 1, maxx - x, y, x);
#endif	/* _VR3_COMPAT_CODE */

	/* if curscr, reset blank structure */
	if (win == curscr) {
		if (_BEGNS[y] >= x)
			/* LINTED */
			_BEGNS[y] = (short) maxx;
		if (_ENDNS[y] >= x)
			_ENDNS[y] = _BEGNS[y] > x ? -1 : x-1;

		_CURHASH[y] = x == 0 ? 0 : _NOHASH;

		if (_MARKS != NULL) {
			char	*mkp = _MARKS[y];
			int	endx = COLS /
				BITSPERBYTE + (COLS  %BITSPERBYTE ? 1 : 0);
			int	m = x / BITSPERBYTE + 1;

			for (; m < endx; ++m)
				mkp[m] = 0;
			mkp += x / BITSPERBYTE;
			if ((m = x % BITSPERBYTE) == 0)
				*mkp = 0;
			else
				for (; m < BITSPERBYTE; ++m)
					*mkp &= ~(1 << m);

			/* if color terminal, do the same for color marks */

			if (_COLOR_MARKS != NULL) {
				mkp = _COLOR_MARKS[y];

				m = x / BITSPERBYTE + 1;
				for (; m < endx; ++m)
					mkp[m] = 0;
				mkp += x / BITSPERBYTE;
				if ((m = x % BITSPERBYTE) == 0)
					*mkp = 0;
				else
					for (; m < BITSPERBYTE; ++m)
						*mkp &= ~(1 << m);
			}
		}
		return (OK);
	} else {
		/* update firstch and lastch for the line. */
#ifdef	DEBUG
	if (outf)
		fprintf(outf, "CLRTOEOL: line %d begx = %d, maxx = %d, "
		    "lastch = %d, next firstch %d\n", y, win->_begx,
		    win->_firstch[y], win->_lastch[y], win->_firstch[y+1]);
#endif	/* DEBUG */

		if (win->_firstch[y] > x)
		    /* LINTED */
		    win->_firstch[y] = (short) x;
		win->_lastch[y] = maxx - 1;
		win->_flags |= _WINCHANGED;
		/* sync with ancestors structures */
		if (win->_sync)
			wsyncup(win);

		return (win->_immed ? wrefresh(win) : OK);
	}
}

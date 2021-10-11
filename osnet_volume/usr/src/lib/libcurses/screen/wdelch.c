/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)wdelch.c	1.8	97/06/25 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
 * This routine performs a delete-char on the line,
 * leaving (_cury, _curx) unchanged.
 */

int
wdelch(WINDOW *win)
{
	chtype	*temp1, *temp2;
	chtype	*end;
	int	cury = win->_cury;
	short	curx = win->_curx;
	chtype	*cp;
	int	s;

	end = &win->_y[cury][win->_maxx - 1];
	temp2 = &win->_y[cury][curx + 1];
	temp1 = temp2 - 1;

	s = 1;
	win->_nbyte = -1;
	if (_scrmax > 1) {
		if (ISMBIT(*temp1)) {
			win->_insmode = TRUE;
			if (_mbvalid(win) == ERR)
				return (ERR);
			curx = win->_curx;
			temp1 = &win->_y[cury][curx];
		}
		if (ISMBIT(*end)) {
			for (cp = end; cp >= temp1; --cp)
				if (!ISCBIT(*cp))
					break;
			if (cp + _curs_scrwidth[TYPE(*cp)] > end+1)
				end = cp - 1;
		}
		if (ISMBIT(*temp1))
			s = _curs_scrwidth[TYPE(RBYTE(*temp1))];
		end -= s - 1;
		temp2 = &win->_y[cury][curx+s];
	}

	while (temp1 < end)
		*temp1++ = *temp2++;

	while (s--)
		*temp1++ = win->_bkgd;

#ifdef	_VR3_COMPAT_CODE
	if (_y16update)
		(*_y16update)(win, 1, win->_maxx - curx, cury, curx);
#endif	/* _VR3_COMPAT_CODE */

	win->_lastch[cury] = win->_maxx - 1;
	if (win->_firstch[cury] > curx)
		win->_firstch[cury] = curx;

	win->_flags |= _WINCHANGED;

	if (win->_sync)
		wsyncup(win);

	return (win->_immed ? wrefresh(win) : OK);
}

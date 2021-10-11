/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)wvline.c	1.7	97/06/25 SMI"	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

int
wvline(WINDOW *win, chtype vertch, int num_chars)
{
	short	cury = win->_cury, curx = win->_curx;
	chtype  a, **fp = win->_y;
	short   *firstch = &(win->_firstch[cury]),
		*lastch = &(win->_lastch[cury]);

	if (num_chars <= 0)
		return (ERR);

	if (num_chars > win->_maxy - cury + 1)
		num_chars = win->_maxy - cury + 1;
	if (vertch == 0)
		vertch = ACS_VLINE;
	a = _ATTR(vertch);
	vertch = _WCHAR(win, vertch) | a;
	for (num_chars += cury; cury < num_chars; cury++, firstch++,
	    lastch++) {
		fp[cury][curx] = vertch;
		if (curx < *firstch)
			*firstch = curx;
		if (curx > *lastch)
			*lastch = curx;
	}
	win->_flags |= _WINCHANGED;

	if (win->_sync)
		wsyncup(win);

	return (win->_immed ? wrefresh(win) : OK);
}

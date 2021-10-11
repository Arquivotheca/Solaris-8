/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#pragma ident  "@(#)wmovenextch.c 1.3 97/06/25 SMI"

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

int
wmovenextch(WINDOW *win)
/*
 * wmovenextch --- moves the cursor forward to the next char of char
 * cursor is currently on.  This is used to move forward over a multi-column
 * character.  When the cursor is on a character at the right-most
 * column, the cursor will stay there.
 */
{
	chtype *_yy;
	short	x;

	_yy = win->_y[win->_cury];
	x = win->_curx;

	if (x + 1 > win->_maxx) /* Can't move any more. */
		return (ERR);

	++x;
	for (; ; ) {
		if (x >= win->_maxx) /* No more space.. */
			return (ERR);
		if (ISCBIT(_yy[x])) {
			++x;
		} else {
			break;
		}
	}

	win->_curx = x;
	win->_flags |= _WINMOVED;
	return (win->_immed ? wrefresh(win): OK);
}

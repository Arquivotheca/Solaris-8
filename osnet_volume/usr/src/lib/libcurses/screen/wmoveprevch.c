/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#pragma ident  "@(#)wmoveprevch.c 1.3 97/06/25 SMI"

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

int
wmoveprevch(WINDOW *win)
/*
 * wmoveprevch --- moves the cursor back to the previous char of char
 * cursor is currently on.  This is used to move back over a multi-column
 * character.  When the cursor is on a character at the left-most
 * column, the cursor will stay there.
 */
{
	chtype	*_yy;
	short	x;

	(void) wadjcurspos(win);
	x = win->_curx;
	if (x == 0) /* Can't back up any more. */
		return (ERR);
	_yy = win->_y[win->_cury];
	--x;
	while ((x > 0) && (ISCBIT(_yy[x])))
		--x;
	win->_curx = x;
	win->_flags |= _WINMOVED;
	return (win->_immed ? wrefresh(win): OK);
}

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#pragma ident  "@(#)wadjcurspos.c 1.3 97/06/25 SMI"

#include	<sys/types.h>
#include	"curses_inc.h"

int
wadjcurspos(WINDOW *win)
	/*
	 * wmadjcurspos --- moves the cursor to the first column within the
	 * multi-column character somewhere on which the cursor curently is on.
	*/
{
	short	x;
	chtype	*_yy;

	x = win->_curx;
	_yy = win->_y[win->_cury];
	while ((x > 0) && (ISCBIT(_yy[x]))) --x;
	if (win->_curx != x) {
		win->_curx = x;
		return (win->_immed ? wrefresh(win): OK);
	} else {
		return (OK);
	}
}

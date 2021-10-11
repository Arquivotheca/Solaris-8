/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)wclrtobot.c	1.7	97/06/25 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/* This routine erases everything on the window. */
int
wclrtobot(WINDOW *win)
{
	bool	savimmed, savsync;
	int	cury = win->_cury;
	short	curx = win->_curx;

	if (win != curscr) {
		savimmed = win->_immed;
		savsync = win->_sync;
		win->_immed = win->_sync = FALSE;
	}

	/* set region to be clear */
	if (cury >= win->_tmarg && cury <= win->_bmarg)
		win->_cury = win->_bmarg;
	else
		win->_cury = win->_maxy - 1;

	win->_curx = 0;
	for (; win->_cury > cury; win->_cury--)
		(void) wclrtoeol(win);
	win->_curx = curx;
	(void) wclrtoeol(win);

	if (win == curscr)
		return (OK);

	/* not curscr */
	win->_sync = savsync;

	if (win->_sync)
		wsyncup(win);

	win->_flags |= _WINCHANGED;
	return ((win->_immed = savimmed) ? wrefresh(win) : OK);
}

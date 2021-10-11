/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)wscrl.c	1.7	97/06/25 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/* Scroll the given window up/down n lines. */
int
wscrl(WINDOW *win, int n)
{
	short	curx, cury;
	bool	 savimmed, savsync;

#ifdef	DEBUG
	if (outf)
		if (win == stdscr)
			fprintf(outf, "scroll(stdscr, %d)\n", n);
		else
			if (win == curscr)
				fprintf(outf, "scroll(curscr, %d)\n", n);
			else
				fprintf(outf, "scroll(%x, %d)\n", win, n);
#endif	/* DEBUG */
	if (!win->_scroll || (win->_flags & _ISPAD))
		return (ERR);

	savimmed = win->_immed;
	savsync = win->_sync;
	win->_immed = win->_sync = FALSE;

	curx = win->_curx; cury = win->_cury;

	if (cury >= win->_tmarg && cury <= win->_bmarg)
		win->_cury = win->_tmarg;
	else
		win->_cury = 0;

	(void) winsdelln(win, -n);
	win->_curx = curx;
	win->_cury = cury;

	win->_sync = savsync;

	if (win->_sync)
		wsyncup(win);

	return ((win->_immed = savimmed) ? wrefresh(win) : OK);
}

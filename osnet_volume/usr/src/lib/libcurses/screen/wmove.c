/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)wmove.c	1.9	97/08/14 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/* This routine moves the cursor to the given point */

int
wmove(WINDOW *win, int y, int x)
{
#ifdef	DEBUG
	if (outf) {
		fprintf(outf, "MOVE to win ");
		if (win == stdscr)
			fprintf(outf, "stdscr ");
		else
			fprintf(outf, "%o ", win);
		fprintf(outf, "(%d, %d)\n", y, x);
	}
#endif	/* DEBUG */
	if (x < 0 || y < 0 || x >= win->_maxx || y >= win->_maxy)
		return (ERR);

	if (y != win->_cury || x != win->_curx)
		win->_nbyte = -1;

	/* LINTED */
	win->_curx = (short) x;
	/* LINTED */
	win->_cury = (short) y;
	win->_flags |= _WINMOVED;
	return (win->_immed ? wrefresh(win) : OK);
}

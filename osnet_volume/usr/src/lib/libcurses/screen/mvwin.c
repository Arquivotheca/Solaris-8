/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)mvwin.c	1.8	97/08/22 SMI"	/* SVr4.0 1.9	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/* relocate the starting position of a _window */

int
mvwin(WINDOW *win, int by, int bx)
{
	if ((by + win->_maxy) > LINES || (bx + win->_maxx) > COLS ||
	    by < 0 || bx < 0)
		return (ERR);
	/*LINTED*/
	win->_begy = (short) by;
	/*LINTED*/
	win->_begx = (short) bx;
	(void) wtouchln(win, 0, win->_maxy, -1);
	return (win->_immed ? wrefresh(win) : OK);
}

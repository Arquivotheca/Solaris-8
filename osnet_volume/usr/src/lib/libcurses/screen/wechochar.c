/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)wechochar.c	1.7	97/06/25 SMI"	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
 *  These routines short-circuit much of the innards of curses in order to get
 *  a single character output to the screen quickly! It is used by getch()
 *  and getstr().
 *
 *  wechochar(WINDOW *win, chtype ch) is functionally equivalent to
 *  waddch(WINDOW *win, chtype ch), wrefresh(WINDOW *win)
 */

int
wechochar(WINDOW *win, chtype ch)
{
	bool	saveimm = win->_immed;
	int	 rv;

	immedok(win, TRUE);
	rv = waddch(win, ch);
	win->_immed = saveimm;
	return (rv);
}

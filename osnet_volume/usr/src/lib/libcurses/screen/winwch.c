/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)winwch.c 1.3 97/06/25 SMI"

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
**	Get a process code at(curx, cury).
*/
chtype
winwch(WINDOW *win)
{
	wchar_t	wchar;
	chtype	a;

	a = (win->_y[win->_cury][win->_curx]) & A_WATTRIBUTES;

	(void) _curs_mbtowc(&wchar, wmbinch(win, win->_cury, win->_curx),
	    sizeof (wchar_t));
	return (a | wchar);
}

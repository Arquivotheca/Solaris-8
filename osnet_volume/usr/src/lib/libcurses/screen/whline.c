/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)whline.c	1.7	97/06/25 SMI"	/* SVr4.0 1.5	*/

#include	<sys/types.h>
#include	<stdlib.h>
#include	"curses_inc.h"

int
whline(WINDOW *win, chtype horch, int num_chars)
{
	short	cury = win->_cury, curx = win->_curx;
	chtype  a, *fp = &(win->_y[cury][curx]);

	if (num_chars <= 0)
		return (ERR);

	if (num_chars > win->_maxx - curx + 1)
		num_chars = win->_maxx - curx + 1;
	if (horch == 0)
		horch = ACS_HLINE;
	a = _ATTR(horch);
	horch = _WCHAR(win, horch) | a;
	memSset(fp, horch | win->_attrs, num_chars);
	if (curx < win->_firstch[cury])
		win->_firstch[cury] = curx;
	if ((curx += (num_chars - 1)) > win->_lastch[cury])
		win->_lastch[cury] = curx;
	win->_flags |= _WINCHANGED;

	if (win->_sync)
		wsyncup(win);

	return (win->_immed ? wrefresh(win) : OK);
}

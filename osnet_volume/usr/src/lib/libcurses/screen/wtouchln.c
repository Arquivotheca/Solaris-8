/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)wtouchln.c	1.7	97/06/25 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
 * Make a number of lines look like they have/have not been changed.
 * y: the start line
 * n: the number of lines affected
 * changed:	1: changed
 * 		0: not changed
 * 		-1: changed. Called internally - In this mode
 *		even REDRAW lines are changed.
 */

int
wtouchln(WINDOW *win, int y, int n, int changed)
{
	short	*firstch, *lastch, b, e;
	int	maxy = win->_maxy;

	if (y >= maxy)
		return (ERR);
	if (y < 0)
		y = 0;
	if ((y + n) > maxy)
		n = maxy - y;
	firstch = win->_firstch + y;
	lastch = win->_lastch + y;
	if (changed) {
		win->_flags |= _WINCHANGED;
		b = 0;
		e = win->_maxx - 1;
	} else {
		b = _INFINITY;
		e = -1;
		win->_flags &= ~_WINCHANGED;
	}

	for (; n-- > 0; firstch++, lastch++) {
		if (changed == -1 || *firstch != _REDRAW)
			*firstch = b, *lastch = e;
	}

	if ((changed == 1) && win->_sync)
		wsyncup(win);

	return (((changed == 1) && win->_immed) ? wrefresh(win) : OK);
}

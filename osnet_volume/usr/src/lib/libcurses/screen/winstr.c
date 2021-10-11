/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)winstr.c	1.9	97/08/22 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

int
winstr(WINDOW *win, char *str)
{
	int	counter = 0;
	int	cy = win->_cury;
	chtype	*ptr = &(win->_y[cy][win->_curx]),
		*pmax = &(win->_y[cy][win->_maxx]);
	chtype	*p1st = &(win->_y[cy][0]);
	chtype	wc;
	int	sw, s;

	while (ISCBIT(*ptr) && (p1st < ptr))
		ptr--;

	while (ptr < pmax) {
		wc = RBYTE(*ptr);
		sw = mbscrw((int) wc);
		(void) mbeucw((int) wc);
		for (s = 0; s < sw; s++, ptr++) {
			if ((wc = RBYTE(*ptr)) == MBIT)
				continue;
			/* LINTED */
			str[counter++] = (char) wc;
			if ((wc = LBYTE(*ptr) | MBIT) == MBIT)
				continue;
			/* LINTED */
			str[counter++] = (char) wc;
		}
	}
	str[counter] = '\0';

	return (counter);
}

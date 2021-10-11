/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)menucursor.c	1.4	97/07/09 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

/* Position the cursor in the user's subwindow. */

void
_position_cursor(MENU *m)
{
	int y, x;
	WINDOW *us, *uw;

	if (Posted(m)) {
		/* x and y represent the position in our subwindow */
		y = Y(Current(m)) - Top(m);
		x = X(Current(m))*(Itemlen(m)+1);

		if (ShowMatch(m)) {
			if (Pindex(m)) {
				x += Pindex(m) + Marklen(m) - 1;
			}
		}

		uw = UW(m);
		us = US(m);
		(void) wmove(us, y, x);

		if (us != uw) {
			wcursyncup(us);
			wsyncup(us);
			/*
			 * The next statement gets around some aberrant
			 * behavior in curses. The subwindow is never being
			 * untouched and this results in the parent window
			 * being touched every time a syncup is done.
			 */
			(void) untouchwin(us);
		}
	}
}

int
pos_menu_cursor(MENU *m)
{
	if (!m) {
		return (E_BAD_ARGUMENT);
	}
	if (!Posted(m)) {
		return (E_NOT_POSTED);
	}
	_position_cursor(m);
	return (E_OK);
}

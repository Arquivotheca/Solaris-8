/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)setsyx.c	1.8	97/08/15 SMI"	/* SVr4.0 1.8	*/

/*LINTLIBRARY*/

/*
 * Set the current screen coordinates (y, x).
 *
 * This routine may be called before doupdate(). It tells doupdate()
 * where to leave the cursor instead of the location of (x, y) of the
 * last window that was wnoutrefreshed or pnoutrefreshed.
 * If x and y are negative, then the cursor will be left wherever
 * curses decides to leave it, as if leaveok() had been TRUE for the
 * last window refreshed.
 */

#include	<sys/types.h>
#include	"curses_inc.h"

int
setsyx(int y, int x)
{
	if (y < 0 && x < 0) {
		SP->virt_scr->_leave = TRUE;
	} else {
		_virtscr->_cury = y + SP->Yabove;
		/* LINTED */
		_virtscr->_curx = (short) x;
		_virtscr->_leave = FALSE;
		_virtscr->_flags |= _WINMOVED;
	}
	return (OK);
}

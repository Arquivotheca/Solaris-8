/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

/* A panels subsystem built on curses--Move a panel */

#pragma ident	"@(#)move.c	1.5	97/09/17 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <curses.h>
#include "private.h"

/*  move_panel    */
int
move_panel(PANEL *panel, int starty, int startx)
{
	if (!panel)
		return (ERR);

	/* Check for hidden panels and move the window */

	if (panel == panel -> below) {
		if (mvwin(panel -> win, starty, startx) == ERR)
			return (ERR);
	} else {

		/*
		 * allocate nodes for overlap of new panel and move
		 * the curses window, removing it from the old location.
		 */

		if (!_alloc_overlap(_Panel_cnt - 1) ||
		    mvwin(panel -> win, starty, startx) == ERR)
			return (ERR);

		_remove_overlap(panel);
	}

	/* Make sure we know where the window is */

	getbegyx(panel -> win, panel -> wstarty, panel -> wstartx);
	getmaxyx(panel -> win, panel -> wendy, panel -> wendx);
	panel -> wendy += panel -> wstarty - 1;
	panel -> wendx += panel -> wstartx - 1;

	/* Determine which panels the new panel obscures (if not hidden) */

	if (panel != panel -> below)
		_intersect_panel(panel);
	return (OK);
}

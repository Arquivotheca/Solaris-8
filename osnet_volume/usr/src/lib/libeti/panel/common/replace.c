/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

/* A panels subsystem built on curses--Replace the window in a panel */

#pragma ident	"@(#)replace.c	1.5	97/09/17 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <curses.h>
#include "private.h"

/*  replace_panel    */
int
replace_panel(PANEL *panel, WINDOW *window)
{
	if (!panel || !window)
		return (ERR);

	/* pre-allocate the overlap nodes if the panel is not hidden */

	if (panel != panel -> below) {
		if (!_alloc_overlap(_Panel_cnt - 1))
			return (ERR);

		/* Remove the window from the old location. */

		_remove_overlap(panel);
	}

	/* Find the size of the new window */

	getbegyx(window, panel -> wstarty, panel -> wstartx);
	getmaxyx(window, panel -> wendy, panel -> wendx);
	panel -> win = window;
	panel -> wendy += panel -> wstarty - 1;
	panel -> wendx += panel -> wstartx - 1;

	/* Determine which panels the new panel obscures (if not hidden) */

	if (panel != panel -> below)
		_intersect_panel(panel);
	(void) touchwin(window);
	return (OK);
}

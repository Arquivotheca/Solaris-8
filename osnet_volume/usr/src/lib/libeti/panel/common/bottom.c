/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

/* A panels subsystem built on curses--Move a panel to the bottom */

#pragma ident	"@(#)bottom.c	1.5	97/09/17 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <curses.h>
#include "private.h"

/*  bottom_panel    */
int
bottom_panel(PANEL *panel)
{
	PANEL	*pnl;
	_obscured_list	*obs;

	if (!panel || panel == panel -> below)
		return (ERR);

	/* If the panel is already on bottom, there is nothing to do */

	if (_Bottom_panel == panel)
		return (OK);

	/*
	 * All the panels that this panel used to obscure now
	 * obscure this panel.
	 */

	for (pnl = panel->below; pnl; pnl = pnl->below) {
		if (obs = _unlink_obs(pnl, panel)) {
			obs -> panel_p = pnl;
			if (panel -> obscured) {
				obs -> next = panel -> obscured -> next;
				panel->obscured = panel->obscured->next = obs;
			}
			else
				obs -> next = panel -> obscured = obs;
		}
	}

	/* Move the panel to the bottom */

	if (panel == _Top_panel)
		(_Top_panel = panel -> below) -> above = 0;
	else {
		panel -> above -> below = panel -> below;
		panel -> below -> above = panel -> above;
	}

	panel -> below = 0;
	panel -> above = _Bottom_panel;
	_Bottom_panel = _Bottom_panel -> below = panel;
	(void) touchwin(panel -> win);

	return (OK);
}

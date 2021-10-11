/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

/* A panels subsystem built on curses--Move a panel to the top */

#pragma ident	"@(#)top.c	1.5	97/09/17 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <curses.h>
#include "private.h"

/*  top_panel    */
int
top_panel(PANEL *panel)
{
	_obscured_list	*obs;
	_obscured_list	*prev_obs, *tmp;

	if (!panel || panel == panel -> below)
		return (ERR);

	/* If the panel is already on top, there is nothing to do */

	if (_Top_panel == panel)
		return (OK);

	/*
	 * All the panels that used to obscure this panel are
	 * now obscured by this panel.
	 */

	if ((obs = panel -> obscured) != 0) {
		do {
			prev_obs = obs;
			obs = obs -> next;
			if ((tmp = prev_obs -> panel_p -> obscured) != 0) {
				prev_obs->next = tmp->next;
				tmp->next = prev_obs;
			} else
				prev_obs->next =
				    prev_obs->panel_p->obscured = prev_obs;
			prev_obs -> panel_p = panel;
		}
		while (obs != panel -> obscured);
		panel -> obscured = 0;
	}

	/* Move the panel to the top */

	if (panel == _Bottom_panel)
		(_Bottom_panel = panel -> above) -> below = 0;
	else {
		panel -> above -> below = panel -> below;
		panel -> below -> above = panel -> above;
	}

	panel -> above = 0;
	panel -> below = _Top_panel;
	_Top_panel = _Top_panel -> above = panel;
	(void) touchwin(panel -> win);

	return (OK);
}

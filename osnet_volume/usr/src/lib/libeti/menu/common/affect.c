/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)affect.c	1.4	97/07/09 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

/*
 * This routine checks the supplied values against the values of
 * current and top items.  If a value has changed then one of
 * terminate routines is called.  Following the actual change of
 * the value is the calling of the initialize routines.
 */

void
_affect_change(MENU *m, int newtop, ITEM *newcurrent)
{
	ITEM *oldcur;
	int topchange = FALSE, curchange = FALSE;

	/* Call term and init routines if posted */
	if (Posted(m)) {

		/* If current has changed terminate the old item. */
		if (newcurrent != Current(m)) {
			Iterm(m);
			curchange = TRUE;
		}

		/* If top has changed then call menu init function */
		if (newtop != Top(m)) {
			Mterm(m);
			topchange = TRUE;
		}

		oldcur = Current(m);
		Top(m) = newtop;
		Current(m) = newcurrent;

		if (topchange) {
			/* Init the new page if top has changed */
			Minit(m);
		}

		if (curchange) {
			/* Unmark the old item and mark  the new one */
			_movecurrent(m, oldcur);
			/* Init the new item if current changed */
			Iinit(m);
		}

		/* If anything changed go change user's copy of menu */
		if (topchange || curchange) {
			_show(m);
		} else {
			_position_cursor(m);
		}

	} else {
		/* Just change Top and Current if not posted */
		Top(m) = newtop;
		Current(m) = newcurrent;
	}
}

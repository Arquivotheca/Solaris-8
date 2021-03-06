/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)topitem.c	1.4	97/07/09 SMI"	/* SVr4.0 1.9	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

int
set_top_row(MENU *m, int top)
{
	ITEM *current;

	if (m) {
		if (Indriver(m)) {
			return (E_BAD_STATE);
		}
		if (!Items(m)) {
			return (E_NOT_CONNECTED);
		}
		if (top < 0 || top > Rows(m) - Height(m)) {
			return (E_BAD_ARGUMENT);
		}
		if (top != Top(m)) {
			/* Get linking information if not already there */
			if (LinkNeeded(m)) {
				_link_items(m);
			}
			/* Set current to toprow */
			current = IthItem(m, RowMajor(m) ? top * Cols(m) : top);
			Pindex(m) = 0;		/* Clear the pattern buffer */
			IthPattern(m, Pindex(m)) = '\0';
			_affect_change(m, top, current);
		}
	} else {
		return (E_BAD_ARGUMENT);
	}
	return (E_OK);
}

int
top_row(MENU *m)
{
	if (m && Items(m) && IthItem(m, 0)) {
		return (Top(m));
	} else {
		return (-1);
	}
}

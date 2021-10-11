/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)scale.c	1.4	97/07/09 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

/* Calculate the numbers of rows and columns needed to display the menu */

void
_scale(MENU *m)
{
	int width;

	if (Items(m) && IthItem(m, 0)) {
		/* Get the width of one column */
		width = MaxName(m) + Marklen(m);

		if (ShowDesc(m) && MaxDesc(m)) {
			width += MaxDesc(m) + 1;
		}
		Itemlen(m) = width;

		/* Multiply this by the number of columns */
		width = width * Cols(m);
		/* Add in the number of spaces between columns */
		width += Cols(m) - 1;
		Width(m) = width;
	}
}

int
scale_menu(MENU *m, int *r, int *c)
{
	if (!m) {
		return (E_BAD_ARGUMENT);
	}
	if (Items(m) && IthItem(m, 0)) {
		*r = Height(m);
		*c = Width(m);
		return (E_OK);
	}
	return (E_NOT_CONNECTED);
}

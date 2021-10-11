/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)visible.c	1.4	97/07/09 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

/* Check to see if an item is being displayed on the current page */

int
item_visible(ITEM *i)
{
	int bottom;
	MENU *m;

	if (!i || !Imenu(i)) {
		return (FALSE);
	}
	m = Imenu(i);
	if (Posted(m)) {
		bottom = Top(m) + Height(m) - 1;
		if (Y(i) >= Top(m) && Y(i) <= bottom) {
			return (TRUE);
		}
	}
	return (FALSE);
}

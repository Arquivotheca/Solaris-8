/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)menuitems.c	1.5	97/09/17 SMI"	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

int
set_menu_items(MENU *m, ITEM **i)
{
	if (!m) {
		return (E_BAD_ARGUMENT);
	}
	if (i && *i == (ITEM *) NULL) {
		return (E_BAD_ARGUMENT);
	}
	if (Posted(m)) {
		return (E_POSTED);
	}

	if (Items(m)) {
		_disconnect(m);
	}
	if (i) {
		/* Go test the item and make sure its not already connected */
		/* to another menu and then connect it to this one. */
		if (!_connect(m, i)) {
			return (E_CONNECTED);
		}
	} else {
		Items(m) = i;
	}
	return (E_OK);
}

ITEM **
menu_items(MENU *m)
{
	if (!m) {
		return ((ITEM **) NULL);
	}
	return (Items(m));
}

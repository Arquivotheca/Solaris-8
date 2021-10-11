/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)newmenu.c	1.4	97/07/09 SMI"	/* SVr4.0 1.7	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <stdlib.h>
#include "private.h"

MENU *
new_menu(ITEM **items)
{
	MENU *m;

	if ((m = (MENU *) calloc(1, sizeof (MENU))) != (MENU *)0) {
		*m = *Dfl_Menu;
		Rows(m) = FRows(m);
		Cols(m) = FCols(m);
		if (items) {
			if (*items == (ITEM *)0 || !_connect(m, items)) {
				free(m);
				return ((MENU *)0);
			}
		}
		return (m);
	}
	return ((MENU *)0);
}

int
free_menu(MENU *m)
{
	if (!m) {
		return (E_BAD_ARGUMENT);
	}
	if (Posted(m)) {
		return (E_POSTED);
	}
	if (Items(m)) {
		_disconnect(m);
	}
	free(m);
	return (E_OK);
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)menuback.c	1.4	97/07/09 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

int
set_menu_back(MENU *m, chtype attr)
{
	/*LINTED [E_CONST_PROMOTED_UNSIGNED_LONG]*/
	if (InvalidAttr(attr)) {
		return (E_BAD_ARGUMENT);
	}
	if (m) {
		Back(m) = attr;
		if (Posted(m)) {
			_draw(m);		/* Redraw menu */
			_show(m);		/* Redisplay menu */
		}
	} else {
		Back(Dfl_Menu) = attr;
	}
	return (E_OK);
}

chtype
menu_back(MENU *m)
{
	return (Back(m ? m : Dfl_Menu));
}

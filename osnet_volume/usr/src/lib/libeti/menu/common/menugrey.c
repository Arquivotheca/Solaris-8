/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)menugrey.c	1.4	97/07/09 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

int
set_menu_grey(MENU *m, chtype attr)
{
	/*LINTED [E_CONST_PROMOTED_UNSIGNED_LONG]*/
	if (InvalidAttr(attr)) {
		return (E_BAD_ARGUMENT);
	}
	if (m) {
		Grey(m) = attr;
		if (Posted(m)) {
			_draw(m);		/* Redraw the menu */
			_show(m);		/* Redisplay menu */
		}
	} else {
		Grey(Dfl_Menu) = attr;
	}
	return (E_OK);
}

chtype
menu_grey(MENU *m)
{
	return (Grey(m ? m : Dfl_Menu));
}

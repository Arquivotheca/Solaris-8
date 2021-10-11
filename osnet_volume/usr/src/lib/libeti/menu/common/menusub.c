/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)menusub.c	1.4	97/07/09 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

int
set_menu_sub(MENU *m, WINDOW *sub)
{
	if (m) {
		if (Posted(m)) {
			return (E_POSTED);
		}
		UserSub(m) = sub;
		/* Since window size has changed go recalculate sizes */
		_scale(m);
	} else {
		UserSub(Dfl_Menu) = sub;
	}
	return (E_OK);
}

WINDOW *
menu_sub(MENU *m)
{
	return (UserSub((m) ? m : Dfl_Menu));
}

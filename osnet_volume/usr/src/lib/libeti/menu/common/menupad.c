/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)menupad.c	1.4	97/07/09 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <ctype.h>
#include "private.h"

int
set_menu_pad(MENU *m, int pad)
{
	if (!isprint(pad)) {
		return (E_BAD_ARGUMENT);
	}
	if (m) {
		Pad(m) = pad;
		if (Posted(m)) {
			_draw(m);		/* Redraw menu */
			_show(m);		/* Display menu */
		}
	} else {
		Pad(Dfl_Menu) = pad;
	}
	return (E_OK);
}

int
menu_pad(MENU *m)
{
	return (Pad(m ? m : Dfl_Menu));
}

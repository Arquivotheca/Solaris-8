/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)menumark.c	1.4	97/07/09 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <strings.h>
#include "private.h"

int
set_menu_mark(MENU *m, char *mark)
{
	int len;	/* Length of mark */

	if (mark && *mark) {
		/*LINTED [E_ASSIGN_INT_TO_SMALL_INT]*/
		len = strlen(mark);
	} else {
		return (E_BAD_ARGUMENT);
	}
	if (m) {
		if (Posted(m) && len != Marklen(m)) {
			return (E_BAD_ARGUMENT);
		}
		Mark(m) = mark;
		Marklen(m) = len;
		if (Posted(m)) {
			_draw(m);		/* Redraw menu */
			_show(m);		/* Redisplay menu */
		} else {
			_scale(m);		/* Redo sizing information */
		}
	} else {
		Mark(Dfl_Menu) = mark;
		Marklen(Dfl_Menu) = len;
	}
	return (E_OK);
}

char *
menu_mark(MENU *m)
{
	return (Mark(m ? m : Dfl_Menu));
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)menuopts.c	1.4	97/07/09 SMI"	/* SVr4.0 1.9	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

int
set_menu_opts(MENU *m, int opt)
{
	ITEM **ip;

	if (m) {
		if (Posted(m)) {
			return (E_POSTED);
		}

		/* Check to see if the ROWMAJOR option is changing.  If so, */
		/* set top and current to 0. */
		if ((opt & O_ROWMAJOR) != RowMajor(m)) {
			Top(m) = 0;
			Current(m) = IthItem(m, 0);
			(void) set_menu_format(m, FRows(m), FCols(m));
		}

		/* if O_NONCYCLIC option changed, set bit to re-link items */
		if ((opt & O_NONCYCLIC) != (Mopt(m) & O_NONCYCLIC)) {
			SetLink(m);
		}

		Mopt(m) = opt;
		if (OneValue(m) && Items(m)) {
			for (ip = Items(m); *ip; ip++) {
				/* Unset values if selection not allowed. */
				Value(*ip) = FALSE;
			}
		}
		_scale(m);		/* Redo sizing information */
	} else {
		Mopt(Dfl_Menu) = opt;
	}
	return (E_OK);
}

int
menu_opts_off(MENU *m, OPTIONS opt)
{
	return (set_menu_opts(m, (Mopt(m ? m : Dfl_Menu)) & ~opt));
}

int
menu_opts_on(MENU *m, OPTIONS opt)
{
	return (set_menu_opts(m, (Mopt(m ? m : Dfl_Menu)) | opt));
}

OPTIONS
menu_opts(MENU *m)
{
	return (Mopt(m ? m : Dfl_Menu));
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)show.c	1.4	97/07/09 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

/* Display that portion of the menu visable to the user */

void
_show(MENU *m)
{
	int r, c;
	WINDOW *us;

	if (Posted(m) || Indriver(m)) {
		(void) mvderwin(Sub(m), Top(m), 0);
		us = US(m);
		getmaxyx(us, r, c);
		r = min(Height(m), r);
		c = min(Width(m), c);
		(void) copywin(Sub(m), us, 0, 0, 0, 0, r-1, c-1, FALSE);
		_position_cursor(m);
	}
}

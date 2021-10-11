/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)slk_atroff.c	1.7	97/06/25 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "curses_inc.h"

int
slk_attroff(chtype a)
{
	WINDOW *win;

	/* currently we change slk attribute only when using software */
	/* slk's.  However, we may introduce a new terminfo variable  */
	/* which would allow manipulating the hardware slk's as well  */

	if ((SP->slk == NULL) || ((win = SP->slk->_win) == NULL))
		return (ERR);

	return (wattroff(win, a));
}

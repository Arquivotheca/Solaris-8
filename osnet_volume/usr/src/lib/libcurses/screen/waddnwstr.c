/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */


#pragma ident  "@(#)waddnwstr.c 1.3 97/06/25 SMI"

#include	<sys/types.h>
#include	"curses_inc.h"

/*
**	Add to 'win' at most n 'characters' of code starting at(cury, curx)
*/
int
waddnwstr(WINDOW *win, wchar_t *code, int n)
{
	char	*sp;

	/* translate the process code to character code */
	if ((sp = _strcode2byte(code, NULL, n)) == NULL)
		return (ERR);

	/* now call waddnstr to do the real work */
	return (waddnstr(win, sp, -1));
}

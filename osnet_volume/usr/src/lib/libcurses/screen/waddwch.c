/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)waddwch.c 1.3 97/06/25 SMI"

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"


/*
**	Add to 'win' a character at(curx, cury).
*/
int
waddwch(WINDOW *win, chtype c)
{
	int	width;
	char	buf[CSMAX];
	chtype	a;
	wchar_t	code;
	char	*p;

	a = c & A_WATTRIBUTES;
	code = c & A_WCHARTEXT;

	/* translate the process code to character code */
	if ((width = _curs_wctomb(buf, code & TRIM)) < 0)
		return (ERR);

	/* now call waddch to do the real work */
	p = buf;
	while (width--)
		if (waddch(win, a|(0xFF & *p++)) == ERR)
			return (ERR);
	return (OK);
}

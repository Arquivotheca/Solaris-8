/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)tgetwch.c 1.4 97/08/20 SMI"

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
**	Read a process code from the terminal
**	cntl:	= 0 for single-char key only
**		= 1 for matching function key and macro patterns.
**		= 2 same as 1 but no time-out for funckey matching.
*/

wchar_t
tgetwch(int cntl)
{
	int	c, n, type, width;
	char	buf[CSMAX];
	wchar_t	wchar;

	/* get the first byte */
	/* LINTED */
	if ((c = (int)tgetch(cntl)) == ERR)
		return (ERR);

	type = TYPE(c);
	width = cswidth[type] - ((type == 1 || type == 2) ? 0 : 1);
	/* LINTED */
	buf[0] = (char) c;
	for (n = 1; n < width; ++n) {
		/* LINTED */
		if ((c = (int)tgetch(cntl)) == ERR)
			return (ERR);
		if (TYPE(c) != 0)
			return (ERR);
		/* LINTED */
		buf[n] = (char) c;
		}

	/* translate it to process code */
	(void) _curs_mbtowc(&wchar, buf, n);
	return (wchar);
}

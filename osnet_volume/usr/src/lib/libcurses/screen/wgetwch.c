/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)wgetwch.c 1.5 97/08/22 SMI"

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
**	Get a process code
*/

int
wgetwch(WINDOW *win)
{
	int	c, n, type, width;
	char	buf[CSMAX];
	wchar_t	wchar;

	/* get the first byte */
	if ((c = wgetch(win)) == ERR)
		return (ERR);

	if (c >= KEY_MIN)
		return (c);

	type = TYPE(c);
	width = cswidth[type] - ((type == 1 || type == 2) ? 0 : 1);
	/* LINTED */
	buf[0] = (char) c;
	for (n = 1; n <= width; ++n) {
		if ((c = wgetch(win)) == ERR)
			return (ERR);
		if (TYPE(c) != 0)
			return (ERR);
		/* LINTED */
		buf[n] = (char) c;
	}

	/* translate it to process code */
	if ((_curs_mbtowc(&wchar, buf, n)) < 0)
		return (ERR);
	return ((int) wchar);
	}

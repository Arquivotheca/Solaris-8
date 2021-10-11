/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)ungetwch.c 1.3 97/06/25 SMI"

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
**	Push a process code back into the input stream
*/

int
ungetwch(wchar_t code)
{
	int	i, n;
	char	buf[CSMAX];

	n = _curs_wctomb(buf, code & TRIM);
	for (i = n - 1; i >= 0; --i)
		if (ungetch((unsigned char)buf[i]) == ERR) {
			/* remove inserted characters */
			for (i = i + 1; i < n; ++i)
				(void) tgetch(0);
			return (ERR);
		}

	return (OK);
}

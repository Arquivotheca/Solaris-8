/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)mbdisplen.c 1.3 97/06/25 SMI"

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
**	Get the display length of a string.
*/

int
mbdisplen(char *sp)
{
	int		n, m, k, ty;
	chtype	c;

	n = 0;
	for (; *sp != '\0'; ++sp) {
		if (!ISMBIT(*sp))
			++n;
		else {
			c = RBYTE(*sp);
			ty = TYPE(c & 0377);
			m  = cswidth[ty] - (ty == 0 ? 1 : 0);
			for (sp += 1, k = 1; *sp != '\0' && k <= m;
			    ++k, ++sp) {
				c = RBYTE(*sp);
				if (TYPE(c) != 0)
					break;
			}
			if (k <= m)
				break;
			n += _curs_scrwidth[ty];
		}
	}
	return (n);
}

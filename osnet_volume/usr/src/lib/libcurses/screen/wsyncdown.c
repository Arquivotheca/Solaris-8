/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)wsyncdown.c	1.7	97/06/25 SMI"	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/* Make the changes in ancestors visible in win. */

void
wsyncdown(WINDOW *win)
{
	short	*wbch, *wech, *pbch, *pech, bch, ech, endx;
	int	wy, px, py, endy;
	WINDOW	*par;

	py = win->_pary;
	px = win->_parx;
	endy = win->_maxy;
	endx = win->_maxx - 1;

	for (par = win->_parent; par != NULL; par = par->_parent) {
		if (par->_flags & (_WINCHANGED | _WIN_ADD_ONE |
		    _WIN_INS_ONE)) {
			wbch = win->_firstch;
			wech = win->_lastch;
			pbch = par->_firstch + py;
			pech = par->_lastch + py;

			for (wy = 0; wy < endy; ++wy, ++wbch, ++wech,
			    ++pbch, ++pech) {
				if (*pbch != _INFINITY) {
					if ((bch = *pbch - px) < 0)
						bch = 0;
					if ((ech = *pech - px) > endx)
						ech = endx;
					if (!(bch > endx || ech < 0)) {
						if (*wbch > bch)
							*wbch = bch;
						if (*wech < ech)
							*wech = ech;
					}
				}
			}
			win->_flags |= _WINCHANGED;
		}

		py += par->_pary;
		px += par->_parx;
	}
}

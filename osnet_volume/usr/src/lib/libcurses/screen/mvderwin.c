/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)mvderwin.c	1.8	97/08/22 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
 * Move a derived window inside its parent window.
 * This routine does not change the screen relative
 * parameters begx and begy. Thus, it can be used to
 * display different parts of the parent window at
 * the same screen coordinate.
 */

int
mvderwin(WINDOW *win, int pary, int parx)
{
	int	y, maxy;
	WINDOW	*par;
	chtype	obkgd, **wc, **pc;
	short	*begch, *endch, maxx;

	if ((par = win->_parent) == NULL)
		goto bad;
	if (pary == win->_pary && parx == win->_parx)
		return (OK);

	maxy = win->_maxy-1;
	maxx = win->_maxx-1;
	if ((parx + maxx) >= par->_maxx || (pary + maxy) >= par->_maxy)
bad:
		return (ERR);

	/* save all old changes */
	wsyncup(win);

	/* rearrange pointers */
	/*LINTED*/
	win->_parx = (short) parx;
	/*LINTED*/
	win->_pary = (short) pary;
	wc = win->_y;
	pc = par->_y + pary;
	begch = win->_firstch;
	endch = win->_lastch;
	for (y = 0; y <= maxy; ++y, ++wc, ++pc, ++begch, ++endch) {
		*wc = *pc + parx;
		*begch = 0;
		*endch = maxx;
	}

	/* change background to our own */
	obkgd = win->_bkgd;
	win->_bkgd = par->_bkgd;
	return (wbkgd(win, obkgd));
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)putwin.c	1.7	97/06/25 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
 * Write a window to a file.
 *
 * win:	the window to write out.
 * filep:	the file to write to.
 */

int
putwin(WINDOW *win, FILE *filep)
{
	int	maxx, nelt;
	chtype	**wcp, **ecp;

	/* write everything from _cury to _bkgd inclusive */
	nelt = sizeof (WINDOW) - sizeof (win->_y) - sizeof (win->_parent) -
	    sizeof (win->_parx) - sizeof (win->_pary) -
	    sizeof (win->_ndescs) - sizeof (win->_delay);

	if (fwrite((char *) &(win->_cury), 1, nelt, filep) != nelt)
		goto err;

	/* Write the character image */
	maxx = win->_maxx;
	ecp = (wcp = win->_y) + win->_maxy;
	while (wcp < ecp)
		if (fwrite((char *) *wcp++, sizeof (chtype),
		    maxx, filep) != maxx)
err:
			return (ERR);

	return (OK);
}

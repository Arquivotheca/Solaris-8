/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)wsetscrreg.c	1.8	97/08/14 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
 *	Change scrolling region. Since we depend on the values
 *	of tmarg and bmarg in various ways, this can no longer
 *	be a macro.
 */

int
wsetscrreg(WINDOW *win, int topy, int boty)
{
	if (topy < 0 || topy >= win->_maxy || boty < 0 || boty >= win->_maxy)
		return (ERR);

	/* LINTED */
	win->_tmarg = (short) topy;
	/* LINTED */
	win->_bmarg = (short) boty;
	return (OK);
}

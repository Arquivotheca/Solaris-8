/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)wprintw.c	1.7	97/06/25 SMI"	/* SVr4.0 1.10	*/

/*LINTLIBRARY*/

/*
 * printw and friends
 *
 */

#include	<sys/types.h>
#include	"curses_inc.h"
#include	<stdarg.h>

/*
 *	This routine implements a printf on the given window.
 */
/*VARARGS*/
wprintw(WINDOW *win, ...)
{
	va_list ap;
	char * fmt;

	va_start(ap, win);
	fmt = va_arg(ap, char *);
	return (vwprintw(win, fmt, ap));
}

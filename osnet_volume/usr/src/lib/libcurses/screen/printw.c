/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)printw.c	1.6	97/06/25 SMI"	/* SVr4.0 1.8	*/
/*	From:	SVr4.0	curses:screen/printw.c	1.8		*/

/*LINTLIBRARY*/

/*
 * printw and friends
 *
 */

#include	<sys/types.h>
#include	"curses_inc.h"
#include	<stdarg.h>

/*
 *	This routine implements a printf on the standard screen.
 */
/*VARARGS1*/
printw(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	return (vwprintw(stdscr, fmt, ap));
}

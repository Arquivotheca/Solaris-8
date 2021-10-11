/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)vwprintw.c	1.8	97/06/25 SMI"	/* SVr4.0 1.7	*/

/*LINTLIBRARY*/

/*
 * printw and friends
 *
 */

#include	<sys/types.h>
#include	<stdlib.h>
#include	"curses_inc.h"
#include	<stdarg.h>

/*
 *	This routine actually executes the printf and adds it to the window
 *
 *	This code now uses the vsprintf routine, which portably digs
 *	into stdio.  We provide a vsprintf for older systems that don't
 *	have one.
 */

/*VARARGS2*/
int
vwprintw(WINDOW *win, char *fmt, va_list ap)
{
	int size = BUFSIZ;
	char *buffer;
	int n, rv;

	buffer = (char *) malloc(size);
	if (buffer == NULL)
		return (ERR);
	/*CONSTCOND*/
	while (1) {
		n = vsnprintf(buffer, size, fmt, ap);
		if (n < size)
			break;
		size *= 2;
		buffer = (char *) realloc(buffer, size);
		if (buffer == NULL)
			return (ERR);
	}
	va_end(ap);
	rv = waddstr(win, buffer);
	free(buffer);
	return (rv);
}

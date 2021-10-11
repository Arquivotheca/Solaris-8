/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)vwscanw.c	1.9	97/06/25 SMI"	/* SVr4.0 1.8	*/

/*LINTLIBRARY*/

/*
 * scanw and friends
 *
 */
#include	<sys/types.h>
#include	"curses_inc.h"
#include	<stdarg.h>

/*
 *	This routine actually executes the scanf from the window.
 *
 *	This code calls _vsscanf, which is like sscanf except
 * 	that it takes a va_list as an argument pointer instead
 *	of the argument list itself.  We provide one until
 *	such a routine becomes available.
 */

/*VARARGS2*/
int
vwscanw(WINDOW *win, char *fmt, va_list ap)
{
	wchar_t code[256];
	char	*buf;
	int	n;

	if (wgetwstr(win, code) == ERR)
		n = ERR;
	else	{
		buf = _strcode2byte(code, NULL, -1);
		n = _vsscanf(buf, fmt, ap);
	}

	va_end(ap);
	return (n);
}

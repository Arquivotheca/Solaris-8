/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)wscanw.c	1.7	97/06/25 SMI"	/* SVr4.0 1.10	*/

/*LINTLIBRARY*/

/*
 * scanw and friends
 *
 */
#include	<sys/types.h>
#include	"curses_inc.h"
#include	<stdarg.h>

/*
 *	This routine implements a scanf on the given window.
 */
/*VARARGS*/
wscanw(WINDOW *win, ...)
{
	char	*fmt;
	va_list	ap;

	va_start(ap, win);
	fmt = va_arg(ap, char *);
	return (vwscanw(win, fmt, ap));
}

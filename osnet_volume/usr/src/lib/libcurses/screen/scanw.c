/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)scanw.c	1.6	97/06/25 SMI"	/* SVr4.0 1.8	*/
/*	From:	SVr4.0	curses:screen/scanw.c	1.8		*/

/*LINTLIBRARY*/

/*
 * scanw and friends
 *
 */

#include	<sys/types.h>
#include	"curses_inc.h"
#include	<stdarg.h>

/*
 *	This routine implements a scanf on the standard screen.
 */
/*VARARGS1*/

int
scanw(char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	return (vwscanw(stdscr, fmt, ap));
}

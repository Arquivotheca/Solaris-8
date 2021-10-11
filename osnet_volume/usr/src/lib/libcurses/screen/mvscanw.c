/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)mvscanw.c	1.7	97/06/25 SMI"	/* SVr4.0 1.9	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"
#include	<stdarg.h>

/*
 * implement the mvscanw commands.  Due to the variable number of
 * arguments, they cannot be macros.  Another sigh....
 *
 */

int
mvscanw(int y, int x, ...)
{
	char	*fmt;
	va_list		ap;

	va_start(ap, x);
	fmt = va_arg(ap, char *);
	return (move(y, x) == OK ? vwscanw(stdscr, fmt, ap) : ERR);
}

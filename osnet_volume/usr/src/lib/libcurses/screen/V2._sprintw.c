/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)V2._sprintw.c	1.8	97/08/25 SMI"	/* SVr4.0 1.7	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"
#include	<stdarg.h>

#ifdef _VR2_COMPAT_CODE
/*
	This is only here for compatibility with SVR2 curses.
	It will go away someday. Programs should reference
	vwprintw() instead.
 */

int
_sprintw(WINDOW *win, char *fmt, va_list ap)
{
	return (vwprintw(win, fmt, ap));
}
#endif

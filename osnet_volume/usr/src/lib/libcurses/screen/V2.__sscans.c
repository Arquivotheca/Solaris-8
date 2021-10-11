/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)V2.__sscans.c	1.8	97/08/25 SMI"	/* SVr4.0 1.8	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"
#include	<stdarg.h>

#ifdef _VR2_COMPAT_CODE
/*
	This file is provided for compatibility reasons only
	and will go away someday. Programs should reference
	vwscanw() instead.
 */


int
__sscans(WINDOW *win, char *fmt, va_list ap)
{
	return (vwscanw(win, fmt, ap));
}
#endif

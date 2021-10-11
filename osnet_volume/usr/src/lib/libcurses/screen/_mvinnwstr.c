/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)_mvinnwstr.c 1.3 97/06/25 SMI"

/*LINTLIBRARY*/

#define		NOMACROS
#include	<sys/types.h>
#include	"curses_inc.h"

int
mvinnwstr(int y, int x, wchar_t *ws, int n)
{
	return (mvwinnwstr(stdscr, y, x, ws, n));
}
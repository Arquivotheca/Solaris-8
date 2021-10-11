/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)scrollok.c	1.8	97/06/20 SMI"	/* SVr4.0 1.8	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

int
scrollok(WINDOW *win, bool bf)
{
	win->_scroll = (bf) ? TRUE : FALSE;
	return (OK);
}

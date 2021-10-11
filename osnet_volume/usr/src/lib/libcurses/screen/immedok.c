/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)immedok.c	1.8	97/06/20 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"


void
immedok(WINDOW *win, bool bf)
{
	if (bf) {
		win->_immed = TRUE;
		_quick_ptr = _quick_echo;
	} else
		win->_immed = FALSE;
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)keypad.c	1.8	97/06/20 SMI"	/* SVr4.0 1.8	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/* TRUE => special keys should be passed as a single value by getch. */

int
keypad(WINDOW *win, bool bf)
{
	/*
	* Since _use_keypad is a bit and not a bool, we must check
	* bf, in case someone uses an odd number instead of 1 for TRUE
	*/

	win->_use_keypad = (bf) ? TRUE : FALSE;
	if (bf && (!SP->kp_state)) {
		(void) tputs(keypad_xmit, 1, _outch);
		(void) fflush(SP->term_file);
		SP->kp_state = TRUE;
		return (setkeymap());
	}
	return (OK);
}

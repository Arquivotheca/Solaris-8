/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)ungetch.c	1.8	97/06/25 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "curses_inc.h"

/* Place a char onto the beginning of the input queue. */

int
ungetch(int ch)
{
	int	i = cur_term->_chars_on_queue, j = i - 1;
	chtype	*inputQ = cur_term->_input_queue;

	/* Place the character at the beg of the Q */
	/*
	 * Here is commented out because 'ch' should deal with a single byte
	 * character only. So ISCBIT(ch) is 0 every time.

	 * register chtype	r;

	 * if (ISCBIT(ch)) {
	 * 	r = RBYTE(ch);
	 *	ch = LBYTE(ch);
	 *	-* do the right half first to maintain the byte order *-
	 *	if (r != MBIT && ungetch(r) == ERR)
	 *		return (ERR);
	 *}
	*/

	while (i > 0)
		inputQ[i--] = inputQ[j--];
	cur_term->_ungotten++;
	inputQ[0] = -ch;
	cur_term->_chars_on_queue++;
	return (0);
}

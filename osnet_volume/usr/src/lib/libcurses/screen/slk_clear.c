/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)slk_clear.c	1.10	97/08/22 SMI"	/* SVr4.0 1.9	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/* Clear the soft labels. */

int
slk_clear(void)
{
	SLK_MAP	*slk;
	int	i;
	char *	spaces = "        ";

	if ((slk = SP->slk) == NULL)
		return (ERR);

	slk->_changed = 2;	/* This means no more soft labels. */
	if (slk->_win) {
		(void) werase(slk->_win);
		(void) wrefresh(slk->_win);
	} else {
		/* send hardware clear sequences */
		for (i = 0; i < slk->_num; i++)
			_PUTS(tparm_p2(plab_norm, i + 1,
			    (long) spaces), 1);
		_PUTS(label_off, 1);
		(void) fflush(SP->term_file);
	}

	for (i = 0; i < slk->_num; ++i)
		slk->_lch[i] = FALSE;

	return (OK);
}

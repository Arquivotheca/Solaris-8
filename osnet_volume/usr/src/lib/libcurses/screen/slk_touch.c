/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)slk_touch.c	1.7	97/06/25 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/* Make the labels appeared changed. */

int
slk_touch(void)
{
	SLK_MAP	*slk;
	int	i;

	if (((slk = SP->slk) == NULL) || (slk->_changed == 2))
		return (ERR);

	for (i = 0; i < slk->_num; ++i)
		slk->_lch[i] = TRUE;
	slk->_changed = TRUE;

	return (OK);
}

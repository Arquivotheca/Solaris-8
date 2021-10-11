/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)slk_restore.c	1.7	97/06/25 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/* Restore screen labels. */

int
slk_restore(void)
{
	if (SP->slk) {
		SP->slk->_changed = TRUE;
		(void) slk_touch();
		(void) slk_refresh();
	}
	return (OK);
}

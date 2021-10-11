/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)slk_label.c	1.7	97/06/25 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/* Return the current label of key number 'n'. */

char *
slk_label(int n)
{
	SLK_MAP	*slk = SP->slk;

	/*
	 * strip initial blanks
	 *
	 * for (; *lab != '\0'; ++lab)
	 * if (*lab != ' ')
	 *   break;
	 * strip trailing blanks
	 *
	 * for (; cp > lab; --cp)
	 * if (*(cp-1) != ' ')
	 *   break;
	*/

	return ((!slk || n < 1 || n > slk->_num) ? NULL : slk->_lval[n - 1]);
}

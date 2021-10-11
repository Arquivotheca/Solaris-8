/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)tgoto.c	1.6	97/06/25 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "curses_inc.h"
/*
 * tgoto: function included only for upward compatibility with old termcap
 * library.  Assumes exactly two parameters in the wrong order.
 */

char	*
tgoto(char *cap, int col, int row)
{
	char	*cp;

	cp = tparm_p2(cap, row, col);
	return (cp);
}

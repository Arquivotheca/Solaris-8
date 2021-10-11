/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)has_il.c	1.7	97/06/25 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "curses_inc.h"

/* Query: does the terminal have insert/delete line? */

int
has_il(void)
{
	return (((insert_line || parm_insert_line) &&
	    (delete_line || parm_delete_line)) || change_scroll_region);
}

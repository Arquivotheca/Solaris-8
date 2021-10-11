/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)_del_curterm.c	1.8	97/08/25 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#define	NOMACROS
#include	<sys/types.h>
#include	"curses_inc.h"

int
del_curterm(TERMINAL *terminal)
{
	return (delterm(terminal));
}

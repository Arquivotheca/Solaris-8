/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)erasechar.c	1.8	97/06/25 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

/*
 * Routines to deal with setting and resetting modes in the tty driver.
 * See also setupterm.c in the termlib part.
 */

#include <sys/types.h>
#include "curses_inc.h"

char
erasechar(void)
{
#ifdef SYSV
	return (SHELLTTYS.c_cc[VERASE]);
#else
	return (SHELLTTY.sg_erase);
#endif
}

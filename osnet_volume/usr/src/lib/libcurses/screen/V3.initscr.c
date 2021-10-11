/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)V3.initscr.c	1.7	97/06/25 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

extern void	_update_old_y_area(WINDOW *, int, int, int, int);

#ifdef	_VR3_COMPAT_CODE

#undef	initscr
WINDOW	*
initscr(void)
{
	_y16update = _update_old_y_area;
	return (initscr32());
}
#endif /* _VR3_COMPAT_CODE */

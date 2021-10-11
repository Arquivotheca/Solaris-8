/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)V2.makenew.c	1.7	97/06/25 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "curses_inc.h"

#ifdef _VR2_COMPAT_CODE

WINDOW *
makenew(int num_lines, int num_cols, int begy, int begx)
{
	return (_makenew(num_lines, num_cols, begy, begx));
}
#endif

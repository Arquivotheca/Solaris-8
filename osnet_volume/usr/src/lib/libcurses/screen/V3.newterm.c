/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)V3.newterm.c	1.6	97/06/25 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

#ifdef	_VR3_COMPAT_CODE

#undef	newterm
SCREEN	*
newterm(char *type, FILE *outfptr, FILE *infptr)
{
	_y16update = _update_old_y_area;
	return (newscreen(type, 0, 0, 0, outfptr, infptr));
}
#endif	/* _VR3_COMPAT_CODE */

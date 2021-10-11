/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)V3.vidattr.c	1.7	97/08/25 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

extern int _outchar(char);

#ifdef	_VR3_COMPAT_CODE
#undef	vidattr

int
vidattr(_ochtype a)
{
	vidupdate(_FROM_OCHTYPE(a), cur_term->sgr_mode, _outchar);
	return (OK);
}
#endif	/* _VR3_COMPAT_CODE */

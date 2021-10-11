/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)V3.vidputs.c	1.7	97/08/25 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

#ifdef	_VR3_COMPAT_CODE
#undef	vidputs
int
vidputs(_ochtype a, int (*o)(char))
{
	vidupdate(_FROM_OCHTYPE(a), cur_term->sgr_mode, o);
	return (OK);
}
#endif	/* _VR3_COMPAT_CODE */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)setcurterm.c	1.7	97/06/25 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "curses_inc.h"

/*
 * Establish the terminal that the #defines in term.h refer to.
 */

TERMINAL *
setcurterm(TERMINAL *newterminal)
{
	TERMINAL	*oldterminal = cur_term;

	if (newterminal) {
#ifdef	_VR3_COMPAT_CODE
		acs_map = cur_term->_acs32map;
#else	/* _VR3_COMPAT_CODE */
		acs_map = cur_term->_acsmap;
#endif	/* _VR3_COMPAT_CODE */
		cur_bools = newterminal->_bools;
		cur_nums = newterminal->_nums;
		cur_strs = newterminal->_strs;
		cur_term = newterminal;
	}
	return (oldterminal);
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)color_cont.c	1.9	97/06/20 SMI"
		/* SVr4.0 1.3.1.1	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "curses_inc.h"

int
color_content(short color, short *r, short *g, short *b)
{
	_Color *ctp;

	if (color < 0 || color > COLORS || !can_change ||
	    (ctp = cur_term->_color_tbl) == (_Color *) NULL)
		return (ERR);

	ctp += color;
	*r = ctp->r;
	*g = ctp->g;
	*b = ctp->b;
	return (OK);
}

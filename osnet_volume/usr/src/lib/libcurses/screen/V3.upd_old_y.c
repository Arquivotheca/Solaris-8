/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)V3.upd_old_y.c	1.7	97/08/22 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

#ifdef	_VR3_COMPAT_CODE
void
_update_old_y_area(WINDOW *win, int nlines, int ncols,
	int start_line, int start_col)
{
	int	row, col, num_cols;

	for (row = start_line; nlines > 0; nlines--, row++)
		for (num_cols = ncols, col = start_col; num_cols > 0;
		    num_cols--, col++)
			/*LINTED*/
			win->_y16[row][col] = _TO_OCHTYPE(win->_y[row][col]);
}
#endif	/* _VR3_COMPAT_CODE */

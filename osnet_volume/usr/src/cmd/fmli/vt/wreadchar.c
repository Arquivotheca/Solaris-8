/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright  (c) 1986 AT&T
 *	All Rights Reserved
 */
#ident	"@(#)wreadchar.c	1.4	92/07/14 SMI"       /* SVr4.0 1.1 */

#include	<curses.h>
#include	"wish.h"
#include	"vtdefs.h"
#include	"vt.h"

wreadchar(row, col)
unsigned row;
unsigned col;
{
	register struct	vt	*v;
	int savey, savex;
	register char ch;

	v = &VT_array[VT_curid];
	getyx(v->win, savey, savex);
	if (!(v->flags & VT_NOBORDER)) {
		row++;
		col++;
	}
	ch = (char)(mvwinch(v->win, row, col) & A_CHARTEXT);
	wmove(v->win, savey, savex);		/* return cursor */
	return(ch);
}

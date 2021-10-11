/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)setcurscreen.c	1.8	97/06/25 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "curses_inc.h"

SCREEN	*
setcurscreen(SCREEN *new)
{
	SCREEN	*rv = SP;

	if (new != SP) {

#ifdef	DEBUG
		if (outf)
			fprintf(outf, "setterm: old %x, new %x\n", rv, new);
#endif	/* DEBUG */

		SP = new;
		if (new) {
			(void) setcurterm(SP->tcap);
			LINES = SP->lsize;
			COLS = SP->csize;
			TABSIZE = SP->tsize;
			stdscr = SP->std_scr;
			curscr = SP->cur_scr;
			_virtscr = SP->virt_scr;
		}
	}
	return (rv);
}

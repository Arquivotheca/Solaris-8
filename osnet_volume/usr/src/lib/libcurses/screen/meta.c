/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)meta.c	1.9	97/06/20 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/* TRUE => all 8 bits of input character should be passed through. */

int
_meta(int bf)
{
	/*
	* Do the appropriate fiddling with the tty driver to make it send
	* all 8 bits through.  On SYSV this means clearing ISTRIP, on
	* V7 you have to resort to RAW mode.
	*/
#ifdef	SYSV
	if (bf)
		PROGTTYS.c_iflag &= ~ISTRIP;
	else
		PROGTTYS.c_iflag |= ISTRIP;
	(void) reset_prog_mode();
#else	/* SYSV */
	if (bf)
		raw();
	else
		noraw();
#endif	/* SYSV */

	/* Do whatever is needed to put the terminal into meta-mode. */

	if ((SP->fl_meta = bf) != 0)
		(void) tputs(meta_on, 1, _outch);
	else
		(void) tputs(meta_off, 1, _outch);
	(void) fflush(SP->term_file);
	return (OK);
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)cbreak.c	1.8	97/06/25 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

/*
 * Routines to deal with setting and resetting modes in the tty driver.
 * See also setupterm.c in the termlib part.
 */
#include <sys/types.h>
#include "curses_inc.h"

int
cbreak(void)
{
	/*
	* This optimization is here because till SVR3.1 curses did not come up
	* in cbreak mode and now it does.  Therefore, most programs when they
	* call cbreak won't pay for it since we'll know we're in the right mode.
	*/

	if (cur_term->_fl_rawmode != 1) {
#ifdef SYSV
	/*
	 * You might ask why ICRNL has anything to do with cbreak.
	 * The problem is that there are function keys that send
	 * a carriage return (some hp's).  Curses cannot virtualize
	 * these function keys if CR is being mapped to a NL.  Sooo,
	 * when we start a program up we unmap those but if you are
	 * in nocbreak then we map them back.  The reason for that is that
	 * if a getch or getstr is done and you are in nocbreak the tty
	 * driver won't return until it sees a new line and since we've
	 * turned it off any program that has nl() and nocbreak() would
	 * force the user to type a NL.  The problem with the function keys
	 * only gets solved if you are in cbreak mode which is OK
	 * since program taking action on a function key is probably
	 * in cbreak because who would expect someone to press a function
	 * key and then return ?????
	 */

		PROGTTYS.c_iflag &= ~ICRNL;
		PROGTTYS.c_lflag &= ~ICANON;
		PROGTTYS.c_cc[VMIN] = 1;
		PROGTTYS.c_cc[VTIME] = 0;
#else
		PROGTTY.sg_flags |= (CBREAK | CRMOD);
#endif

#ifdef DEBUG
#ifdef SYSV
		if (outf)
			fprintf(outf, "cbreak(), file %x, flags %x\n",
			    cur_term->Filedes, PROGTTYS.c_lflag);
#else
		if (outf)
			fprintf(outf, "cbreak(), file %x, flags %x\n",
			    cur_term->Filedes, PROGTTY.sg_flags);
#endif
#endif
		cur_term->_fl_rawmode = 1;
		cur_term->_delay = -1;
		(void) reset_prog_mode();
#ifdef FIONREAD
		cur_term->timeout = 0;
#endif /* FIONREAD */
	}
	return (OK);
}

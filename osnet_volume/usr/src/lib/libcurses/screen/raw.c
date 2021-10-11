/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)raw.c	1.8	97/06/25 SMI"	/* SVr4.0 1.12	*/

/*LINTLIBRARY*/

/*
 * Routines to deal with setting and resetting modes in the tty driver.
 * See also setupterm.c in the termlib part.
 */
#include <sys/types.h>
#include "curses_inc.h"

int
raw(void)
{
#ifdef SYSV
	/* Disable interrupt characters */
	PROGTTYS.c_lflag &= ~(ISIG|ICANON);
	PROGTTYS.c_cc[VMIN] = 1;
	PROGTTYS.c_cc[VTIME] = 0;
	PROGTTYS.c_iflag &= ~IXON;
#else
	PROGTTY.sg_flags &= ~CBREAK;
	PROGTTY.sg_flags |= RAW;
#endif

#ifdef DEBUG
#ifdef SYSV
	if (outf)
		fprintf(outf, "raw(), file %x, iflag %x, cflag %x\n",
	cur_term->Filedes, PROGTTYS.c_iflag, PROGTTYS.c_cflag);
#else
	if (outf)
		fprintf(outf, "raw(), file %x, flags %x\n",
	cur_term->Filedes, PROGTTY.sg_flags);
#endif /* SYSV */
#endif

	if (!needs_xon_xoff)
		xon_xoff = 0;	/* Cannot use xon/xoff in raw mode */
	cur_term->_fl_rawmode = 2;
	cur_term->_delay = -1;
	(void) reset_prog_mode();
#ifdef FIONREAD
	cur_term->timeout = 0;
#endif /* FIONREAD */
	return (OK);
}

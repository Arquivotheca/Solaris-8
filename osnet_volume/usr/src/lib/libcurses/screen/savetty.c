/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)savetty.c	1.8	97/06/25 SMI"	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/

/*
 * Routines to deal with setting and resetting modes in the tty driver.
 * See also setupterm.c in the termlib part.
 */
#include <sys/types.h>
#include "curses_inc.h"

int
savetty(void)
{
#ifdef SYSV
	if (prog_istermios < 0) {
		int i;

		PROGTTY.c_lflag = PROGTTYS.c_lflag;
		PROGTTY.c_oflag = PROGTTYS.c_oflag;
		PROGTTY.c_iflag = PROGTTYS.c_iflag;
		PROGTTY.c_cflag = PROGTTYS.c_cflag;
		for (i = 0; i < NCC; i++)
			PROGTTY.c_cc[i] = PROGTTYS.c_cc[i];
		SP->save_tty_buf = PROGTTY;
	} else
		SP->save_tty_bufs = PROGTTYS;
#else	/* SYSV */
	SP->save_tty_buf = PROGTTY;
#endif	/* SYSV */
#ifdef DEBUG
#ifdef SYSV
	if (outf)
		fprintf(outf, "savetty(), file %x, SP %x, flags %x,%x,%x,%x\n",
	cur_term->Filedes, SP, PROGTTYS.c_iflag, PROGTTYS.c_oflag,
	PROGTTYS.c_cflag, PROGTTYS.c_lflag);
#else
	if (outf)
		fprintf(outf, "savetty(), file %x, SP %x, flags %x\n",
	cur_term->Filedes, SP, PROGTTY.sg_flags);
#endif
#endif
	return (OK);
}

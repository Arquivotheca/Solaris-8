/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)noraw.c	1.8	97/06/25 SMI"	/* SVr4.0 1.9	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

int
noraw(void)
{
#ifdef	SYSV
	/* Enable interrupt characters */
	PROGTTYS.c_lflag |= (ISIG|ICANON);
	PROGTTYS.c_cc[VEOF] = _CTRL('D');
	PROGTTYS.c_cc[VEOL] = 0;
	PROGTTYS.c_iflag |= IXON;
#else	/* SYSV */
	PROGTTY.sg_flags &= ~(RAW|CBREAK);
#endif	/* SYSV */

#ifdef	DEBUG
#ifdef	SYSV
	if (outf)
		fprintf(outf, "noraw(), file %x, flags %x\n",
		    cur_term->Filedes, PROGTTYS.c_lflag);
#else	/* SYSV */
	if (outf)
		fprintf(outf, "noraw(), file %x, flags %x\n",
		    cur_term->Filedes, PROGTTY.sg_flags);
#endif	/* SYSV */
#endif	/* DEBUG */

	cur_term->_fl_rawmode = FALSE;
	cur_term->_delay = -1;
	(void) reset_prog_mode();
#ifdef	FIONREAD
	cur_term->timeout = 0;
#endif	/* FIONREAD */
	return (OK);
}

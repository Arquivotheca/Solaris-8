/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)restart.c	1.8	97/06/25 SMI"	/* SVr4.0 1.9	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
 * This is useful after saving/restoring memory from a file (e.g. as
 * in a rogue save game).  It assumes that the modes and windows are
 * as wanted by the user, but the terminal type and baud rate may
 * have changed.
 */

extern	char	_called_before;

int
/* The next line causes a lint warning because errret is not used */
restartterm(char *term, int filenum, int *errret)
/* int	filenum - This is a UNIX file descriptor, not a stdio ptr. */
{
	int	saveecho = SP->fl_echoit;
	int	savecbreak = cur_term->_fl_rawmode;
	int	savenl;

#ifdef	SYSV
	savenl = PROGTTYS.c_iflag & ONLCR;
#else	/* SYSV */
	savenl = PROGTTY.sg_flags & CRMOD;
#endif	/* SYSV */

	_called_before = 0;
	(void) setupterm(term, filenum, (int *) 0);

	/* Restore curses settable flags, leaving other stuff alone. */
	SP->fl_echoit = saveecho;

	(void) nocbreak();
	(void) noraw();
	if (savecbreak == 1)
		(void) cbreak();
	else
		if (savecbreak == 2)
			(void) raw();

	if (savenl)
		(void) nl();
	else
		(void) nonl();

	(void) reset_prog_mode();

	LINES = SP->lsize;
	COLS = columns;
	return (OK);
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)tstp.c	1.7	97/06/25 SMI"	/* SVr4.0 1.11	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	<stdlib.h>
#include	<signal.h>
#include	"curses_inc.h"


/* handle stop and start signals */

#ifdef	SIGTSTP
void
_tstp(int dummy)
{
#ifdef	DEBUG
	if (outf)
		(void) fflush(outf);
#endif	/* DEBUG */
	curscr->_attrs = A_ATTRIBUTES;
	(void) endwin();
	(void) fflush(stdout);
	(void) kill(0, SIGTSTP);
	(void) signal(SIGTSTP, _tstp);
	(void) fixterm();
	/* changed ehr3 SP->doclear = 1; */
	curscr->_clear = TRUE;
	(void) wrefresh(curscr);
}
#endif	/* SIGTSTP */

void
_ccleanup(int signo)
{
	(void) signal(signo, SIG_IGN);

	/*
	* Fake curses into thinking that all attributes are on so that
	* endwin will turn them off since the < BREAK > key may have
	* interrupted the sequence to turn them off.
	*/

	curscr->_attrs = A_ATTRIBUTES;
	(void) endwin();
#ifdef	DEBUG
	fprintf(stderr, "signal %d caught. quitting.\n", signo);
#endif	/* DEBUG */
	if (signo == SIGQUIT)
		(void) abort();
	else
		exit(1);
}

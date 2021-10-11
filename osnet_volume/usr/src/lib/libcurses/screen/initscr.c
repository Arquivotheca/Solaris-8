/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*	From:	SVr4.0	curses:screen/initscr.c	1.7		*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)initscr.c	1.8	97/08/22 SMI"	/* SVr4.0 1.7	*/

/*LINTLIBRARY*/

#include	<stdlib.h>
#include	<signal.h>
#include	<sys/types.h>
#include	"curses_inc.h"

/*
 * This routine initializes the current and standard screen,
 * and sets up the terminal.  In case of error, initscr aborts.
 * If you want an error status returned, call
 *	scp = newscreen(getenv("TERM"), 0, 0, 0, stdout, stdin);
 */

WINDOW	*
initscr(void)
{
#ifdef	SIGPOLL
	void	(*savsignal)(int);
	extern	void	_ccleanup(int);
#else	/* SIGPOLL */
	int		(*savsignal)();
	extern	int	_ccleanup(int);
#endif	/* SIGPOLL */

#ifdef	SIGTSTP
	extern	void	_tstp(int);
#endif	/* SIGTSTP */

	static	char	i_called_before = FALSE;

/* Free structures we are about to throw away so we can reuse the memory. */

	if (i_called_before && SP) {
		delscreen(SP);
		SP = NULL;
	}
	if (newscreen(NULL, 0, 0, 0, stdout, stdin) == NULL) {
		(void) reset_shell_mode();
		if (term_errno != -1)
			termerr();
		else
			curserr();
		exit(1);
	}

#ifdef	DEBUG
	if (outf)
		fprintf(outf, "initscr: term = %s\n", SP);
#endif	/* DEBUG */
	i_called_before = TRUE;

#ifdef	SIGTSTP
	/*LINTED*/
	if ((savsignal = signal(SIGTSTP, SIG_IGN)) == SIG_DFL)
		(void) signal(SIGTSTP, _tstp);
	else
		(void) signal(SIGTSTP, savsignal);
#endif	/* SIGTSTP */
	/*LINTED*/
	if ((savsignal = signal(SIGINT, SIG_IGN)) == SIG_DFL)
		(void) signal(SIGINT, _ccleanup);
	else
		(void) signal(SIGINT, savsignal);

	/*LINTED*/
	if ((savsignal = signal(SIGQUIT, SIG_IGN)) == SIG_DFL)
		(void) signal(SIGQUIT, _ccleanup);
	else
		(void) signal(SIGQUIT, savsignal);

	return (stdscr);
}

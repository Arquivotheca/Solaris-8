/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)flushinp.c	1.7	97/06/25 SMI"	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/

#include	<unistd.h>
#include	<sys/types.h>
#include	"curses_inc.h"

int
flushinp(void)
{
#ifdef	DEBUG
	if (outf)
		fprintf(outf, "flushinp(), file %x, SP %x\n",
		    cur_term->Filedes, SP);
#endif	/* DEBUG */

#ifdef	SYSV
	(void) ioctl(cur_term -> Filedes, TCFLSH, 0);
#else	/* SYSV */
	/* for insurance against someone using their own buffer: */
	(void) ioctl(cur_term -> Filedes, TIOCGETP, &(PROGTTY));

	/*
	* SETP waits on output and flushes input as side effect.
	* Really want an ioctl like TCFLSH but Berkeley doesn't have one.
	*/
	(void) ioctl(cur_term -> Filedes, TIOCSETP, &(PROGTTY));
#endif	/* SYSV */

	/*
	* Get rid of any typeahead which was read().
	* Leave characters which were ungetch()'d.
	*/
	cur_term->_chars_on_queue = cur_term->_ungotten;

	/*
	* Have to doupdate() because, if we have stopped output due to
	* typeahead, now that typeahead is gone, so we had better catch up.
	*/
	if (_INPUTPENDING)
		(void) doupdate();
	return (OK);
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)draino.c	1.9	97/08/26 SMI"	/* SVr4.0 1.7	*/

/*LINTLIBRARY*/

#include	<unistd.h>
#include	<sys/types.h>
#include	"curses_inc.h"

/*
 * Code for various kinds of delays.  Most of this is nonportable and
 * requires various enhancements to the operating system, so it won't
 * work on all systems.  It is included in curses to provide a portable
 * interface, and so curses itself can use it for function keys.
 */

#define	NAPINTERVAL	100
/*
 * Wait until the output has drained enough that it will only take
 * ms more milliseconds to drain completely.
 * Needs Berkeley TIOCOUTQ ioctl.  Returns ERR if impossible.
 */

int
draino(int ms)
{
#ifdef	TIOCOUTQ
#define	_DRAINO
	/* number of chars = that many ms */
	long	ncneeded;

	/* 10 bits/char, 1000 ms/sec, baudrate in bits/sec */
	ncneeded = SP->baud * ms / (10 * 1000);
	/*CONSTCOND*/
	while (TRUE) {
		int	rv;		/* ioctl return value */
		int	ncthere = 0;	/* number of chars actually in */
					/* output queue */

		rv = ioctl(cur_term->Filedes, TIOCOUTQ, &ncthere);
#ifdef	DEBUG
		if (outf)
			fprintf(outf, "draino: rv %d, ncneeded %d, "
			    "ncthere %d\n", rv, ncneeded, ncthere);
#endif	/* DEBUG */
		if (rv < 0)
			return (ERR);	/* ioctl didn't work */
		if (ncthere <= ncneeded)
			return (OK);
		(void) napms(NAPINTERVAL);
	}
#else	/* TIOCOUTQ */

#ifdef	TCSETAW
#define	_DRAINO
	/*
	 * SYSV simulation - waits until the entire queue is empty,
	 * then sets the state to what it already is (e.g. no-op).
	 * Unfortunately this only works if ms is zero.
	 */
	if (ms <= 0) {
#ifdef SYSV
		if (prog_istermios < 0) {
			int i;

			PROGTTY.c_lflag = PROGTTYS.c_lflag;
			PROGTTY.c_oflag = PROGTTYS.c_oflag;
			PROGTTY.c_iflag = PROGTTYS.c_iflag;
			PROGTTY.c_cflag = PROGTTYS.c_cflag;
			for (i = 0; i < NCC; i++)
				PROGTTY.c_cc[i] = PROGTTYS.c_cc[i];
			(void) ioctl(cur_term->Filedes, TCSETAW, &PROGTTY);
		} else
			(void) ioctl(cur_term->Filedes, TCSETSW, &PROGTTYS);
#else	/* SYSV */
			(void) ioctl(cur_term->Filedes, TCSETAW, &PROGTTY);
#endif	/* SYSV */
		return (OK);
	} else
		return (ERR);
#endif	/* TCSETAW */
#endif	/* TIOCOUTQ */

#ifndef	_DRAINO
	/*
	* No way to fake it, so we return failure.
	* Used #else to avoid warning from compiler about unreached stmt
	*/
	return (ERR);
#endif	/* _DRAINO */
/*NOTREACHED*/
}

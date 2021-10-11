/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ssignal.c	1.9	96/12/03 SMI"	/* SVr4.0 1.10	*/

/*LINTLIBRARY*/

/*
 *	ssignal, gsignal: software signals
 */
#pragma weak gsignal = _gsignal
#pragma weak ssignal = _ssignal

#include "synonyms.h"
#include <sys/types.h>
#include <signal.h>

/* Highest allowable user signal number */
#define	MAXSIGNUM	17

/* Lowest allowable signal number (lowest user number is always 1) */
#define	MINSIG	(-4)

/* Table of signal values */
static int (*sigs[MAXSIGNUM-MINSIG+1])(int);

int (*
ssignal(int sig, int (*action)(int)))(int)
{
	int (*savefn)(int);

	if (sig >= MINSIG && sig <= MAXSIGNUM) {
		savefn = sigs[sig-MINSIG];
		sigs[sig-MINSIG] = action;
	} else
		savefn = (int(*)(int))SIG_DFL;

	return (savefn);
}

int
gsignal(int sig)
{
	int (*sigfn)(int);

	if (sig < MINSIG || sig > MAXSIGNUM ||
	    (sigfn = sigs[sig-MINSIG]) == (int(*)(int))SIG_DFL)
		return (0);
	else if (sigfn == (int(*)(int))SIG_IGN)
		return (1);
	else {
		sigs[sig-MINSIG] = (int(*)(int))SIG_DFL;
		return ((*sigfn)(sig));
	}
}

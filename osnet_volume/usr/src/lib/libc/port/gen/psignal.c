/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)psignal.c	1.15	96/12/03 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

/*
 * Print the name of the signal indicated by "sig", along with the
 * supplied message
 */

#pragma weak psignal = _psignal

#include	"synonyms.h"
#include	"_libc_gettext.h"
#include	<sys/types.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<string.h>
#include	<signal.h>
#include	<siginfo.h>

#define	strsignal(i)	(_libc_gettext(_sys_siglistp[i]))

void
psignal(int sig, const char *s)
{
	char *c;
	size_t n;
	char buf[256];

	if (sig < 0 || sig >= NSIG)
		sig = 0;
	c = strsignal(sig);
	n = strlen(s);
	if (n) {
		(void) sprintf(buf, "%s: %s\n", s, c);
	} else {
		(void) sprintf(buf, "%s\n", c);
	}
	(void) write(2, buf, strlen(buf));
}

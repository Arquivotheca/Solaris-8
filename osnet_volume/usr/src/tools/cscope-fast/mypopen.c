/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mypopen.c	1.1	99/01/11 SMI"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include "global.h"	/* pid_t, SIGTYPE, shell, and basename() */

#define	tst(a, b) (*mode == 'r'? (b) : (a))
#define	RDR	0
#define	WTR	1

static pid_t popen_pid[20];
static SIGTYPE (*tstat)();

FILE *
mypopen(char *cmd, char *mode)
{
	int	p[2];
	pid_t *poptr;
	int myside, yourside;
	pid_t pid;

	if (pipe(p) < 0)
		return (NULL);
	myside = tst(p[WTR], p[RDR]);
	yourside = tst(p[RDR], p[WTR]);
	if ((pid = fork()) > 0) {
		tstat = signal(SIGTSTP, SIG_DFL);
	} else if (pid == 0) {
		/* myside and yourside reverse roles in child */
		int	stdio;

		/* close all pipes from other popen's */
		for (poptr = popen_pid; poptr < popen_pid+20; poptr++) {
			if (*poptr)
				(void) close(poptr - popen_pid);
		}
		stdio = tst(0, 1);
		(void) close(myside);
		(void) close(stdio);
		(void) fcntl(yourside, F_DUPFD, stdio);
		(void) close(yourside);
		(void) execlp(shell, basename(shell), "-c", cmd, 0);
		_exit(1);
	}
	if (pid == -1)
		return (NULL);
	popen_pid[myside] = pid;
	(void) close(yourside);
	return (fdopen(myside, mode));
}

int
mypclose(FILE *ptr)
{
	int f;
	pid_t r;
	int status;
	SIGTYPE (*hstat)(), (*istat)(), (*qstat)();

	f = fileno(ptr);
	(void) fclose(ptr);
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);
	hstat = signal(SIGHUP, SIG_IGN);
	while ((r = wait(&status)) != popen_pid[f] && r != -1) {
	}
	if (r == -1)
		status = -1;
	(void) signal(SIGINT, istat);
	(void) signal(SIGQUIT, qstat);
	(void) signal(SIGHUP, hstat);
	(void) signal(SIGTSTP, tstat);
	/* mark this pipe closed */
	popen_pid[f] = 0;
	return (status);
}

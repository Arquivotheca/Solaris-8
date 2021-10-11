/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)popenvp.c	1.5	99/03/09 SMI" 	/* SVr4.0 1.3	*/
/*LINTLIBRARY*/

/*
    These routines are based on the standard UNIX stdio popen/pclose
    routines. This version takes an argv[][] argument instead of a string
    to be passed to the shell. The routine execvp() is used to call the
    program, hence the name popenvp() and the argument types.

    This routine avoids an extra shell completely, along with not having
    to worry about quoting conventions in strings that have spaces,
    quotes, etc.
*/

#include "synonyms.h"
#include <sys/types.h>
#include "libmail.h"
#include <sys/wait.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define	tst(a, b) (*mode == 'r'? (b) : (a))
#define	RDR	0
#define	WTR	1

#include <unistd.h>
static pid_t popen_pid[20];

FILE *
popenvp(char *file, char **argv, char *mode, int resetid)
{
	int	p[2];
	int myside, yourside;
	pid_t pid;

	if (pipe(p) < 0)
		return (NULL);
	myside = tst(p[WTR], p[RDR]);
	yourside = tst(p[RDR], p[WTR]);
	if ((pid = fork()) == 0) {
		/* myside and yourside reverse roles in child */
		int	stdio;

		if (resetid) {
			(void) setgid(getgid());
			(void) setuid(getuid());
		}
		stdio = tst(0, 1);
		(void) close(myside);
		(void) close(stdio);
		(void) fcntl(yourside, F_DUPFD, stdio);
		(void) close(yourside);
		(void) execvp(file, argv);
		(void) fprintf(stderr, "exec failed. errno=%d.\n", errno);
		(void) fflush(stderr);
		_exit(1);
	}
	if (pid == (pid_t) -1)
		return (NULL);
	popen_pid[myside] = pid;
	(void) close(yourside);
	return (fdopen(myside, mode));
}

int
pclosevp(FILE *ptr)
{
	int f;
	pid_t r;
	int status;
	void (*hstat)(int), (*istat)(int), (*qstat)(int);

	f = fileno(ptr);
	(void) fclose(ptr);
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);
	hstat = signal(SIGHUP, SIG_IGN);
	do
		r = wait(&status);
	while (r != popen_pid[f] && r != (pid_t) -1);

	if (r == (pid_t) -1)
		status = -1;
	(void) signal(SIGINT, istat);
	(void) signal(SIGQUIT, qstat);
	(void) signal(SIGHUP, hstat);
	return (status);
}

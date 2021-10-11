/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
    These routines are based on the standard UNIX stdio popen/pclose
    routines. This version takes an argv[][] argument instead of a string
    to be passed to the shell. The routine execvp() is used to call the
    program, hence the name popenvp() and the argument types.

    This routine avoids an extra shell completely, along with not having
    to worry about quoting conventions in strings that have spaces,
    quotes, etc.
*/

#pragma ident	"@(#)systemvp.c	1.5	99/03/09 SMI" 	/* SVr4.0 1.4	*/
/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include "libmail.h"
#include <signal.h>
#include <unistd.h>
#include <wait.h>

pid_t
systemvp(char *file, char **argv, int resetid)
{
	int	status;
	pid_t	pid, w;
	void (*istat)(int), (*qstat)(int);

	if ((pid = fork()) == 0) {
		if (resetid) {
			(void) setgid(getgid());
			(void) setuid(getuid());
		}
		(void) execvp(file, argv);
		_exit(127);
	}
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);
	do
		w = wait(&status);
	while (w != pid && w != (pid_t)-1);
	(void) signal(SIGINT, istat);
	(void) signal(SIGQUIT, qstat);
	return ((w == (pid_t)-1)? w: (pid_t)status);
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)p2open.c 1.11     98/03/18 SMI"        /* SVr4.0 1.5.8.1 */

/*LINTLIBRARY*/

#pragma weak p2open = _p2open
#pragma weak p2close = _p2close

/*
 * Similar to popen(3S) but with pipe to cmd's stdin and from stdout.
 */

#include "synonyms.h"
#include <sys/types.h>
#include <libgen.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "lib_gen.h"
#include "libc.h"

int
p2open(const char *cmd, FILE *fp[2])
{
	int	fds[2];

	if (__p2open(cmd, fds) == -1)
		return(-1);

	fp[0] = fdopen(fds[0], "w");
	fp[1] = fdopen(fds[1], "r");
	return(0);
}

int
p2close(FILE *fp[2])
{
	int	status;
	int	fds[2];

	fds[0] = fileno(fp[0]);
	fds[1] = fileno(fp[1]);

	status = __p2close(fds);

	(void) fclose(fp[0]);
	(void) fclose(fp[1]);

	return(status);
}

int
__p2open(const char *cmd, int fds[2])
{
	int	tocmd[2];
	int	fromcmd[2];
	pid_t	pid;

	if (pipe(tocmd) < 0 || pipe(fromcmd) < 0)
		return (-1);
#ifndef _LP64
	if (tocmd[1] >= 256 || fromcmd[0] >= 256) {
		(void) close(tocmd[0]);
		(void) close(tocmd[1]);
		(void) close(fromcmd[0]);
		(void) close(fromcmd[1]);
		return (-1);
	}
#endif	/*	_LP64	*/
	if ((pid = fork()) == 0) {
		(void) close(tocmd[1]);
		(void) close(0);
		(void) fcntl(tocmd[0], F_DUPFD, 0);
		(void) close(tocmd[0]);
		(void) close(fromcmd[0]);
		(void) close(1);
		(void) fcntl(fromcmd[1], F_DUPFD, 1);
		(void) close(fromcmd[1]);
		(void) execl("/bin/sh", "sh", "-c", cmd, 0);
		_exit(1);
	}
	if (pid == (pid_t)-1)
		return (-1);
	(void) _insert(pid, tocmd[1]);
	(void) _insert(pid, fromcmd[0]);
	(void) close(tocmd[0]);
	(void) close(fromcmd[1]);
	fds[0] = tocmd[1];
	fds[1] = fromcmd[0];
	return (0);
}

int
__p2close(int fds[2])
{
	int		status;
	void		(*hstat)(int),
			(*istat)(int),
			(*qstat)(int);
	pid_t pid, r;

	pid = _delete(fds[0]);
	if (pid != _delete(fds[1]))
		return (-1);

	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);
	hstat = signal(SIGHUP, SIG_IGN);
	while ((r = waitpid(pid, &status, 0)) == (pid_t)-1 && errno == EINTR)
		;
	if (r == (pid_t)-1)
		status = -1;
	(void) signal(SIGINT, istat);
	(void) signal(SIGQUIT, qstat);
	(void) signal(SIGHUP, hstat);
	return (status);
}

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*LINTLIBRARY*/
#ident	"@(#)pkgexecl.c	1.17	95/02/15 SMI"	/* SVr4.0  1.8.2.1	*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <wait.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include "pkglocale.h"
#include <pkglib.h>

extern char	**environ;

#define	ERR_FREOPEN	"freopen(%s, \"%s\", %s) failed, errno=%d"
#define	ERR_FDOPEN	"fdopen(%d, \"%s\") failed, errno=%d"
#define	ERR_CLOSE	"close(%d) failed, errno=%d"
#define	ERR_SETGID	"setgid(%d) failed."
#define	ERR_SETUID	"setuid(%d) failed."
#define	ERR_EX_FAIL	"exec of %s failed, errno=%d"

#define	MAXARGS	16

/*VARARGS5*/
int
pkgexecl(char *filein, char *fileout, char *uname, char *gname, ...)
{
	va_list		ap;
	char		*pt, *arg[MAXARGS+1];
	int		n, status;
	pid_t		pid;
	struct passwd	*pwp;
	struct group	*grp;
	void		(*func)();

	va_start(ap, gname);

	n = 0;
	while (pt = va_arg(ap, char *)) {
		arg[n++] = pt;
	}
	arg[n] = NULL;
	va_end(ap);

	pid = fork();
	if (pid < 0) {
		progerr(pkg_gt("bad fork(), errno=%d"), errno);
		return (-1);
	} else if (pid) {
		int stopped_by_sig;
		int sig_no = 0;
		int exit_no = 0;

		/* parent process */
		func = signal(SIGINT, SIG_IGN);
		n = waitpid(pid, &status, 0);

		if (n != pid) {
			progerr(
			    pkg_gt("wait for %d failed, pid=%d errno=%d"),
			    pid, n, errno);
			return (-1);
		}

		if (stopped_by_sig = WIFSIGNALED(status))
			sig_no = WTERMSIG(status);

		if (WIFEXITED(status))
			exit_no = WEXITSTATUS(status);

		/*
		 * The SIGXFSZ signal is a special case with a special signal
		 * handler.
		 */
		if (stopped_by_sig && (sig_no == SIGXFSZ))
			kill(getpid(), SIGXFSZ);

		(void) signal(SIGINT, func);
		return (stopped_by_sig ? (-1) : exit_no);
	}
	/* child */
	/*
	 * The caller wants to have stdin connected to filein.
	 */
	if (filein && *filein) {
		/*
		 * If input is supposed to be connected to /dev/tty
		 */
		if (strncmp(filein, "/dev/tty", 8) == 0) {
			/*
			 * If stdin is connected to a tty device.
			 */
			if (isatty(fileno(stdin))) {
				/*
				 * Reopen it to /dev/tty.
				 */
				if (freopen(filein, "r", stdin) == NULL) {
					progerr(pkg_gt(ERR_FREOPEN), filein,
					    "r", "stdin", errno);
					_exit(-1);
				}
			}
		} else {
			/*
			 * If we did not want to be connected to /dev/tty, we
			 * connect input to the requested file no questions.
			 */
			if (freopen(filein, "r", stdin) == NULL) {
				progerr(pkg_gt(ERR_FREOPEN), filein, "r",
				    "stdin", errno);
				_exit(-1);
			}
		}
	}
	/*
	 * The caller wants to have stdout and stderr connected to fileout.
	 */
	if (fileout && *fileout) {
		/*
		 * If output is supposed to be connected to /dev/tty
		 */
		if (strncmp(fileout, "/dev/tty", 8) == 0) {
			/*
			 * If stdout is connected to a tty device.
			 */
			if (isatty(fileno(stdout))) {
				/*
				 * Reopen it to /dev/tty.
				 */
				if (freopen(fileout, "a", stdout) == NULL) {
					progerr(pkg_gt(ERR_FREOPEN), fileout,
					    "a", "stdout", errno);
					_exit(-1);
				}
			}
		} else {
			/*
			 * If we did not want to be connected to /dev/tty, we
			 * connect output to the requested file no questions.
			 */
			if (freopen(fileout, "a", stdout) == NULL) {
				progerr(pkg_gt(ERR_FREOPEN), fileout, "a",
				    "stdout", errno);
				_exit(-1);
			}
		}
		/*
		 * Dup stderr from stdout.
		 */
		n = fileno(stderr);
		if (close(n) == -1) {
			progerr(pkg_gt(ERR_CLOSE), n, errno);
			_exit(-1);
		}
		n = fileno(stdout);
		if (fdopen(dup(n), "a") == NULL) {
			progerr(pkg_gt(ERR_FDOPEN), n, "a",
			    errno);
			_exit(-1);
		}
	}
	if (gname && *gname && (grp = cgrnam(gname)) != NULL) {
		if (setgid(grp->gr_gid) == -1)
			progerr(pkg_gt(ERR_SETGID), grp->gr_gid);
	}

	if (uname && *uname && (pwp = cpwnam(uname)) != NULL) {
		if (setuid(pwp->pw_uid) == -1)
			progerr(pkg_gt(ERR_SETUID), pwp->pw_uid);
	}

	/* Now execute the program with arguments */
	(void) execve(arg[0], arg, environ);

	progerr(pkg_gt(ERR_EX_FAIL), arg[0], errno);
	_exit(99);	/* exit and leave the parent's stack frame. */
	/*NOTREACHED*/
}

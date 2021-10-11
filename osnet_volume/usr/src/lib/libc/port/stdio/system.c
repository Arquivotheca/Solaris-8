/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)system.c	1.26	98/08/27 SMI"	/* SVr4.0 1.21	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include "mtlib.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <memory.h>
#include <thread.h>
#include <errno.h>
#include <synch.h>

#pragma	weak	_cancelon
#pragma	weak	_canceloff

extern void _cancelon(void);
extern void _canceloff(void);
extern int _libc_sigprocmask(int, const sigset_t *, sigset_t *);

extern	int __xpg4;	/* defined in _xpg4.c; 0 if not xpg4-compiled program */

int
system(const char *s)
{
	int	status, pid, w;
	struct sigaction cbuf, ibuf, qbuf, ignore, dfl;
	sigset_t mask;
	sigset_t savemask;
	struct stat64 buf;
	char *shpath, *shell;
	int rc;

	if (_cancelon != NULL)
		_cancelon();

	if (__xpg4 == 0) {	/* not XPG4 */
		shpath = "/bin/sh";
		shell = "sh";
	} else {
		/* XPG4 */
		shpath = "/bin/ksh";
		shell = "ksh";
	}
	if (s == NULL) {
		if (stat64(shpath, &buf) != 0) {
			rc = 0;
			goto out;
		} else if (getuid() == buf.st_uid) {
			/* exec for user */
			if ((buf.st_mode & 0100) == 0) {
				rc = 0;
				goto out;
			}
		} else if (getgid() == buf.st_gid) {
			/* exec for group */
			if ((buf.st_mode & 0010) == 0) {
				rc = 0;
				goto out;
			}
		} else if ((buf.st_mode & 0001) == 0) {	/* exec for others */
			rc = 0;
			goto out;
		}
		rc = 1;
		goto out;
	}

	/*
	 * First we have to block SIGCHLD so that we don't cause
	 * the caller's signal handler, if any, to be called.
	 * Then we have to setup a SIG_DFL handler for SIGCHLD
	 * in case the caller is ignoring SIGCHLD, which would
	 * cause us to fail with ECHILD rather than returning
	 * the status of the spawned-off shell as we should.
	 */
	(void) sigemptyset(&mask);
	(void) sigaddset(&mask, SIGCHLD);
	(void) sigprocmask(SIG_BLOCK, &mask, &savemask);
	(void) memset(&dfl, 0, sizeof (dfl));
	dfl.sa_handler = SIG_DFL;
	(void) sigaction(SIGCHLD, &dfl, &cbuf);

	if ((pid = vfork()) == 0) {
		/*
		 * Do not callback to libthread between here and execl() for
		 * libthread fork-saftey.
		 */
		(void) _libc_sigprocmask(SIG_SETMASK, &savemask, NULL);
		(void) execl(shpath, shell, (const char *)"-c", s, (char *)0);
		_exit(127);
	} else if (pid == -1) {
		(void) sigaction(SIGCHLD, &cbuf, NULL);
		(void) sigprocmask(SIG_SETMASK, &savemask, NULL);
		rc = -1;
		goto out;
	}

	(void) memset(&ignore, 0, sizeof (ignore));
	ignore.sa_handler = SIG_IGN;
	(void) sigaction(SIGINT, &ignore, &ibuf);
	(void) sigaction(SIGQUIT, &ignore, &qbuf);

	/*
	 * _WNOCHLD ensures that the calling process will not receive
	 * a SIGCHLD for this process after SIGCHLD is unblocked.
	 * This should be the default behavior for the wait() family,
	 * but we have to wait (and wait) for Posix to decide the matter.
	 */
	do {
		w = waitpid(pid, &status, _WNOCHLD);
	} while (w == -1 && errno == EINTR);

	(void) sigaction(SIGINT, &ibuf, NULL);
	(void) sigaction(SIGQUIT, &qbuf, NULL);

	(void) sigaction(SIGCHLD, &cbuf, NULL);
	(void) sigprocmask(SIG_SETMASK, &savemask, NULL);

	rc = ((w == -1)? w: status);
out:
	if (_cancelon != NULL)
		_canceloff();
	return (rc);

}

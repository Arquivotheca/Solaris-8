/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)pt.c	1.18	99/03/02 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

#ifndef lint
#include "synonyms.h"
#endif
#pragma weak ptsname = _ptsname
#pragma weak grantpt = _grantpt
#pragma weak unlockpt = _unlockpt

#include <mtlib.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/mkdev.h>
#include <sys/fs/ufs_fsdir.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/ptms.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <synch.h>
#include <thread.h>
#include <libc.h>
#include "tsd.h"

#define	PTSNAME "/dev/pts/"		/* slave name */
#define	PTLEN   32			/* slave name length */
#define	PTPATH  "/usr/lib/pt_chmod"    	/* setuid root program */
#define	PTPGM   "pt_chmod"		/* setuid root program */

static char sname_st[PTLEN];

static void itoa(int, char *);


/*
 *  Check that fd argument is a file descriptor of an opened master.
 *  Do this by sending an ISPTM ioctl message down stream. Ioctl()
 *  will fail if:(1) fd is not a valid file descriptor.(2) the file
 *  represented by fd does not understand ISPTM(not a master device).
 *  If we have a valid master, get its minor number via fstat().
 *  Concatenate it to PTSNAME and return it as the name of the slave
 *  device.
 */
char *
ptsname(int fd)
{
	dev_t dev;
	struct stat64 status;
	struct strioctl istr;
	char *sname = (_thr_main() ? sname_st : (char *)_tsdbufalloc(_T_PTSNAME,
		    (size_t)1, sizeof (char) * PTLEN));

	istr.ic_cmd = ISPTM;
	istr.ic_len = 0;
	istr.ic_timout = 0;
	istr.ic_dp = NULL;

	if (ioctl(fd, I_STR, &istr) < 0)
		return (NULL);

	if (fstat64(fd, &status) < 0)
		return (NULL);

	dev = minor(status.st_rdev);

	(void) strcpy(sname, PTSNAME);
	itoa(dev, sname + strlen(PTSNAME));

	if (access(sname, 00) < 0)
		return (NULL);

	return (sname);
}


/*
 * Send an ioctl down to the master device requesting the
 * master/slave pair be unlocked.
 */
unlockpt(int fd)
{
	struct strioctl istr;

	istr.ic_cmd = UNLKPT;
	istr.ic_len = 0;
	istr.ic_timout = 0;
	istr.ic_dp = NULL;


	if (ioctl(fd, I_STR, &istr) < 0)
		return (-1);

	return (0);
}


/*
 * Execute a setuid root program to change the mode, ownership and
 * group of the slave device. The parent forks a child process that
 * executes the setuid program. It then waits for the child to return.
 */
static int
grantpt_u(int fd)
{
	int	st_loc;
	pid_t	pid;
	int	w;
	char	fds[PTLEN];
	sigset_t	oset, nset;

	if (sigemptyset(&nset) == -1)
		return (-1);
	if (sigaddset(&nset, SIGCHLD) == -1)
		return (-1);
	if (sigprocmask(SIG_BLOCK, &nset, &oset) == -1)
		return (-1);

	if ((pid = fork1()) == 0) {
		itoa(fd, fds);
		(void) execl(PTPATH, PTPGM, fds, (char *)0);
		/*
		 * the process should not return, unless exec() failed.
		 */
		_exit(127);
	} else if (pid == -1) {
		(void) sigprocmask(SIG_SETMASK, &oset, (sigset_t *)0);
		return (-1);
	}

	/*
	 * wait() will return the process id for the child process
	 * or -1 (on failure).  _WNOCHLD ensures that the calling
	 * process will not receive a SIGCHLD for this process
	 * after SIGCHLD is unblocked.
	 */
	while (((w = waitpid(pid, &st_loc, _WNOCHLD)) < 0) && errno == EINTR)
		continue;

	/* Restore signal mask */
	if (sigprocmask(SIG_SETMASK, &oset, (sigset_t *)0) == -1)
		return (-1);

	/*
	 * if w == -1, the child process did not fork properly.
	 */
	if (w == -1)
		return (-1);

	/*
	 * If child terminated due to exit()...
	 *	if high order bits are zero
	 *		was an exit(0).
	 *	else it was an exit(-1);
	 * Else it was struck by a signal.
	 */
	if ((st_loc & 0377) == 0)
		return (((st_loc & 0177400) == 0) ? 0 : -1);
	else
		return (-1);
}

int
grantpt(int fd)
{
	static mutex_t glk = DEFAULTMUTEX;
	int ret;

	(void) mutex_lock(&glk);
	ret = grantpt_u(fd);
	(void) mutex_unlock(&glk);
	return (ret);
}


static void
itoa(int i, char *ptr)
{
	int dig = 0;
	int tempi;

	tempi = i;
	do {
		dig++;
		tempi /= 10;
	} while (tempi);

	ptr += dig;
	*ptr = '\0';
	while (--dig >= 0) {
		*(--ptr) = i % 10 + '0';
		i /= 10;
	}
}

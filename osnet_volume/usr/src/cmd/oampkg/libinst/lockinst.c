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
#ident	"@(#)lockinst.c	1.10	96/04/05 SMI"	/* SVr4.0 1.2	*/

/*  5-20-92	added newroot functions */

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pkglocs.h>
#include <locale.h>
#include <libintl.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <pkglib.h>
#include "libadm.h"

extern int errno;

#define	ST_QUIT	1
#define	ST_OK	0

#define	LOCKFILE	".lockfile"
#define	LCKBUFSIZ	128
#define	LOCKWAIT	20	/* seconds between retries */
#define	LOCKRETRY	10	/* number of retries for a DB lock */
#define	LF_SIZE		128	/* size of governing lock file */

#define	MSG_WTING	"NOTE: Waiting for access to the package database."
#define	MSG_XWTING	"NOTE: Waiting for exclusive access to the package " \
			    "database."
#define	MSG_WTFOR	"NOTE: Waiting for %s of %s to complete."
#define	WRN_CLRLOCK	"WARNING: Stale lock installed for %s, pkg %s quit " \
			    "in %s state."
#define	WRN_CLRLOCK1	"Removing lock."
#define	ERR_MKLOCK	"unable to create governing lock file <%s>."
#define	ERR_NOLOCK	"unable to install governing lock on <%s>."
#define	ERR_NOOPEN	"unable to open <%s>."
#define	ERR_LCKTBL	"unable to lock <%s> - lock table full."
#define	ERR_LCKREM	"unable to lock <%s> - remote host unavailable."
#define	ERR_BADLCK	"unable to lock <%s> - unknown error."
#define	ERR_DEADLCK	"unable to lock <%s> - deadlock condition."

static pid_t lock_pid;
static int lock_fd, lock_is_applied;
static char lock_name[64];
static char lock_pkg[20];
static char lock_place[64];
static unsigned int lock_state;
static char lockbuf[LCKBUFSIZ];
static char lockpath[PATH_MAX];

/*
 * This function writes the PID, effective utility name, package name,
 * current progress of the utility and the exit state to the lockfile in
 * support of post mortem operations.
 */
static int
wrlockdata(int fd, int this_pid, char *this_name,
    char *this_pkg, char *this_place, unsigned int this_state)
{
	if (this_pid < 0 || *this_name == '\000')
		return (0);

	memset(lockbuf, 0, LCKBUFSIZ);

	sprintf(lockbuf, "%d %s %s %s %d\n", this_pid, this_name, this_pkg,
	    this_place, this_state);

	lseek(fd, 0, SEEK_SET);
	if (write(fd, lockbuf, LF_SIZE) == LF_SIZE)
		return (1);
	else
		return (0);
}

/*
 * This function reads the lockfile to obtain the PID and name of the lock
 * holder. Upon those rare circumstances that an older version of pkgadd
 * created the lock, this detects that zero-length file and provides the
 * appropriate data. Since this data is only used if an actual lock (from
 * lockf()) is detected, a manually created .lockfile will not result in a
 * message.
 */
static void
rdlockdata(int fd)
{
	lseek(fd, 0, SEEK_SET);
	if (read(fd, lockbuf, LF_SIZE) != LF_SIZE) {
		lock_pid = 0;
		strcpy(lock_name, "old version pkg command");
		strcpy(lock_pkg, "unknown package");
		strcpy(lock_place, "unknown");
		lock_state = ST_OK;
	} else
		sscanf(lockbuf, "%d %s %s %s %d", &lock_pid, lock_name,
		    lock_pkg, lock_place, &lock_state);
}

static void
do_alarm(int n)
{
#ifdef lint
	int i = n;
	n = i;
#endif	/* lint */
	(void) signal(SIGALRM, do_alarm);
	alarm(LOCKWAIT);
}

/*
 * This establishes a locked status file for a pkgadd, pkgrm or swmtool - any
 * of the complex package processes. Since numerous packages currently use
 * installf and removef in preinstall scripts, we can't enforce a contents
 * file write lock throughout the install process. In 2.7 we will enforce the
 * write lock and allow this lock to serve as a simple information carrier
 * which can be used by installf and removef too.
 */
int
lockinst(char *util_name, char *pkg_name)
{
	int	fd, read_cnt, retry_cnt, old_file;

	(void) sprintf(lockpath, "%s/%s", get_PKGADM(), LOCKFILE);

	/* If the exit file is not present, create it. */
	if ((fd = open(lockpath, O_RDWR | O_CREAT, 0644)) == -1) {
		progerr(gettext(ERR_MKLOCK), lockpath);
		return (0);
	}

	lock_fd = fd;

	retry_cnt = LOCKRETRY;
	lock_is_applied = 0;

	(void) signal(SIGALRM, do_alarm);
	(void) alarm(LOCKWAIT);

	/*
	 * This tries to create the lock LOCKRETRY times waiting LOCKWAIT
	 * seconds between retries.
	 */
	do {

		if (lockf(fd, F_LOCK, 0)) {
			/*
			 * Try to read the status of the last (or current)
			 * utility.
			 */
			rdlockdata(fd);

			logerr(gettext(MSG_WTFOR), lock_name, lock_pkg);
		} else {	/* This process has the lock. */
			rdlockdata(fd);

			if (lock_state != 0) {
				logerr(gettext(WRN_CLRLOCK), lock_name,
				    lock_pkg, lock_place);
				logerr(gettext(WRN_CLRLOCK1));
			}

			lock_pid = getpid();
			strcpy(lock_name, (util_name) ?
			    util_name : gettext("unknown"));
			strcpy(lock_pkg, (pkg_name) ?
			    pkg_name : gettext("unknown"));

			wrlockdata(fd, lock_pid, lock_name,
			    lock_pkg, gettext("initial"), ST_QUIT);
			lock_is_applied = 1;
			break;
		}
	} while (retry_cnt--);

	(void) signal(SIGALRM, SIG_IGN);

	if (!lock_is_applied) {
		progerr(gettext(ERR_NOLOCK), lockpath);
		return (0);
	}

	return (1);
}

/*
 * This function updates the utility progress data in the lock file. It is
 * used for post mortem operations if the utility should quit improperly.
 */
void
lockupd(char *place)
{
	wrlockdata(lock_fd, lock_pid, lock_name, lock_pkg, place, ST_QUIT);
}

/*
 * This clears the governing lock and closes the lock file. If this was
 * called already, it just returns.
 */
void
unlockinst(void)
{
	if (lock_is_applied) {
		wrlockdata(lock_fd, lock_pid, lock_name, lock_pkg,
		    gettext("finished"), ST_OK);

		/*
		 * If close() fails, we can't be sure the lock has been
		 * removed, so we assume the worst in case this function is
		 * called again.
		 */
		if (close(lock_fd) != -1)
			lock_is_applied = 0;
	}
}

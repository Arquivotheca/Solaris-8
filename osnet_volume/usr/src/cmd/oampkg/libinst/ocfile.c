/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1995,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*LINTLIBRARY*/
#pragma ident	"@(#)ocfile.c	1.18	98/12/19 SMI"	/* SVr4.0 1.4.1.1	*/

/*  5-20-92  	added newroot option  */

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <pkglocs.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>
#include "libinst.h"
#include "libadm.h"

#define	LOCKFILE	".pkg.lock"
#define	LOCKWAIT	10	/* seconds between retries */
#define	LOCKRETRY	20	/* number of retries for a DB lock */

#define	ERR_CFBACK	"Not enough space to backup <%s>"
#define	ERR_CREAT_CONT	"unable to create contents file <%s>"
#define	ERR_CFBACK1	"Need=%d blocks, Available=%d blocks (block size=%d)"
#define	ERR_NOCFILE	"unable to locate contents file <%s>"
#define	ERR_NOROPEN	"unable to open <%s> for reading"
#define	ERR_NOOPEN	"unable to open <%s> for writing"
#define	ERR_NOSTAT	"unable to stat contents file <%s>"
#define	ERR_NOSTATV	"statvfs(%s) failed"
#define	ERR_NOUPD	"unable to update contents file"
#define	ERR_DRCONTCP	"unable to copy contents file to <%s>"

#define	MSG_XWTING	"NOTE: Waiting for exclusive access to the package " \
			    "database."
#define	MSG_NOLOCK	"NOTE: Couldn't lock the package database."

#define	ERR_NOLOCK	"Database lock failed."
#define	ERR_OPLOCK	"unable to open lock file <%s>."
#define	ERR_MKLOCK	"unable to create lock file <%s>."
#define	ERR_LCKREM	"unable to lock package database - remote host " \
			    "unavailable."
#define	ERR_BADLCK	"unable to lock package database - unknown error."
#define	ERR_DEADLCK	"unable to lock package database - deadlock condition."
#define	ERR_TMOUT	"unable to lock package database - too many retries."
static char contents[PATH_MAX], t_contents[PATH_MAX];
static int active_lock;
static int lock_fd;	/* fd of LOCKFILE. */
static char *pkgadm_dir;

static int pkgWlock(int verbose);
static int pkgWunlock(void);

int relslock(void);

static void
do_alarm(int n)
{
#ifdef lint
	int i = n;
	n = i;
#endif	/* lint */

	(void) signal(SIGALRM, SIG_IGN);
	(void) signal(SIGALRM, do_alarm);
	alarm(LOCKWAIT);
}

/*
 * Point packaging to the appropriate contents file. This is primarily used
 * to establish a dryrun contents file. If the malloc() doesn't work, this
 * returns 99 (internal error), else 0.
 */
int
set_cfdir(char *cfdir)
{
	char realcf[PATH_MAX], tmpcf[PATH_MAX];
	char cmd[(sizeof ("/usr/bin/cp %s/contents %s") - 4) + PATH_MAX*2 + 1];
	int status;

	if (cfdir == NULL)
		pkgadm_dir = get_PKGADM();
	else {
		if ((pkgadm_dir = strdup(cfdir)) == NULL)
			return (99);

		sprintf(tmpcf, "%s/contents", pkgadm_dir);

		/*
		 * If there's a contents file already there, assume it's from
		 * a prior package in this series.
		 */
		if (access(tmpcf, F_OK)) {
			sprintf(realcf, "%s/contents", get_PKGADM());

			/*
			 * If there's a contents file there already, copy it
			 * over, otherwise initialize one.
			 */
			if (access(realcf, F_OK) == 0) {
				sprintf(cmd, "/usr/bin/cp %s %s",
				    realcf, pkgadm_dir);

				status = system(cmd);
				if (status == -1 || WEXITSTATUS(status)) {
					progerr(gettext(ERR_DRCONTCP),
					    pkgadm_dir);
					return (99);
				}
			} else {
				int n;

				if ((n = creat(tmpcf, 0644)) == -1) {
					progerr(gettext(ERR_CREAT_CONT), tmpcf);
					return (99);
				}
				close(n);
			}
		}
	}

	return (0);
}

/*
 * XXX This function truncates the temporary contents file. It was
 * XXX removed from the function sortmap() so as to try to get a handle
 * XXX on the contents file name space for dryrun mode. I don't see how
 * XXX the temporary contents file would ever be populated at this point.
 * XXX Further review of this issue would be a good idea since the
 * XXX question of who has access to the contents file and when is a very
 * XXX tangled subject. Since there's only one type of error here, this
 * XXX returns 0 for OK and 1 for FAIL.
 */
int
trunc_tcfile(void)
{
	(void) sprintf(t_contents, "%s/t.contents", pkgadm_dir);
	if (truncate(t_contents, 0) != 0) {
		progerr(gettext("unable to truncate temporary contents file"));
		return (0);
	}

	return (1);
}

/*
 * This function installs the database lock, opens the contents file for
 * reading and creates and opens the temporary contents file for read/write.
 * It returns 1 if successful, 0 otherwise.
 */
int
ocfile(FILE **mapfp, FILE **tmpfp, ulong map_blks)
{
	struct	stat	statb;
	struct	statvfs	svfsb;
	ulong	free_blocks;
	ulong	need_blocks;

	*mapfp = *tmpfp = NULL;

	if (pkgadm_dir == NULL)
		set_cfdir(NULL);

	/* Lock the file for exclusive access. */
	if (!pkgWlock(1)) {
		progerr(gettext(ERR_NOLOCK), contents);
		return (0);
	}

	/*
	 * We now open the contents file to read only.
	 */
	(void) sprintf(contents, "%s/contents", pkgadm_dir);
	if ((*mapfp = fopen(contents, "r")) == NULL) {
		if (errno == ENOENT) {
			progerr(gettext(ERR_NOCFILE), contents);
			logerr("(errno %d)", errno);
			return (0);
		} else {
			progerr(gettext(ERR_NOROPEN), contents);
			logerr("(errno %d)", errno);
			return (0);
		}
	}

	/*
	 * Check and see if there is enough space for the packaging commands
	 * to back up the contents file, if there is not, then do not allow
	 * execution to continue by failing the ocfile() call.
	 */

	/*
	 * Get the contents file size.
	 */
	if (stat(contents, &statb) == -1) {
		progerr(gettext(ERR_NOSTAT), contents);
		logerr("(errno %d)", errno);
		(void) fclose(*mapfp);
		*mapfp = NULL;
		return (0);
	}

	/*
	 * Get the filesystem space.
	 */
	if (statvfs(contents, &svfsb) == -1) {
		progerr(gettext(ERR_NOSTATV), contents);
		logerr("(errno %d)", errno);
		(void) fclose(*mapfp);
		*mapfp = NULL;
		return (0);
	}

	free_blocks = (((long)svfsb.f_frsize > 0) ?
		    howmany(svfsb.f_frsize, DEV_BSIZE) :
		    howmany(svfsb.f_bsize, DEV_BSIZE)) * svfsb.f_bfree;

	if (map_blks == 0L)
		map_blks = 10L;
	/*
	 * Calculate the number of blocks we need to be able to operate on
	 * the contents file.
	 */
	need_blocks = map_blks +
	    nblk(statb.st_size, svfsb.f_bsize, svfsb.f_frsize);

	if ((need_blocks + 10) > free_blocks) {
		progerr(gettext(ERR_CFBACK), contents);
		progerr(gettext(ERR_CFBACK1), need_blocks, free_blocks,
			DEV_BSIZE);
		(void) fclose(*mapfp);
		*mapfp = NULL;
		return (0);
	}

	(void) sprintf(t_contents, "%s/t.contents", pkgadm_dir);
	if ((*tmpfp = fopen(t_contents, "w")) == NULL) {
		progerr(gettext(ERR_NOOPEN), t_contents);
		logerr("(errno %d)", errno);
		(void) fclose(*mapfp);
		*mapfp = NULL;
		return (0);
	}

	return (1);	/* All OK */
}

/*
 * This is a simple open and lock of the contents file. It doesn't create a
 * temporary contents file and it doesn't need to do any space checking.
 * Returns 1 for OK and 0 for "didn't do it".
 */
int
socfile(FILE **mapfp)
{
	*mapfp = NULL;

	if (pkgadm_dir == NULL)
		set_cfdir(NULL);

	/*
	 * Lock the database for exclusive access, but don't make a fuss if
	 * it fails (user may not be root and the .pkg.lock file may not
	 * exist yet).
	 */
	if (!pkgWlock(0)) {
		logerr(gettext(MSG_NOLOCK), contents);
	}

	/*
	 * We now open the contents file to read only.
	 */
	(void) sprintf(contents, "%s/contents", pkgadm_dir);
	if ((*mapfp = fopen(contents, "r")) == NULL) {
		if (errno == ENOENT) {
			progerr(gettext(ERR_NOCFILE), contents);
			logerr("(errno %d)", errno);
			return (0);
		} else {
			progerr(gettext(ERR_NOROPEN), contents);
			logerr("(errno %d)", errno);
			return (0);
		}
	}

	return (1);
}

/*
 * This function replaces the old transitory contents file with the newly
 * updated temporary contents file. pkginst = NULL means just delete the
 * temporary contents file because nothing happened. mapfp is the file
 * pointer for the old contents file and tmpfp is that of the new contents
 * file.
 *
 * It returns RESULT_OK for all's well, RESULT_WRN for non-fatal error or
 * RESULT_ERR for fatal error that deserves an alarming message and a quit().
 */
int
swapcfile(FILE *mapfp, FILE *tmpfp, char *pkginst)
{
	char	s_contents[PATH_MAX];
	time_t	clock;
	struct	tm	*timep;
	char	timeb[BUFSIZ];
	int	retval = RESULT_OK;

	/*
	 * We won't be reading anything else from the original file, so close
	 * it up.
	 */
	if (fclose(mapfp)) {
		logerr(gettext("WARNING: unable to close <%s>"),
		    contents);
		logerr("(errno %d)", errno);
		retval = RESULT_WRN;
	}

	/*
	 * No changes were made to the database, so delete the temporary
	 * contents file and return.
	 */
	if (pkginst == NULL) {
		if (fclose(tmpfp)) {
			logerr(gettext("WARNING: unable to close <%s>"),
			    t_contents);
			logerr("(errno %d)", errno);
			retval = RESULT_WRN;
		}
		if (unlink(t_contents)) {
			logerr(gettext("WARNING: unable to close <%s>"),
			    t_contents);
			logerr("(errno %d)", errno);
			retval = RESULT_WRN;
		}
		pkgWunlock();	/* Free the database lock. */
		return (retval);
	}

	/* Replace the contents file with the temporary contents file. */
	(void) time(&clock);
	timep = localtime(&clock);

	strftime(timeb, sizeof (timeb), "%c\n", timep);
	(void) fprintf(tmpfp,
	    gettext("# Last modified by %s for %s package\n# %s"),
	    get_prog_name(), pkginst, timeb);

	if (fflush(tmpfp)) {
		progerr(gettext(ERR_NOUPD));
		logerr(gettext("fflush failed (errno %d)"), errno);
		return (RESULT_ERR);
	}

	if (fsync(fileno(tmpfp))) {
		progerr(gettext(ERR_NOUPD));
		logerr(gettext("fsync failed (errno %d)"), errno);
		return (RESULT_ERR);
	}

	/*
	 * OK, now the buffers of the file to which we've been writing are
	 * flushed and sync'd up. Everything else we do is by path, so we
	 * close the file.
	 */
	if (fclose(tmpfp)) {
		logerr(gettext("WARNING: unable to close <%s>"),
		    t_contents);
		logerr("(errno %d)", errno);
		retval = RESULT_WRN;
	}

	(void) sprintf(s_contents, "%s/s.contents", pkgadm_dir);

	/*
	 * Now we want to make a copy of the old contents file as a
	 * fail-safe. In support of that, we create a hard link to
	 * s.contents.
	 */
	if ((access(s_contents, F_OK) == 0) && unlink(s_contents)) {
		logerr(gettext("WARNING: unable to unlink latent <%s>"),
		    s_contents);
		logerr("(errno %d)", errno);
		return (RESULT_ERR);
	}

	if (link(contents, s_contents)) {
		progerr(gettext(ERR_NOUPD));
		logerr(gettext("link(%s, %s) failed (errno %d)"),
		    contents, s_contents, errno);
		return (RESULT_ERR);
	}

	if (rename(t_contents, contents)) {
		progerr(gettext("unable to establish contents file"));
		logerr(gettext("rename(%s, %s) failed (errno %d)"),
		    t_contents, contents, errno);
		if (rename(s_contents, contents)) {
			progerr(gettext("attempt to restore <%s> failed"),
			    contents);
			logerr(gettext("rename(%s, %s) failed (errno %d)"),
			    s_contents, contents, errno);
		}
	}

	if (unlink(s_contents)) {
		logerr(gettext("WARNING: unable to unlink <%s>"), s_contents);
		logerr("(errno %d)", errno);
		retval = RESULT_WRN;
	}

	if (relslock())
		return (retval);
	else
		return (RESULT_ERR);
}

/* This function releases the lock on the package database. */
int
relslock(void)
{
	/*
	 * This closes the contents file and releases the lock.
	 */
	if (!pkgWunlock()) {
		progerr(gettext(ERR_NOUPD));
		logerr(gettext("fclose failed (errno %d)"), errno);
		return (0);
	}
	return (1);
}

/*
 * This function attempts to lock the package database. It returns 1 on
 * success, 0 on failure. The positive logic verbose flag determines whether
 * or not the function displays the error message upon failure.
 */
static int
pkgWlock(int verbose) {
	int retry_cnt, retval;
	char lockpath[PATH_MAX];

	active_lock = 0;

	(void) sprintf(lockpath, "%s/%s", pkgadm_dir, LOCKFILE);

	retry_cnt = LOCKRETRY;

	/*
	 * If the lock file is not present, create it. The mode is set to
	 * allow any process to lock the database, that's because pkgchk may
	 * be run by a non-root user.
	 */
	if (access(lockpath, F_OK) == -1) {
		if ((lock_fd = open(lockpath, O_RDWR | O_CREAT, 0666)) == -1) {
			if (verbose)
				progerr(gettext(ERR_MKLOCK), lockpath);
			return (0);
		} else {
			fchmod(lock_fd, 0666);	/* force perms. */
		}
	} else {
		if ((lock_fd = open(lockpath, O_RDWR)) == -1) {
			if (verbose)
				progerr(gettext(ERR_OPLOCK), lockpath);
			return (0);
		}
	}

	(void) signal(SIGALRM, do_alarm);
	(void) alarm(LOCKWAIT);

	do {
		if (lockf(lock_fd, F_LOCK, 0)) {
			if (errno == EAGAIN || errno == EINTR)
				logerr(gettext(MSG_XWTING));
			else if (errno == ECOMM) {
				logerr(gettext(ERR_LCKREM));
				retval = 0;
				break;
			} else if (errno == EBADF) {
				logerr(gettext(ERR_BADLCK));
				retval = 0;
				break;
			} else if (errno == EDEADLK) {
				logerr(gettext(ERR_DEADLCK));
				retval = 0;
				break;
			}
		} else {
			active_lock = 1;
			retval = 1;
			break;
		}
	} while (retry_cnt--);

	(void) signal(SIGALRM, SIG_IGN);

	if (retval == 0)
	{
		if (retry_cnt == -1)
			logerr(gettext(ERR_TMOUT));

		pkgWunlock();	/* close the lockfile. */
	}

	return (retval);
}

/*
 * Release the lock on the package database. Returns 1 on success, 0 on
 * failure.
 */
static int
pkgWunlock(void) {
	if (active_lock) {
		active_lock = 0;
		if (close(lock_fd))
			return (0);
		else
			return (1);
	} else
		return (1);
}

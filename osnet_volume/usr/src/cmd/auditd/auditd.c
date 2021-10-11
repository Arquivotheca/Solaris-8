#ifndef lint
static char sccsid[] = "@(#)auditd.c 97/12/15 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

/* Audit daemon server */
/*
 * These routines make up the audit daemon server.  This daemon, called
 * auditd, handles the user level parts of auditing.  For the most part
 * auditd will be in the audit_svc system call; otherwise, it is handling
 * the interrupts and errors from the system call and handling the audit
 * log files.
 *
 * The major interrupts are SIGHUP (start over), SIGTERM (start shutting
 * down), SIGALRM (quit), and SIGUSR1 (start a new audit log file).
 *
 * The major errors are EBUSY (auditing is already in use), EINVAL (someone
 * has disabled auditing), EINTR (one of the above signals was received),
 * ENOSPC (the directory is out of space), and EFBIG (file size limt exceeded).
 * All other errors are treated the same as ENOSPC.
 *
 * Much of the work of the audit daemon is taking care of when the
 * directories fill up.  In the audit_control file, there is a value
 * min_free which determines a "soft limit" for what percentage of space
 * to reserve on the file systems.  This code deals with the cases where
 * one file systems is at this limit (soft), all the file systems are at
 * this limit (allsoft), one of the file systems is completely full (hard),
 * and all of the file systems are completely full (allhard).  The
 * audit daemon warns the administrator about these and other problems
 * using the auditwarn shell script.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <bsm/audit.h>
#include <bsm/audit_record.h>
#include <bsm/libbsm.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <pwd.h>
#include <signal.h>
#include <tzfile.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <termios.h>
#include <string.h>
#include <locale.h>
#include <libintl.h>
#include "flock.h"

/*
 * DEFINES:
 */

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SUNW_OST_OSCMD"
#endif

#define	MACH_NAME_SZ	64
#define	AUDIT_DATE_SZ	14
#define	AUDIT_FNAME_SZ	2 * AUDIT_DATE_SZ + 2 + MACH_NAME_SZ
#define	AUDIT_BAK_SZ	50	/* size of name of audit_data back-up file */

#define	SOFT_LIMIT	0	/* used by the limit flag */
#define	HARD_LIMIT	1

#define	SOFT_SPACE	0	/* used in the dirlist_s structure for	*/
#define	HARD_SPACE	1	/*   how much space there is in the	*/
#define	SPACE_FULL	2	/*   filesystem				*/
#define	STAY_FULL	3

#define	AVAIL_MIN	50	/* If there are less that this number	*/
/* of blocks avail, the filesystem is	*/
/* presumed full.  May need tweeking.	*/

/*
 * After we get a SIGTERM, we want to set a timer for 2 seconds and
 * let the auditsvc syscall write as many records as it can until the tim
er
 * goes off(at which point it returns to auditd with SIGALRM).
 * If any other signals are received during that time, we call dowarn()
 * to indicate that the queue may not have been fully flushed.
 */
#define	ALRM_TIME	2

#define	SLEEP_TIME	20	/* # of seconds to sleep in all hard loop */

extern int	gettimeofday();
extern int	gethostname();
extern int	errno;

#ifdef DEBUG
#define	dprintf	fprintf
#endif /* DEBUG */

/* DATA STRUCTURES: */

/*
 * The directory list is a circular linked list.  It is pointed into by
 * startdir and thisdir.  Each element contains the pointer to the next
 * element, the directory pathname and a flag for how much space there is
 * in the directory's filesystem.
 */
struct dirlist_s {
	struct dirlist_s *dl_next;
	int	dl_space;
	char	*dl_name;
};
typedef struct dirlist_s dirlist_t;

/*
 * The alogfile contains a file descriptor, a directory pathname,
 * and a filename.
 */
struct alogfile_s {
	int	l_fd;
	char	*l_name;
};
typedef struct alogfile_s alogfile_t;

/*
 * GLOBALS:
 *	startdir	pointer to the "start" of the dir list
 *	thisdir		pointer into dir list for the current directory
 *	alogfile		points to struct with info of current log file
 *	force_close	true if the user explicitly asked to close the log file
 *	minfree		the lower bound percentage read from audit_control
 *	minfreeblocks	tells audit_svc what the soft limit is in blocks
 *	limit		whether we are using soft or hard limits
 *	errno		error returned by audit_svc (and other syscalls)
 *	hung_count	count for how many times sent message for all hard
 *	opened		whether the open was successful
 *	the files	these are the five files auditing uses in addition
 *				to the audit log files
 */

static dirlist_t *startdir = NULL;
static dirlist_t *thisdir = NULL;
static alogfile_t *alogfile = NULL;

static int	force_close;
static int	minfree;
static int	minfreeblocks;
static int	limit = SOFT_LIMIT;
static int	hung_count;
static int	opened = 1;
static char	auditdata[] = AUDITDATAFILE;
static char	auditwarn[] = "/etc/security/audit_warn";
static int	audit_data_fd; /* file descriptor of audit_data */

static int	caught_alrm;	/* number of SIGALRMs pending */
static int	caught_hup;	/* number of SIGHUPs pending */
static int	caught_term;	/* number of SIGTERMs pending */
static int	caught_usr1;	/* number of SIGUSR1s pending */

static int	turn_audit_on  = AUC_AUDITING;
static int	turn_audit_off = AUC_NOAUDIT;

static int	close_log();
static int	do_auditing();
static void	dowarn();
static void	catch_alrm();
static void	catch_hup();
static void	catch_term();
static void	catch_usr1();
static void	getauditdate();
static int	handleallhard();
static int	loadauditlist();
static void	logpost();
static int	my_sleep();
static int	open_log();
static int	process_audit();
static void	sigsetmymask();
static int	testspace();
static int	write_file_token();

/*
 * SIGNAL HANDLERS
 */

/* ARGSUSED */
static void
catch_alrm(sig)
{
	sigsetmymask();
	caught_alrm++;
#ifdef DEBUG
	dprintf(stderr, "catch_alarm()\n");
#endif
}


/* ARGSUSED */
static void
catch_hup(sig)
{

	sigsetmymask();
	caught_hup++;
#ifdef DEBUG
	dprintf(stderr, "catch_hup()\n");
#endif
}


/* ARGSUSED */
static void
catch_term(sig)
{
	sigsetmymask();
	caught_term++;
#ifdef DEBUG
	dprintf(stderr, "catch_term()\n");
#endif
}


/* ARGSUSED */
static void
catch_usr1(sig)
{
	sigsetmymask();
	caught_usr1++;
#ifdef DEBUG
	dprintf(stder, "catch_usr1()\n");
#endif
}


static void
sigsetmymask()
{
	static sigset_t set;
	static called_once = 0;

	if (called_once) {
		(void) sigprocmask(SIG_SETMASK, &set, (sigset_t *)NULL);
	} else {
		(void) sigfillset(&set);
		(void) sigdelset(&set, SIGALRM);
		(void) sigdelset(&set, SIGTERM);
		(void) sigdelset(&set, SIGHUP);
		(void) sigdelset(&set, SIGUSR1);
#ifdef DEBUG
		(void) sigdelset(&set, SIGINT);
#endif
		called_once++;
	}
}


/* ARGSUSED */
main(argc, argv)
int	argc;
char	*argv[];
{
	struct sigaction sv;	/* used to set up signal handling */
	int fd;			/* used to remove the control tty */
	int reset_list;		/* 1 if user wants to re-read audit_control */
	auditinfo_t as_null;	/* audit state to set */
	au_id_t auid;

	/* Internationalization */
	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);
	/*
	 * Only allow super-user to invoke this program
	 */
	if (getuid() != 0) {
		(void) fprintf(stderr, gettext("auditd: Not super-user.\n"));
		exit(1);
	}

	/*
	 * Turn off all auditing for this process. Set the audit user id
	 * to the effective user id (should be "audit") so that a change
	 * imposed by setuseraudit(2) to "root" will not effect this
	 * process.
	 */
	getaudit(&as_null);
	as_null.ai_mask.as_success = 0;
	as_null.ai_mask.as_failure = 0;
	(void) setaudit(&as_null);
	auid = geteuid();
	(void) setauid(&auid);

#ifndef	NO_BACK
	/*
		 * Set the process group to something safe
		 */
	(void) setpgrp();
#endif	NO_BACK
	/*
	 * Remove the control tty
	 */
	if ((fd = open("/dev/tty", 0, O_RDWR)) > 0) {
		(void) ioctl(fd, TIOCNOTTY, 0);
		(void) close(fd);
	}
	/*
	 * Set the audit state flag to AUDITING.
	 */
	if (auditon(A_SETCOND, (caddr_t) & turn_audit_on, (int)sizeof (int)) <
	    0) {
		dowarn("nostart", "", 0);
		exit(0);
	}

#ifndef	NO_BACK
	/*
	 * Fork child process and abandon it (it gets inherited by init)
	 */
	if (fork())
		exit(0);
#endif	NO_BACK

#ifdef	DEBUG
	dprintf(stderr, "auditd: euid is: %d ruid is %d\n",
	    geteuid(), getuid());

#endif	/* DEBUG */

	/*
	 * Set up signal handling.
	 */
	sigsetmymask();
	(void) sigfillset(&sv.sa_mask);

	/*
	 * SV_INTERRUPT is no longer supported.
	 */

	/* sv.sa_flags = SV_INTERRUPT; */

	sv.sa_flags = 0;

	sv.sa_handler = catch_alrm;
	(void) sigaction(SIGALRM, &sv, (struct sigaction *)0);

	sv.sa_handler = catch_hup;
	(void) sigaction(SIGHUP, &sv, (struct sigaction *)0);

	sv.sa_handler = catch_term;
	(void) sigaction(SIGTERM, &sv, (struct sigaction *)0);

	sv.sa_handler = catch_usr1;
	(void) sigaction(SIGUSR1, &sv, (struct sigaction *)0);

	/*
	 * Set the umask so that only audit or other users in the audit group
	 * can get to the files created by auditd.
	 */
	(void) umask(007);
	/*
		 * Open the audit_data file. Use O_APPEND so that the contents
		 * are not destroyed if there is another auditd running.
		 */
	if ((audit_data_fd = open(auditdata,
	    O_RDWR | O_APPEND | O_CREAT, 0660)) < 0) {
		dowarn("tmpfile", "", 0);
		exit(1);
	}
	if (flock(audit_data_fd, LOCK_EX | LOCK_NB) < 0) {
		dowarn("ebusy", "", 0);
		exit(1);
	}

	/*
		 * Here is the main body of the audit daemon.
		 */
	/* CONSTANTCONDITION */
	while (1) {
		/*
		 * Read in the directory list.
		 * Sets thisdir to the first dir.
		 */
		if (loadauditlist() != 0) {
			exit(2);
		}

		force_close = 0;
		hung_count = 0;
		limit = HARD_LIMIT;

		/*
		 * Call process_audit to do the auditing and the handling
		 * of space overflows until reset_list is true which
		 * means the user wants to reread the audit_control file.
		 */
		reset_list = 0;
		while (!reset_list)
			(void) process_audit(&reset_list);

	}
}



/*
 * process_audit - handle auditing and space overflows
 */
static int
process_audit(reset_list)
int	*reset_list;
{
	int audit_break = 0; /* whether we were broken out of do_auditing() */
	/*
	 * allhard determines whether all of the filesystems are full.
	 * It is set to 1 (true) until a filesystem is found which has
	 * space, at which point it is set to 0.
	 */
	int	allhard = 1;
	int	firsttime;	/* used when searching for hard space */

#ifdef	DEBUG
	dprintf(stderr, "auditd: inside process_audit\n");

#endif	/* DEBUG */
	startdir = thisdir;

	do {
		/*
		 * test whether thisdir is under the soft limit
		 */
		if (testspace(thisdir, SOFT_SPACE)) {
			limit = SOFT_LIMIT;
			/*
			 * open_log calls close_log.  If the open fails, it
			 * sets thisdir to the next directory in the list
			 */
			if (open_log()) {
				/*
				 * We finally call audit_svc here.
				 */
				allhard = 0;
				(void) do_auditing(minfreeblocks, &audit_break,
					reset_list);
			}
		} else {
			if (thisdir->dl_space == HARD_SPACE)
				allhard = 0;
			/*
			 * Go to the next directory
			 */
			thisdir = thisdir->dl_next;
		}

	} while (thisdir != startdir && !(audit_break || *reset_list));

	if (*reset_list || audit_break)
		return (0);

	if (thisdir == startdir) {
		if (allhard) {
			(void) handleallhard(reset_list);
		} else {
			if (limit == SOFT_LIMIT)
				dowarn("allsoft", "", 0);

			/*
			 * Find the first directory that has
			 * hard space and open a new alogfile.
			 *
			 * Complications:
			 * Open may fail which may mean that
			 * there is no more space anywhere.
			 * Also, if they explicitly want to
			 * change files, we need to do so.
			 */
			opened = 0;
			firsttime = 1;

			while (firsttime || thisdir != startdir) {
				firsttime = 0;
				if (thisdir->dl_space == HARD_SPACE) {
					if (thisdir == startdir &&
					    alogfile != (alogfile_t *)0 &&
					    !force_close) {
						/* use the file already open */
#ifdef	DEBUG
dprintf(stderr, "auditd: using the same alogfile %s\n", thisdir->dl_name);
#endif	/* DEBUG */
						opened = 1;
						break;
					} else if (opened = open_log()) {
						break;
					} else {
#ifdef	DEBUG
dprintf(stderr, "can't open %s", thisdir->dl_name);
#endif	/* DEBUG */
						thisdir = thisdir->dl_next;
					}
				} else {
					thisdir = thisdir->dl_next;
				}
			}
			limit = HARD_LIMIT;
			if (opened) {
				minfreeblocks = 0;
				(void) do_auditing(minfreeblocks, &audit_break,
					reset_list);
			} else {
				/* allhard is true */
#ifdef	DEBUG
dprintf(stderr, "auditd: allhard because open failed\n");
#endif	/* DEBUG */
				(void) handleallhard(reset_list);
			}
		}
	}
	return (0);
}



/*
 * do_auditing - set up for calling audit_svc and handle it's various
 *	returns.
 */
static int
do_auditing(minfreeblocks, audit_break, reset_list)
int	minfreeblocks;
int	*audit_break;
int	*reset_list;

{
	int	error = 0;		/* used to save the errno */

	*audit_break = 1;
	*reset_list = 0;
	force_close = 0;	/* this is a global variable */

	if (limit == HARD_LIMIT) {
		minfreeblocks = 0;
	}

	/*
	 * Enter auditsvc iff there are no signals pending.
	 */
	if (caught_hup || caught_term || caught_alrm || caught_usr1) {
		error = EINTR;
	} else {
		auditsvc(alogfile->l_fd, minfreeblocks);
		error = errno;
#ifdef DEBUG
		dprintf(stderr, "auditsvc returns with %d\n", error);
#endif
	}

	switch (error) {
		/* FALLTHRU */
	case EBUSY:
		dowarn("ebusy", "", 0);
		exit(1);
		/* FALLTHRU */
	case EINVAL:
		/*
		 * who knows what we should do here - someone has turned
		 * auditing off unexpectedly - for now we will exit
		 */
		dowarn("auditoff", "", 0);
		exit(1);
		/* FALLTHRU */
	case EFBIG:
		/*
		 * file size limit set by auditon(A_SETFSIZE ...) exceeded
		 * switch to the next file
		 */
		force_close = 1;

	case EINTR:
		/*
		 * Got here because a signal came in.
		 * Since we may have gotten more than one, we assume a
		 * priority scheme with SIGALRM being the most
		 * significant.
		 */
		if (caught_alrm) {
			/*
			 * We have returned from our timed entrance into
			 * auditsvc().  We need to really shut down here.
			 */
			if (alogfile)
				(void) close_log(alogfile, "", "");
			/*
			 * Set audit state to NOAUDIT.
			 */
			(void) auditon(A_SETCOND, (caddr_t) & turn_audit_off,
				(int)sizeof (int));
			exit(0);
		}
		if (caught_term) {
			/*
			 * one-shot finish-up code, from which
			 * return is impossible.
			 */
			if (hung_count <= 0 && alogfile != (alogfile_t *)0) {
				/*
				 * There is a place to audit into.
				 */
				caught_alrm = 0;
				caught_hup  = 0;
				caught_term = 0;
				caught_usr1 = 0;

				(void) alarm(ALRM_TIME);
#ifdef DEBUG_AUDITSVC
				(void) sleep(60);
#else
				auditsvc(alogfile->l_fd, 0);
#endif

				if (caught_hup || caught_term || caught_usr1)
					/*
					 * We might have been interrupted by
					 * something other than the
					 * alarm we set
					 */
					dowarn("postsigterm", "", 0);
			}
			if (alogfile != (alogfile_t *)0) {
				/*
				 * terminate file properly
				 */
				(void) close_log(alogfile, "", "");
				alogfile = (alogfile_t *)0;
			}
			/*
			 * Close down auditing and exit
			 */
			auditon(A_SETCOND, (caddr_t) & turn_audit_off,
				(int)sizeof (int));

			exit(0);
			/* NOTREACHED */
		}
		if (caught_hup) {
			/*
			 * They want to reread the audit_control file.  Set
			 * reset_list which will return us to the main while
			 * loop in the main routine.
			 */
			*reset_list = 1;
			caught_hup = 0;
		}
		if (caught_usr1) {
			/*
			 * In every normal case, we could ignore this case
			 * because the normal behavior of the rest of the
			 * code is to switch to a new log file before getting
			 * back into audit_svc.  There is one exception -
			 * if all the filesystems are at the soft limit,
			 * the normal behavior is to keep using the same
			 * log file in audit_svc until this file system
			 * fills up.  Since our user is explicitly asking
			 * for a new file, we must make sure in that case
			 * that a new file is used.
			 */
			force_close = 1;
			caught_usr1 = 0;
		}
		break;
		/*
		 * EDQUOT appears to be gone in SYS V.
		 * Lets leave the code here in case we
		 * eventually need to check for a similar
		 * condition.
		 */
#if 0
	case EDQUOT:
		/*
		 * Update the structure for this directory to have the
		 * correct space value (HARD_SPACE or SPACE_FULL).  Nothing
		 * further needs to be done here, the main routine will
		 * worry about finding space (here or elsewhere).
		 */
		thisdir->dl_space =
			(limit == SOFT_LIMIT) ? HARD_SPACE : SPACE_FULL;
		break;
#endif
	case EIO:
	case ENETDOWN:
	case ENETUNREACH:
	case ECONNRESET:
	case ETIMEDOUT:
	case EHOSTDOWN:
	case EHOSTUNREACH:
	case ESTALE:
		/*
		 * If any of the errors are returned, the filesystem is
		 * unusable.  Make sure that it does not try to use it
		 * the next time that testspace is called.  After that,
		 * it will try to use it again since the problem may have
		 * been solved.
		 */
		thisdir->dl_space = STAY_FULL;
		break;
	default:
		/*
		 * Correct the space portain of the structure for this
		 * directory.  Throw away the return value from testspace.
		 */
		(void) testspace(thisdir, SPACE_FULL);
		break;
	}

	/*
	 * We treat any of the signals or errors that get us here except
	 * for SIGHUP and SIGUSR1 as if we have run out of space.
	 */
	if (!*reset_list && !force_close) {
		if (limit == SOFT_LIMIT)
			dowarn("soft", alogfile->l_name, 0);
			else
			dowarn("hard", alogfile->l_name, 0);
	}

	hung_count = 0;
	return (0);
}


/*
 * handleallhard - handle the calling of dowarn, the sleeping, etc
 *		   for the allhard case
 * globals:
 *	hung_count - how many times dowarn has been called for the
 *		allhard case.
 *	alogfile - the current alogfile to close and reinitialize.
 * arguments:
 * 	reset_list - set if SIGHUP was received in my_sleep
 */
static int
handleallhard(reset_list)
int	*reset_list;
{

	++hung_count;
	if (hung_count == 1) {
		(void) close_log(alogfile, "", "");
		alogfile = (alogfile_t *)0;
		logpost(getpid(), "");
	}
	dowarn("allhard", "", hung_count);
	(void) my_sleep(reset_list);
	return (0);
}



/*
 * my_sleep - sleep for SLEEP_TIME seconds but only accept the signals
 *	that we want to accept
 *
 * arguments:
 * 	reset_list - set if SIGHUP was received
 */
static int
my_sleep(reset_list)
int	*reset_list;
{

#ifdef	DEBUG
	dprintf(stderr, "auditd: sleeping for 20 seconds\n");
#endif	/* DEBUG */

	/*
	 * Set timer to "sleep"
	 */
	(void) alarm(SLEEP_TIME);
	(void) sigpause(SIGALRM);
	/*
	 * Handle the signal from sigpause
	 */
	if (caught_term)
		/*
		 * Exit, as requested.
		 */
		exit(1);
	if (caught_hup)
		/*
		 * Reread the audit_control file
		 */
		*reset_list = 1;
	caught_alrm = 0;
	caught_hup = 0;
	caught_usr1 = 0;
	return (0);
}

/*
 * open_log - open a new alogfile in the current directory.  If a
 *	alogfile is already open, close it.
 *
 * globals:
 * 	thisdir - used to determine where to put the new file
 *	alogfile - used to get the oldfile name (and change it),
 *		to close the oldfile and then is set to the newfile
 */
static int
open_log()
{
	char	auditdate[AUDIT_DATE_SZ+1];
	char	host[MACH_NAME_SZ+1];
	char	oldname[AUDIT_FNAME_SZ+1];
	char	*name;			/* pointer into oldname */

	alogfile_t *newlog = (alogfile_t *)0;
	int	opened;
	int	error = 0;

#ifdef	DEBUG
	dprintf(stderr, "auditd: inside open_log for dir %s\n",
	    thisdir->dl_name);
#endif	/* DEBUG */
	/* make ourselves a new alogfile structure */
	newlog = (alogfile_t *)malloc(sizeof (alogfile_t));
	if (newlog == NULL)
		exit(1);

	newlog->l_name = (char *)malloc(AUDIT_FNAME_SZ);
	if (newlog->l_name == NULL)
		exit(1);

	(void) gethostname(host, MACH_NAME_SZ);
	/* loop to get a filename which does not already exist */
	opened = 0;
	while (!opened) {
		getauditdate(auditdate);
		(void) sprintf(newlog->l_name, "%s/%s.not_terminated.%s",
		    thisdir->dl_name, auditdate, host);
		newlog->l_fd = open(newlog->l_name,
		    O_RDWR | O_APPEND | O_CREAT | O_EXCL, 0600);
		if (newlog->l_fd < 0) {
			switch (errno) {
			case EEXIST:
				(void) sleep(1);
				break;
			default:
				/* open failed */
#ifdef	DEBUG
				dprintf(stderr, "open failed");
#endif	/* DEBUG */
				thisdir->dl_space = SPACE_FULL;
				thisdir = thisdir->dl_next;
				return (0);
			} /* switch */
		} else
			opened = 1;
	} /* while */

	/*
	 * When we get here, we have opened our new log file.
	 * Now we need to update the name of the old file to
	 * store in this file's header
	 */
	if (alogfile) {
		/* set up oldname, but do not change alogfile */
#ifdef DEBUG
		(void) strcpy(oldname, alogfile->l_name);
		dprintf(stderr, "auditd: original oldname is %s\n", oldname);
		name = (char *)strrchr(oldname, '/') + 1;
		dprintf(stderr, "auditd: will modify %s\n", name);
		(void) memcpy(name + AUDIT_DATE_SZ + 1, auditdate,
			AUDIT_DATE_SZ);
		dprintf(stderr, "auditd: revised oldname is %s\n", oldname);
		dprintf(stderr, "AUDIT_DATE_SZ is %d\n", AUDIT_DATE_SZ);
#else
		(void) strcpy(oldname, alogfile->l_name);
		name = (char *)strrchr(oldname, '/') + 1;
		(void) memcpy(name + AUDIT_DATE_SZ + 1, auditdate,
			AUDIT_DATE_SZ);
#endif
	} else
		(void) strcpy(oldname, "");

	error = write_file_token(newlog->l_fd, oldname);
	if (error) {
#ifdef	DEBUG
		dprintf(stderr, "auditd errno = %d\n", errno);
#endif	/* DEBUG */
		(void) close(newlog->l_fd);
		free((caddr_t)newlog->l_name);
		free((caddr_t)newlog);
		thisdir->dl_space = SPACE_FULL;
		thisdir = thisdir->dl_next;
		return (0);
	}

	(void) close_log(alogfile, oldname, newlog->l_name);
	alogfile = newlog;
	logpost(getpid(), alogfile->l_name);
	startdir = thisdir;
#ifdef	DEBUG
	dprintf(stderr, "auditd: Log opened: %s\n", alogfile->l_name);
#endif	/* DEBUG */
	return (1);
}

/*
 * close_log - close the alogfile if open.  Also put the name of the
 *	new log file in the trailer, and rename the old file
 *	to oldname
 * arguments -
 *	alogfile - the alogfile structure to close (and free)
 *	oldname - the new name for the file to be closed
 *	newname - the name of the new log file (for the trailer)
 */
static int
close_log(alogfile, oname, newname)
	register alogfile_t *alogfile;
	char	*oname;
	char	*newname;
{
	char	auditdate[AUDIT_DATE_SZ+1];
	char	*name;
	char	oldname[AUDIT_FNAME_SZ+1];

#ifdef	DEBUG
	dprintf(stderr, "auditd: inside close_log\n");
#endif	/* DEBUG */
	/*
	 * If there is not a file open, return.
	 */
	if (!alogfile)
		return (-1);
	/*
	 * If oldname is blank, we were called by the main routine
	 * instead of by open_log, so we need to update our name.
	 */
	(void) strcpy(oldname, oname);
	if (strcmp(oldname, "") == 0) {
		getauditdate(auditdate);
		(void) strcpy(oldname, alogfile->l_name);
		name = strrchr(oldname, '/') + 1;
		(void) memcpy(name + AUDIT_DATE_SZ + 1, auditdate,
		    AUDIT_DATE_SZ);
#ifdef	DEBUG
		dprintf(stderr, "auditd: revised oldname is %s\n", oldname);
#endif	/* DEBUG */
	}

	/*
	 * Write the trailer record and rename and close the file.
	 * If any of the write, rename, or close fail, ignore it
	 * since there is not much else we can do.
	 */
	(void) write_file_token(alogfile->l_fd, newname);

	(void) close(alogfile->l_fd);
	(void) rename(alogfile->l_name, oldname);
#ifdef	DEBUG
	dprintf(stderr, "auditd: Log closed %s\n", alogfile->l_name);
#endif	/* DEBUG */
	free((caddr_t)alogfile->l_name);
	free((caddr_t)alogfile);

	return (0);
}

/*
 * getauditdate - get the current time (GMT) and put it in the form
 *		  yyyymmddHHMMSS .
 */
static void
getauditdate(date)
char	*date;
{
	struct timeval tp;
	struct timezone tzp;
	struct tm tm;

#ifdef	DEBUG
	dprintf(stderr, "auditd: inside getauditdate\n");
#endif	/* DEBUG */
	(void) gettimeofday(&tp, &tzp);
	tm = *gmtime(&tp.tv_sec);
	/*
	 * NOTE:  if we want to use gmtime, we have to be aware that the
	 *	structure only keeps the year as an offset from TM_YEAR_BASE.
	 *	I have used TM_YEAR_BASE in this code so that if they change
	 *	this base from 1900 to 2000, it will hopefully mean that this
	 *	code does not have to change.  TM_YEAR_BASE is defined in
	 *	tzfile.h .
	 */
	(void) sprintf(date, "%.4d%.2d%.2d%.2d%.2d%.2d",
		tm.tm_year + TM_YEAR_BASE, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);
#ifdef	DEBUG
	dprintf(stderr, "auditd: auditdate = %s\n", date);
#endif	/* DEBUG */
}

/*
 * loadauditlist - read the directory list from the audit_control file.
 *		   Creates a circular linked list of directories.  Also
 *		   gets the minfree value from the file.
 * globals -
 *	thisdir and startdir - both are left pointing to the start of the
 *		directory list
 *	minfree - the soft limit value to use when filling the directories
 */
static int
loadauditlist()
{
	char	buf[200];
	dirlist_t	 * node;
	dirlist_t	 * *nodep;
	int	acresult;
	int	temp;
	int	wait_count;
	int	got_one = 0;

#ifdef	DEBUG
	dprintf(stderr, "auditd: Loading audit list from auditcontrol\n");
#endif	/* DEBUG */

	/*
	 * Free up the old directory list
	 */
	if (startdir) {
		thisdir = startdir->dl_next;
		while (thisdir != startdir) {
			node = thisdir->dl_next;
			free((caddr_t)thisdir);
			thisdir = node;
		}
		free((caddr_t)startdir);
	}

	/*
	 * Build new directory list
	 */
	nodep = &startdir;
	wait_count = 0;
	while (!got_one) {
		/*
		 * Close and reopen the audit_control file.
		 */
		endac();
		(void) setac();
		if (testac() != 0) {
			(void) fprintf(stderr, gettext(
				"auditd: can't open audit_control(5) file.\n"));
			return (1);
		}
		while ((acresult = getacdir(buf, sizeof (buf))) == 0 ||
		    acresult == 2 || acresult == -3) {
			/*
			 * loop if the result is 0 (success), 2 (a warning
			 * that the audit_data file has been rewound),
			 * or -3 (a directory entry was found, but it
			 * was badly formatted.
			 */
			if (acresult == 0) {
				/*
				 * A directory entry was found.
				 */
				got_one = 1;
				node = (dirlist_t *)malloc(sizeof (dirlist_t));
				if (node == NULL) {
					perror("auditd");
					exit(1);
				}
				node->dl_name = (char *)
					malloc((unsigned)strlen(buf) + 1);
				if (node->dl_name == NULL) {
					perror("auditd");
					exit(1);
				}
				(void) strcpy(node->dl_name, buf);
				node->dl_next = 0;
				*nodep = node;
				nodep = &(node->dl_next);
			}
		}   /* end of getacdir while */
		if (!got_one) {
			/*
			 * there was a problem getting the directory
			 * list from the audit_control file
			 */
			wait_count++;
#ifdef	DEBUG
if (wait_count == 1)
	dprintf(stderr,
		"auditd: problem getting directory list from audit_control.\n");
#endif	/* DEBUG */
			dowarn("getacdir", "", wait_count);
			/*
			 * sleep for SLEEP_TIME seconds - temp would
			 * be set if the user asked us to re-read the
			 * audit_control file during the sleep.  Since
			 * that is exactly what we are trying to do,
			 * this value can be ignored.
			 */
			if (wait_count == 1)
				logpost(getpid(), "");
			(void) my_sleep(&temp);
			continue;
		}
	}    /* end of got_one while */
	node->dl_next = startdir;

#ifdef	DEBUG
	/* print out directory list */

	dprintf(stderr, "Directory list:\n%s\n", startdir->dl_name);
	thisdir = startdir->dl_next;
	while (thisdir != startdir) {
		dprintf(stderr, "%s\n", thisdir->dl_name);
		thisdir = thisdir->dl_next;
	}
#endif	/* DEBUG */

	thisdir = startdir;
	/*
	 * Get the minfree value
	 */
	if (!(getacmin(&minfree) == 0 && minfree >= 0 && minfree <= 100)) {
		minfree = 0;
		dowarn("getacmin", "", wait_count);
	}

#ifdef	DEBUG
	dprintf(stderr, "auditd: new minfree: %d\n", minfree);
#endif	/* DEBUG */
	return (0);
}



/*
 * logpost - post the new audit log file name to audit_data.
 */
static void
logpost(pid, name)
pid_t pid;
char	*name;
{
	char	buffer[MAXPATHLEN];

#ifdef	DEBUG
	dprintf(stderr, "auditd: Posting %d, %s to %s\n",
	    pid, name, auditdata);
#endif	/* DEBUG */

	(void) sprintf(buffer, "%d:%s\n", pid, name);
	(void) ftruncate(audit_data_fd, (off_t)0);
	(void) write(audit_data_fd, buffer, strlen(buffer));
	(void) fsync(audit_data_fd);
}

/*
 * testspace - determine whether the given directorie's filesystem
 *	has the given amount of space.  Also set the space
 *	value in the directory list structure.
 * globals -
 *	minfree - what the soft limit is (% of filesystem to reserve)
 *	limit - whether we are using soft or hard limits
 */
static int
testspace(thisdir, test_limit)
dirlist_t *thisdir;
int	test_limit;
{
	struct statvfs sb;

#ifdef	DEBUG
	dprintf(stderr, "auditd: checking %s for space limit %d\n",
	    thisdir->dl_name, test_limit);
#endif	/* DEBUG */

	if (thisdir->dl_space == STAY_FULL) {
		thisdir->dl_space = SPACE_FULL;
		minfreeblocks = 0;
	} else if (statvfs(thisdir->dl_name, &sb) < 0) {
#ifdef	DEBUG
		dprintf(stderr, "auditd: statvfs() errno = %d\n", errno);
#endif	/* DEBUG */
		thisdir->dl_space = SPACE_FULL;
		minfreeblocks = 0;
	} else {
		minfreeblocks = minfree * (sb.f_blocks / 100);
#ifdef	DEBUG
		dprintf(stderr, "auditd: bavail = %d, minblocks = %d\n",
		    sb.f_bavail, minfreeblocks);
#endif	/* DEBUG */
		if (sb.f_bavail < AVAIL_MIN)
			thisdir->dl_space = SPACE_FULL;
		else if (sb.f_bavail < minfreeblocks)
			thisdir->dl_space = HARD_SPACE;
			else
			thisdir->dl_space = SOFT_SPACE;

	}
	return (thisdir->dl_space == test_limit);
}



/*
 * dowarn - invoke the shell script auditwarn to notify the adminstrator

 *	    about the given problem.
 * parameters -
 *	option - what the problem is
 *	alogfile - used with options soft and hard: which file was being
 *		   used when the filesystem filled up
 *	count - used with option allhard: how many times auditwarn has
 *		been called for this problem
 */
void
dowarn(option, filename, count)
char	*option;
char	*filename;
int	count;
{
	register int	pid;
	int	st;
	char	countstr[5];
	char	warnstring[80];

#ifdef	DEBUG
	dprintf(stderr, "auditd: calling %s with %s %s %d\n",
	    auditwarn, option, filename, count);
#endif	/* DEBUG */

	if ((pid = fork()) == -1) {
		(void) fprintf(stderr, gettext("auditd: fork failed\n"));
		return;
	}
	if (pid != 0) {
		pid = wait(&st);
		return;
	}

	/*
	 * Set our effective uid back to root so that audit_warn can
	 * write to the console if it needs to.
	 */

	/*
	 * setreuid does not exist in SYS V.  Replace it with setuid.
	 */

	/* (void)setreuid(0, 0); */
	(void) setuid(0);

	(void) sprintf(countstr, "%d", count);

	if (strcmp(option, "soft") == 0 || strcmp(option, "hard") == 0)
		(void) execl(auditwarn, auditwarn, option, filename, 0);
	else if (strcmp(option, "allhard") == 0 ||
	    strcmp(option, "getacdir") == 0)
		(void) execl(auditwarn, auditwarn, option, countstr, 0);
		else
		(void) execl(auditwarn, auditwarn, option, 0);

	/*
	 * If get here, execls above failed.
	 */
	if (strcmp(option, "soft") == 0)
		(void) sprintf(warnstring, "soft limit in %s.\n", filename);
	else if (strcmp(option, "hard") == 0)
		(void) sprintf(warnstring, "hard limit in %s.\n", filename);
	else if (strcmp(option, "allhard") == 0)
		(void) sprintf(warnstring, "All audit filesystems are full.\n");
	else if (strcmp(option, "getacmin") == 0)
		(void) sprintf(warnstring, "audit_control minfree error.\n");
	else if (strcmp(option, "getacdir") == 0)
		(void) sprintf(warnstring, "audit_control directory error.\n");
		else
		(void) sprintf(warnstring, "error %s.\n", option);

	(void) openlog("auditd", LOG_PID | LOG_ODELAY | LOG_CONS, LOG_AUTH);
	(void) syslog(LOG_ALERT, (const char *)warnstring);
	(void) closelog();
	exit(1);
}


/*
 * write_file_token - put the file token into the audit log
 */
static int
write_file_token(fd, name)
int	fd;
char	*name;
{
	adr_t adr;					/* xdr ptr */
	struct timeval tv;				/* time now */
	char for_adr[AUDIT_FNAME_SZ + AUDIT_FNAME_SZ];	/* plenty of room */
	char	token_id;
	short	i;

	(void) gettimeofday(&tv, (struct timezone *)0);
	i = strlen(name) + 1;
	adr_start(&adr, for_adr);
#ifdef _LP64
		token_id = AUT_OTHER_FILE64;
		adr_char(&adr, &token_id, 1);
		adr_int64(&adr, (int64_t *) & tv, 2);
#else
		token_id = AUT_OTHER_FILE32;
		adr_char(&adr, &token_id, 1);
		adr_int32(&adr, (int32_t *) & tv, 2);
#endif

	adr_short(&adr, &i, 1);
	adr_char(&adr, name, i);

	if (write(fd, for_adr, adr_count(&adr)) < 0) {
#ifdef	DEBUG
		dprintf(stderr, "auditd: Bad write\n");
#endif	/* DEBUG */
		return (errno);
	}
	return (0);
}

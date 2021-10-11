/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rcm_main.c	1.1	99/08/10 SMI"

/*
 * Reconfiguration Coordination Daemon
 *
 * Accept RCM messages in the form of RCM events and process them
 * - to build and update the system resource map
 * - to allow clients to register/unregister for resource
 * - to allow dr initiators to offline a resource before removal
 * - to call into clients to perform suspend/offline actions
 *
 * The goal is to enable fully automated Dynamic Reconfiguration and better
 * DR information tracking.
 */

#include <librcm_event.h>

#include "rcm_impl.h"

/* will run in daemon mode if debug level < DEBUG_LEVEL_FORK */
#define	DEBUG_LEVEL_FORK	RCM_DEBUG

#define	DAEMON_LOCK_FILE "/var/run/rcm_daemon_lock"

static int hold_daemon_lock;
static int daemon_lock_fd;
static const char *daemon_lock_file = DAEMON_LOCK_FILE;

static int debug_level = 0;
static int idle_timeout;
static int logflag = 0;
static char *prog;

static void usage(void);
static void catch_sighup(void);
static void catch_sigusr1(void);
static pid_t enter_daemon_lock(void);
static void exit_daemon_lock(void);

/*
 * Print command line syntax for starting rcm_daemon
 */
static void
usage() {
	(void) fprintf(stderr,
	    gettext("usage: %s [-d debug_level] [-t idle_timeout]\n"), prog);
	rcmd_exit(EINVAL);
}

/*
 * common exit function which ensures releasing locks
 */
void
rcmd_exit(int status)
{
	if (status == 0) {
		rcm_log_message(RCM_INFO,
		    gettext("rcm_daemon normal exit\n"));
	} else {
		rcm_log_message(RCM_ERROR,
		    gettext("rcm_daemon exit: errno = %d\n"), status);
	}

	if (hold_daemon_lock) {
		exit_daemon_lock();
	}

	exit(status);
}

/*
 * When SIGHUP is received, reload modules at the next safe moment (when
 * there is no DR activity.
 */
void
catch_sighup(void)
{
	rcm_log_message(RCM_INFO,
	    gettext("SIGHUP received, will exit when daemon is idle\n"));
	rcmd_thr_signal();
}

/*
 * When SIGUSR1 is received, exit the thread
 */
void
catch_sigusr1(void)
{
	rcm_log_message(RCM_DEBUG, "SIGUSR1 received in thread %d\n",
	    thr_self());
	thr_exit(NULL);
}

/*
 * Use an advisory lock to ensure that only one daemon process is
 * active at any point in time.
 */
static pid_t
enter_daemon_lock(void)
{
	struct flock lock;

	rcm_log_message(RCM_TRACE1,
	    "enter_daemon_lock: lock file = %s\n", daemon_lock_file);

	daemon_lock_fd = open(daemon_lock_file, O_CREAT|O_RDWR, 0644);
	if (daemon_lock_fd < 0) {
		rcm_log_message(RCM_ERROR, gettext("open(%s) - %s\n"),
		    daemon_lock_file, strerror(errno));
		rcmd_exit(errno);
	}

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	if (fcntl(daemon_lock_fd, F_SETLK, &lock) == 0) {
		hold_daemon_lock = 1;
		return (getpid());
	}

	/* failed to get lock, attempt to find lock owner */
	if ((errno == EAGAIN || errno == EDEADLK) &&
	    (fcntl(daemon_lock_fd, F_GETLK, &lock) == 0)) {
		return (lock.l_pid);
	}

	/* die a horrible death */
	rcm_log_message(RCM_ERROR, gettext("lock(%s) - %s"), daemon_lock_file,
	    strerror(errno));
	exit(errno);
	/*NOTREACHED*/
}

/*
 * Drop the advisory daemon lock, close lock file
 */
static void
exit_daemon_lock(void)
{
	struct flock lock;

	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	if (fcntl(daemon_lock_fd, F_SETLK, &lock) == -1) {
		rcm_log_message(RCM_ERROR, gettext("unlock(%s) - %s"),
		    daemon_lock_file, strerror(errno));
	}

	(void) close(daemon_lock_fd);
}

/*
 * print error messages to the terminal or to syslog
 */
void
rcm_log_message(int level, char *message, ...)
{
	va_list ap;
	int log_level;

	if (level > debug_level) {
		return;
	}

	if (!logflag) {
		/*
		 * RCM_ERROR goes to stderr, others go to stdout
		 */
		FILE *out = (level <= RCM_ERROR) ? stderr : stdout;
		va_start(ap, message);
		(void) vfprintf(out, message, ap);
		va_end(ap);
		return;
	}

	/*
	 * translate RCM_* to LOG_*
	 */
	switch (level) {
	case RCM_ERROR:
		log_level = LOG_ERR;
		break;
	case RCM_WARNING:
		log_level = LOG_WARNING;
		break;
	case RCM_NOTICE:
		log_level = LOG_NOTICE;
		break;
	case RCM_INFO:
		log_level = LOG_INFO;
		break;
	case RCM_DEBUG:
		log_level = LOG_DEBUG;
		break;
	default:
		/*
		 * Don't log RCM_TRACEn messages
		 */
		return;
	}

	va_start(ap, message);
	(void) vsyslog(log_level, message, ap);
	va_end(ap);
}

/*
 * grab daemon_lock and direct messages to syslog
 */
static void
detachfromtty()
{
	(void) chdir("/");
	(void) setsid();
	(void) close(0);
	(void) close(1);
	(void) close(2);
	(void) open("/dev/null", O_RDWR, 0);
	(void) dup2(0, 1);
	(void) dup2(0, 2);
	openlog(prog, LOG_PID, LOG_DAEMON);
	logflag = 1;
}

void
main(int argc, char **argv)
{
	int c;
	pid_t pid;
	extern char *optarg;
	sigset_t mask;
	struct sigaction act;

	(void) setlocale(LC_ALL, "");
#ifndef	TEXT_DOMAIN
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	if ((prog = strrchr(argv[0], '/')) == NULL) {
		prog = argv[0];
	} else {
		prog++;
	}

	/*
	 * process arguments
	 */
	if (argc > 3) {
		usage();
	}
	while ((c = getopt(argc, argv, "d:t:")) != EOF) {
		switch (c) {
		case 'd':
			debug_level = atoi(optarg);
			break;
		case 't':
			idle_timeout = atoi(optarg);
			break;
		case '?':
		default:
			usage();
			/*NOTREACHED*/
		}
	}

	/*
	 * Check permission
	 */
	if (getuid() != 0) {
		(void) fprintf(stderr, gettext("Must be root to run %s\n"),
		    prog);
		exit(EPERM);
	}

	/*
	 * block SIGUSR1, use it for killing specific threads
	 */
	(void) sigemptyset(&mask);
	(void) sigaddset(&mask, SIGUSR1);
	thr_sigsetmask(SIG_BLOCK, &mask, NULL);

	/*
	 * Setup signal handlers for SIGHUP and SIGUSR1
	 * SIGHUP - causes a "delayed" daemon exit, effectively the same
	 *	as a daemon restart.
	 * SIGUSR1 - causes a thr_exit(). Unblocked in selected threads.
	 */
	act.sa_flags = 0;
	act.sa_handler = catch_sighup;
	(void) sigaction(SIGHUP, &act, NULL);
	act.sa_handler = catch_sigusr1;
	(void) sigaction(SIGUSR1, &act, NULL);

	/*
	 * run in daemon mode
	 */
	if (debug_level < DEBUG_LEVEL_FORK) {
		if (fork()) {
			exit(0);
		}
		detachfromtty();
	}

	/* only one daemon can run at a time */
	if ((pid = enter_daemon_lock()) != getpid()) {
		rcm_log_message(RCM_DEBUG, "%s pid %d already running\n",
		    prog, pid);
		exit(EDEADLK);
	}

	rcm_log_message(RCM_TRACE1, "%s started, debug level = %d\n",
	    prog, debug_level);

	/*
	 * Set daemon state to block RCM requests before rcm_daemon is
	 * fully initialized. See rcmd_thr_incr().
	 */
	rcmd_set_state(RCMD_INIT);

	/*
	 * create rcm_daemon door and set permission to 0400
	 */
	if (create_event_service(RCM_SERVICE_DOOR, event_service) == -1) {
		rcm_log_message(RCM_ERROR,
		    gettext("cannot create door service: %s\n"),
		    strerror(errno));
		rcmd_exit(errno);
	}
	(void) chmod(RCM_SERVICE_DOOR, S_IRUSR);

	/*
	 * Initialize database by asking modules to register.
	 */
	rcmd_db_init();

	/*
	 * Initialize locking, including lock recovery in the event of
	 * unexpected daemon failure.
	 */
	rcmd_lock_init();

	/*
	 * Start accepting normal requests
	 */
	rcmd_set_state(RCMD_NORMAL);

	/*
	 * Start cleanup thread
	 */
	rcmd_db_clean();

	/*
	 * Loop and shutdown daemon after a period of inactivity.
	 */
	rcmd_start_timer(idle_timeout);
	/* NOTREACHED */
}

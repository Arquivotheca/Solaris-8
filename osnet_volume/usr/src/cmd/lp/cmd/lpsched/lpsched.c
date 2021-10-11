/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)lpsched.c	1.27	99/04/20 SMI" 

#include "limits.h"
#include "ulimit.h"
#include "sys/utsname.h"

#include "lpsched.h"

#include <sys/stat.h>
#include <sys/time.h>		/* to up the max # of fds */
#include <sys/resource.h>
#include <syslog.h>


int			lock_fd		= -1;
int			isStartingForms = 0;
int			Starting	= 0;
int			Shutdown	= 0;
int			DoneChildren	= 0;
int			Sig_Alrm	= 0;
int			OpenMax		= OPEN_MAX;
int			Reserve_Fds	= 0;

char			*Local_System	= 0;
char			*SHELL		= 0;

char *LP_TRAY_UNMOUNT = NULL;
char *LP_KILL_NO_PAPER = NULL;
char *LP_ALL_NEW = NULL;

gid_t			Lp_Gid;
uid_t			Lp_Uid;

#if	defined(DEBUG)
unsigned long		debug = 0;
static int		signals = 0;
#endif

extern int		errno;
extern char		*lpsched_buildinfo;
extern void		shutdown_messages();

int			am_in_background	= 0;

static void		disable_signals();
static void		startup();
static void		process();
static void		ticktock(int);
static void		background();
static void		usage();
static void		Exit();
static void		disable_signals();

/**
 ** main()
 **/

main(int argc, char *argv[])
{
    int		c;
    extern char	*optarg;
    extern int	optopt;
    extern int	opterr;
    char *	cp;
    struct rlimit rlim;
    int fd_limit = 4096;

    if ((cp = strrchr(argv[0], '/')) == NULL)
	    cp = argv[0];
    else
	    cp++;

    /* open the syslog() */
    openlog(cp, LOG_PID|LOG_NDELAY|LOG_NOWAIT, LOG_LPR);

	SHELL = DEFAULT_SHELL;
	LP_TRAY_UNMOUNT = getenv("LP_TRAY_UNMOUNT");
	LP_KILL_NO_PAPER = getenv("LP_KILL_NO_PAPER");
	LP_ALL_NEW = getenv("LP_ALL_NEW");

    opterr = 0;
    while((c = getopt(argc, (char * const *)argv, "D:dsf:n:r:M:p:")) != EOF)
        switch(c)
        {
# if defined (DEBUG)
	    case 'd':
		debug = DB_ALL;
		goto SkipD;
	    case 'D':
		if (*optarg == '?') {
			note (
"-D flag[,flag...]    (all logs \"foo\" are in /var/lp/logs,\n"
"                      although \"lpsched\" goes to stdout if SDB)\n"
"\n"
"  EXEC               (log all exec's in \"exec\")\n"
"  DONE               (log just exec finishes in \"exec\")\n"
"  INIT               (log initialization info in \"lpsched\" or stdout)\n"
"  ABORT              (issue abort(2) on fatal error)\n"
"  SCHEDLOG           (log additional debugging info in \"lpsched\")\n"
"  SDB                (don't start lpsched as background process)\n"
"  MESSAGES           (log all message traffic in \"messages\")\n"
			);
			note ("\
  ALL                (all of the above; equivalent to -d)\n"
			);
			exit (0);
		}
		while ((cp = strtok(optarg, ", "))) {
#define IFSETDB(P,S,F)	if (STREQU(P, S)) debug |= F
			IFSETDB (cp, "EXEC", DB_EXEC);
			else IFSETDB (cp, "DONE", DB_DONE);
			else IFSETDB (cp, "INIT", DB_INIT);
			else IFSETDB (cp, "ABORT", DB_ABORT);
			else IFSETDB (cp, "SCHEDLOG", DB_SCHEDLOG);
			else IFSETDB (cp, "SDB", DB_SDB);
			else IFSETDB (cp, "MESSAGES", DB_MESSAGES);
			else IFSETDB (cp, "ALL", DB_ALL);
			else {
				note ("-D flag not recognized; try -D?\n");
				exit (1);
			}
			optarg = 0;
		}
SkipD:
		break;

	    case 's':
		signals++;
		break;
# endif /* DEBUG */

	    case 'f':
		if ((ET_SlowSize = atoi(optarg)) < 1)
		    ET_SlowSize = 1;
		break;

	    case 'n':
		if ((ET_NotifySize = atoi(optarg)) < 1)
		    ET_NotifySize = 1;
		break;

	    case 'r':
		if ((Reserve_Fds = atoi(optarg)) < 0)
			Reserve_Fds = 0;
		break;

	    case 'p':
		if ((fd_limit = atoi(optarg)) < 16)
			fd_limit = 4096;
		break;
	    case '?':
		if (optopt == '?') {
		    usage ();
		    exit (0);
		} else
		    fail ("%s: illegal option -- %c\n", argv[0], optopt);
	}

	/* reset the fd resource limit */
	rlim.rlim_max = rlim.rlim_cur = fd_limit;
	setrlimit(RLIMIT_NOFILE, &rlim);
	getrlimit(RLIMIT_NOFILE, &rlim);
	syslog(LOG_DEBUG, "file descriptor resource limit is %d (~%d printers)",
		rlim.rlim_cur, (rlim.rlim_cur - 12)/ 2);
    
    lp_alloc_fail_handler = mallocfail;

    startup();

    process();

    lpshut(1);	/* one last time to clean up */
    /*NOTREACHED*/
}

static void
startup()
{
    struct passwd		*p;
    struct utsname		utsbuf;

    
    Starting = 1;
    getpaths();

    /*
     * There must be a user named "lp".
     */
    if ((p = getpwnam(LPUSER)) == NULL)
	fail ("Can't find the user \"lp\" on this system!\n");

    Lp_Uid = p->pw_uid;
    Lp_Gid = p->pw_gid;

    /*
     * Only "root" is allowed to run us.
     */
    if ((getuid() != 0) && (geteuid() != 0))
	fail ("You must be \"root\" to run this program.\n");

    setuid (0);

    uname(&utsbuf);
    Local_System = Strdup(utsbuf.nodename);

    /*
     * Make sure that all critical directories are present and that 
     * symbolic links are correct.
     */
    lpfsck();
    
    /*
     * Try setting the lock file to see if another Spooler is running.
     * We'll release it immediately; this allows us to fork the child
     * that will run in the background. The child will relock the file.
     */
    if ((lock_fd = open_locked(Lp_Schedlock, "a", 0664)) < 0)
	if (errno == EAGAIN)
	    fail ("Print services already active.\n");
	else
	    fail ("Can't open file \"%s\" (%s).\n", NB(Lp_Schedlock), PERROR);
    close(lock_fd);

    background();
    /*
     * We are the child process now.
     */

    if ((lock_fd = open_locked(Lp_Schedlock, "w", 0664)) < 0)
	fail ("Failed to lock the file \"%s\" (%s).\n", NB(Lp_Schedlock), PERROR);

    Close (0);
    Close (2);
    if (am_in_background)
	Close (1);

    if ((OpenMax = ulimit(4, 0L)) == -1)
	OpenMax = OPEN_MAX;

    disable_signals();

    init_messages();

    init_memory();

    note (lpsched_buildinfo);
    note ("Print services started.\n");
    Starting = 0;
}

void
lpshut(int immediate)
{
	int			i;
	extern MESG *		Net_md;


	/*
	 * If this is the first time here, stop all running
	 * child processes, and shut off the alarm clock so
	 * it doesn't bug us.
	 */
	if (!Shutdown) {
		mputm (Net_md, S_SHUTDOWN, 1);
		for (i = 0; i < ET_Size; i++)
			terminate (&Exec_Table[i]);
		alarm (0);
		Shutdown = (immediate? 2 : 1);
	}

	/*
	 * If this is an express shutdown, or if all the
	 * child processes have been cleaned up, clean up
	 * and get out.
	 */
	if (Shutdown == 2) {

		/*
		 * We don't shut down the message queues until
		 * now, to give the children a chance to answer.
		 * This means an LP command may have been snuck
		 * in while we were waiting for the children to
		 * finish, but that's OK because we'll have
		 * stored the jobs on disk (that's part of the
		 * normal operation, not just during shutdown phase).
		 */
		shutdown_messages();
    
		(void) close(lock_fd);
		(void) Unlink(Lp_Schedlock);

		note ("Print services stopped.\n");
		exit (0);
		/*NOTREACHED*/
	}
}

static void
process()
{
    register FSTATUS	*pfs;
    register PWSTATUS	*ppws;


    /*
     * Call the "check_..._alert()" routines for each form/print-wheel;
     * we need to do this at this point because these routines
     * short-circuit themselves while we are in startup mode.
     * Calling them now will kick off any necessary alerts.
     */
    isStartingForms = 1;
    for (pfs = walk_ftable(1); pfs; pfs = walk_ftable(0))
	check_form_alert (pfs, (_FORM *)0);
    isStartingForms = 0;

    for (ppws = walk_pwtable(1); ppws; ppws = walk_pwtable(0))
	check_pwheel_alert (ppws, (PWHEEL *)0);
    
    /*
     * Clear the alarm, then schedule an EV_ALARM. This will clear
     * all events that had been scheduled for later without waiting
     * for the next tick.
     */
    alarm (0);
    schedule (EV_ALARM);

    /*
     * Start the ball rolling.
     */
    schedule (EV_INTERF, (PSTATUS *)0);
    schedule (EV_NOTIFY, (RSTATUS *)0);
    schedule (EV_SLOWF, (RSTATUS *)0);

    for (EVER) {
	take_message ();

	if (Sig_Alrm)
		schedule (EV_ALARM);

	if (DoneChildren)
		dowait ();

	if (Shutdown)
		check_children();
	if (Shutdown == 2)
		break;
    }
}

/*ARGSUSED*/
static void
ticktock(int sig)
{
	Sig_Alrm = 1;
	(void)signal (SIGALRM, ticktock);
	return;
}
			    
static void
background()
{
#if	defined(DEBUG)
    if (debug & DB_SDB)
	return;
#endif
    
    switch(fork())
    {
	case -1:
	    fail ("Failed to fork child process (%s).\n", PERROR);
	    /*NOTREACHED*/

	case 0:
	    (void) setpgrp();
	    am_in_background = 1;
	    return;
	    
	default:
	    note ("Print services started.\n");
	    exit(0);
	    /* NOTREACHED */
    }
}

static void
usage()
{
	note ("\
usage: lpsched [ options ]\n\
    [ -f #filter-slots ]    (increase no. concurrent slow filters)\n\
    [ -n #notify-slots ]    (increase no. concurrent notifications)\n\
    [ -r #reserved-fds ]    (increase margin of file descriptors)\n"
	);

#if	defined(DEBUG)
	note ("\
    [ -D flag[,flag...] ]   (debug modes; use -D? for usage info.)\n\
    [ -d ]                  (same as -D ALL)\n\
    [ -s ]                  (don't trap most signals)\n"
	);
#endif

	note ("\
WARNING: all these options are currently unsupported\n"
	);

	return;
}

static void
Exit(n)
    int		n;
{
    fail ("Received unexpected signal %d; terminating.\n", n);
}

static void
disable_signals()
{
    int		i;

# if defined(DEBUG)
    if (!signals)
# endif
	for (i = 0; i < NSIG; i++)
		if (signal(i, SIG_IGN) != SIG_IGN)
			signal (i, Exit);
    
    (void) signal(SIGHUP, SIG_IGN);
    (void) signal(SIGINT, SIG_IGN);
    (void) signal(SIGQUIT, SIG_IGN);
    (void) signal(SIGALRM, ticktock);
    (void) signal(SIGTERM, lpshut);	/* needs arg, but sig# OK */
    (void) signal(SIGCLD, SIG_IGN);
    (void) signal(SIGTSTP, SIG_IGN);
    (void) signal(SIGCONT, SIG_DFL);
    (void) signal(SIGTTIN, SIG_IGN);
    (void) signal(SIGTTOU, SIG_IGN);
    (void) signal(SIGXFSZ, SIG_IGN);	/* could be a problem */
    (void) signal(SIGWINCH, SIG_IGN);   /* if started in a window   */
    (void) signal(SIGTHAW, SIG_IGN);   /* used by CPR - energystar */

#if	defined(DEBUG)
    if (debug & DB_ABORT)
	(void) signal(SIGABRT, SIG_DFL);
#endif

}

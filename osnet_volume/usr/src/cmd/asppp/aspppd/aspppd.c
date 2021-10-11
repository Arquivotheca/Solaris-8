#ident	"@(#)aspppd.c	1.42	96/11/14 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#include <fcntl.h>
#include <poll.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "aspppd.h"
#include "fds.h"
#include "fifo.h"
#include "iflist.h"
#include "ipd.h"
#include "log.h"
#include "parse.h"
#include "path.h"
#include "ppp.h"

int		errno;
int		debug = 0;
/*
 * HB bug 1262630 (SIGHUP rcvd during call establishment).
 * conn_cntr counts the number of transient connections
 * connections on which ppp has not been pushed yet)
 * which is when SIGHUP causes problems.
 */
int		conn_cntr = 0;
jmp_buf		restart;

static void	daemon_init(void);
static void	usage(char *);
static void	catch_exits(int);
static void	sign_log(char *);
static void	sign_off(void);
static void	main_loop(void);
static void	catch_sighup(int);


void
main(int argc, char **argv)
{
	int			c;
	sigset_t		set;
	struct sigaction	act;

	open_log();	/* uses stdout, closes stderr */

	(void) close(STDIN_FILENO);
	(void) close(STDOUT_FILENO);

	daemon_init();

	while ((c = getopt(argc, argv, "d:")) != -1) {
		switch (c) {
		case 'd':
			debug = atoi(optarg);
			if (debug < 0 || debug > 9)
				usage(argv[0]);
			break;
		case '?':
			usage(argv[0]);
			break;
		}
	}

	sign_log("started");

	act.sa_handler = catch_exits;
	if (sigemptyset(&act.sa_mask) < 0)
		fail("main: sigemptyset failed\n");
	if (sigaddset(&act.sa_mask, SIGHUP) < 0)
		fail("main: sigaddset failed\n");
	if (sigaction(SIGTERM, &act, NULL) < 0)
		fail("main: sigaction failed\n");

	if (atexit(sign_off) != 0)
		log(42, "main: atexit failed\n");

	parse_config_file();

	/*
	 * Block SIGHUP (by adding to current mask) until main loop
	 * initialized
	 */

	if (sigprocmask(NULL, NULL, &set) < 0)
		fail("main: sigprocmask couldn't get mask\n");
	if (sigaddset(&set, SIGHUP) < 0)
		fail("main: sigaddset failed\n");
	if (sigprocmask(SIG_BLOCK, &set, NULL) < 0)
		fail("main: couldn't block SIGHUP\n");

	main_loop();
}

static void
daemon_init(void)
{
	pid_t	pid;

	if ((pid = fork()) < 0)
		fail("daemon_init: couldn't fork\n");
	else if (pid != 0)
		exit(EXIT_SUCCESS);

	/*
	 * if (setsid() < 0)
	 *	fail("daemon_init: setsid failed\n");
	 *
	 * setsid() possibly causing a problem hanging onto serial device
	 * so use setpgid instead
	 */

	if (setpgid(0, 0))
	    fail("daemon_init: setpgid failed\n");

	if (chdir("/") < 0)
		fail("daemon_init: chdir failed\n");

	(void) umask(0);
}

static void
usage(char *s)
{
	log(0, "usage: %s [ -d <debug level>]\n", s);
	log(0, "       specify -d <debug level> for debugging. Debug level\n");
	log(0, "       is an integer between 0 and 9 inclusive.  Higher\n");
	log(0, "       numbers give more detailed information.\n");
	exit(EXIT_FAILURE);
}

static void
catch_exits(int signo)
{
	exit(EXIT_SUCCESS);
}


static void
catch_sighup(int signo)
{
	/*
	 * bug 1262630. If HUP is caught when conn_cntr is not null, either the
	 * modem hung up or the user sent SIGHUP explicitly. In neither case
	 * do we want to do anything.
	 */
	if (conn_cntr != 0) {
		log(1, "catch_sighup: "
			"HUP caught when a connection is being established."
			" Ignored.\n");
		return;
	}

	log(1, "catch_sighup: HUP caught\n");
	longjmp(restart, 1);
}

static void
sign_log(char *status)
{
	char buf[80];
	time_t stime;
	struct tm *ltime;
	int n;

	n = sprintf(buf, "Link manager (%d) %s ", getpid(), status);

	if (time(&stime) >= 0) {
		ltime = localtime(&stime);
		(void) strftime(&buf[n], sizeof (buf) - n, "%x", ltime);
	}

	if (n > 0)
		log(0, "%s\n", buf);
}

static void
sign_off(void)
{
	sign_log("exited");
}

static void
main_loop(void)
{
	int			timeout;
	int			i, n;
	struct sigaction	act;

	/* open a stream to IP/dialup */
	if ((ipdcm = open(IPDCM, O_RDWR)) < 0) {
		fail("main_loop: open /dev/ipdcm\n");
	}

	register_interfaces();

	add_to_fds(ipdcm, POLLIN, process_ipd_msg);

	create_fifo();

	/* Restart on SIGHUP */

	if (setjmp(restart) == 0) {	/* no need for sigsetjmp */
		act.sa_handler = catch_sighup;
		if (sigemptyset(&act.sa_mask) < 0)
			fail("main_loop: sigemptyset failed\n");

		act.sa_flags = 0;
		if (sigaction(SIGHUP, &act, NULL) < 0)
			fail("main_loop: sigaction failed\n");
		if (sigaddset(&act.sa_mask, SIGHUP) < 0)
			fail("main_loop: sigaddset failed\n");
	} else {
		struct path	*p, *oldp;
		for (p = paths; p; /* for cstyle! */) {
			terminate_path(p);
			oldp = p;
			p = p->next;
			free_path(oldp);
		}
		paths = NULL;
		parse_config_file();
	}
	if (sigprocmask(SIG_UNBLOCK, &act.sa_mask, NULL) < 0)
		fail("main_loop: couldn't unblock SIGHUP\n");

	timeout = -1;
	while (1) {
		switch ((n = poll(fds, nfds, timeout))) {
		case -1:
			log(1, "main_loop: "
				"poll failed : %s.\n", strerror(errno));
			break;
		case 0:
			log(42, "main_loop: poll timed out\n");
			break;
		default:
			for (i = 0; i <= nfds; ++i) {
				if (fds[i].fd != -1 && fds[i].revents) {
					if (expected_event(fds[i]))
						(*do_callback(i))(i);
					else
						log_bad_event(fds[i]);
					--n;
				}
			}
			break;
		}
	}
}

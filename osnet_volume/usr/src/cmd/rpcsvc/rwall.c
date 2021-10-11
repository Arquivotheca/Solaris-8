#pragma ident	"@(#)rwall.c	1.11	98/10/22 SMI"

/*
 * rwall.c
 *	The client rwall program
 *
 * Copyright (c) 1984-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <thread.h>
#include <string.h>
#include <rpc/rpc.h>
#include <signal.h>
#include <pwd.h>
#include <rpcsvc/rwall.h>
#include <netconfig.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/resource.h>

static void init_who(void);
static void doall(void);
static void doit(char *);
static void *do_one(void *);
static void usage(void);

#define	PATIENCE 10
#define	MAX_THREADS 1024

static mutex_t tty = DEFAULTMUTEX;
static char who[9] = "???";
static char *path;
static struct netconfig *nconf = NULL;
static mutex_t thr_mtx = DEFAULTMUTEX;
static int thread_count = 8;	/* fudge factor for system threads/fds */

main(argc, argv)
	int argc;
	char **argv;
{
	int msize;
	char buf[BUFSIZ];
	register i;
	char hostname[256];
	int hflag;
	void *handle;
	struct rlimit rl;

	if (argc < 2)
		usage();

	if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
		rl.rlim_cur = (rl.rlim_max < MAX_THREADS ?
		    rl.rlim_max : MAX_THREADS);
		(void) setrlimit(RLIMIT_NOFILE, &rl);
	}

	(void) gethostname(hostname, sizeof (hostname));

	init_who();

	(void) sprintf(buf, "From %s@%s:  ", who, hostname);
	msize = strlen(buf);
	while ((i = getchar()) != EOF) {
		if (msize >= sizeof (buf)) {
			(void) fprintf(stderr, "Message too long\n");
			exit(1);
		}
		buf[msize++] = i;
	}
	buf[msize] = '\0';

	handle = setnetpath();
	while ((nconf = getnetpath(handle)) != NULL) {
		if (nconf->nc_semantics == NC_TPI_CLTS) {
			break;
		}
	}
	if (nconf == NULL) {
		(void) fprintf(stderr,
			"No connectionless transports are available.\n");
		(void) endnetpath(handle);
		exit(1);
	}

	path = buf;
	hflag = 1;
	while (argc > 1) {
		if (argv[1][0] == '-') {
			switch (argv[1][1]) {
				case 'h':
					hflag = 1;
					break;
				case 'n':
					hflag = 0;
					break;
				default:
					usage();
					break;
			}
			argc--;
			argv++;
			continue;
		}
		if (hflag) {
			doit(argv[1]);
		} else {
			char *machine, *user, *domain;

			(void) setnetgrent(argv[1]);
			while (getnetgrent(&machine, &user, &domain)) {
				if (machine)
					doit(machine);
				else
					doall();
			}
			(void) endnetgrent();
		}
		argc--;
		argv++;
	}
	(void) endnetpath(handle);
	thr_exit(NULL);
	return (0);
}

static void
init_who(void)
{
	char *wp;
	struct passwd *pwd;

	wp = getlogin();

	if (wp != NULL)
		(void) strncpy(who, wp, sizeof (who));
	else {
		pwd = getpwuid(getuid());
		if (pwd)
			(void) strncpy(who, pwd->pw_name, sizeof (who));
	}

}

/*
 * Saw a wild card, so do everything
 */
static void
doall(void)
{
	(void) mutex_lock(&tty);
	(void) fprintf(stderr, "writing to everyone not supported\n");
	(void) mutex_unlock(&tty);
}

/*
 * Fire off a detached thread for each host in the list, if the thread
 * create fails simply run synchronously.
 */
static void
doit(char *hostname)
{
	thread_t tid;
	char *thread_hostname;

	(void) mutex_lock(&thr_mtx);
	while (thread_count >= MAX_THREADS) {
		(void) mutex_unlock(&thr_mtx);
		(void) sleep(PATIENCE/2);
		(void) mutex_lock(&thr_mtx);
	}

	thread_count++;
	(void) mutex_unlock(&thr_mtx);

	thread_hostname = malloc(strlen(hostname) + 1);
	if (thread_hostname == (char *)NULL) {
		(void) mutex_lock(&tty);
		(void) fprintf(stderr, "Ran out of memory\n");
		(void) mutex_unlock(&tty);
		exit(1);
	}
	strcpy(thread_hostname, hostname);

	if (thr_create(NULL, 0, do_one, thread_hostname,
			THR_DETACHED, &tid) != 0) {
		(void) do_one(hostname);
	}
}

static void *
do_one(void *arg)
{
	char *hostname = arg;
	CLIENT *clnt;
	struct timeval tp;
	void *vp = NULL;

#ifdef DEBUG
	(void) mutex_lock(&tty);
	(void) fprintf(stderr, "sending message to %s\n%s\n", hostname, path);
	(void) mutex_unlock(&tty);
	return (0);
#endif
	tp.tv_sec = PATIENCE;
	tp.tv_usec = 0;
	clnt = clnt_tp_create_timed(hostname, WALLPROG, WALLVERS, nconf, &tp);
	if (clnt == NULL) {
		(void) mutex_lock(&tty);
		(void) fprintf(stderr, "rwall: Can't send to %s\n", hostname);
		clnt_pcreateerror(hostname);
		(void) mutex_unlock(&tty);
		goto errout;
	}

	if (wallproc_wall_1(&path, vp, clnt) != RPC_SUCCESS) {
		(void) mutex_lock(&tty);
		clnt_perror(clnt, hostname);
		(void) mutex_unlock(&tty);
	}
	clnt_destroy(clnt);
errout:
	(void) mutex_lock(&thr_mtx);
	thread_count--;
	(void) mutex_unlock(&thr_mtx);
	free(hostname);
	return (0);
}

static void
usage(void)
{
	(void) fprintf(stderr,
		"Usage: rwall host .... [-n netgroup ....] [-h host ...]\n");
	exit(1);
}

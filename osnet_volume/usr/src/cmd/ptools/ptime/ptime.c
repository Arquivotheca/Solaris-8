/*
 * Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ptime.c	1.8	98/07/26 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <wait.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <libproc.h>

static	int	look(pid_t);
static	void	hr_min_sec(char *, long);
static	void	prtime(char *, timestruc_t *);
static	int	perr(const char *);

static	void	tsadd(timestruc_t *result, timestruc_t *a, timestruc_t *b);
static	void	tssub(timestruc_t *result, timestruc_t *a, timestruc_t *b);
#if SOMEDAY
static	void	tszero(timestruc_t *);
static	int	tsiszero(timestruc_t *);
static	int	tscmp(timestruc_t *a, timestruc_t *b);
#endif

static	char	*command;
static	char	procname[64];

main(int argc, char **argv)
{
	int ctlfd;
	long ctl[2];
	pid_t pid;
	struct siginfo info;
	int status;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	if (argc <= 1) {
		(void) fprintf(stderr,
			"usage:\t%s command [ args ... ]\n", command);
		(void) fprintf(stderr,
			"  (time a command using microstate accounting)\n");
		return (1);
	}

	switch (pid = fork()) {
	case -1:
		(void) fprintf(stderr, "%s: cannot fork\n", command);
		return (2);
	case 0:
		/* open the /proc ctl file and turn on microstate accounting */
		(void) sprintf(procname, "/proc/%d/ctl", (int)getpid());
		ctlfd = open(procname, O_WRONLY);
		ctl[0] = PCSET;
		ctl[1] = PR_MSACCT;
		(void) write(ctlfd, ctl, 2*sizeof (long));
		(void) close(ctlfd);
		(void) execvp(argv[1], &argv[1]);
		(void) fprintf(stderr, "%s: exec failed\n", command);
		if (errno == ENOENT)
			_exit(127);
		else
			_exit(126);
	}

	(void) sprintf("%d", procname, (int)pid);	/* for perr() */
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);
	(void) waitid(P_PID, pid, &info, WEXITED | WNOWAIT);

	(void) look(pid);

	(void) waitpid(pid, &status, 0);

	if (WIFEXITED(status))
		return (WEXITSTATUS(status));
	else
		return ((status & ~WCOREFLG) | 0200);
}

static int
look(pid_t pid)
{
	char pathname[100];
	int rval = 0;
	int fd;
	psinfo_t psinfo;
	prusage_t prusage;
	timestruc_t real, user, sys;
	prusage_t *pup = &prusage;

	if (proc_get_psinfo(pid, &psinfo) < 0)
		return (perr("read psinfo"));

	(void) sprintf(pathname, "/proc/%d/usage", (int)pid);
	if ((fd = open(pathname, O_RDONLY)) < 0)
		return (perr("open usage"));

	if (read(fd, &prusage, sizeof (prusage)) != sizeof (prusage))
		rval = perr("read usage");
	else {
		real = pup->pr_term;
		tssub(&real, &real, &pup->pr_create);
		user = pup->pr_utime;
		sys = pup->pr_stime;
		tsadd(&sys, &sys, &pup->pr_ttime);
		(void) fprintf(stderr, "\n");
		prtime("real", &real);
		prtime("user", &user);
		prtime("sys", &sys);
	}

	(void) close(fd);
	return (rval);
}

static void
hr_min_sec(char *buf, long sec)
{
	if (sec >= 3600)
		(void) sprintf(buf, "%ld:%.2ld:%.2ld",
			sec / 3600, (sec % 3600) / 60, sec % 60);
	else if (sec >= 60)
		(void) sprintf(buf, "%ld:%.2ld",
			sec / 60, sec % 60);
	else {
		(void) sprintf(buf, "%ld", sec);
	}
}

static void
prtime(char *name, timestruc_t *ts)
{
	char buf[32];

	hr_min_sec(buf, ts->tv_sec);
	(void) fprintf(stderr, "%-4s %8s.%.3u\n",
		name, buf, (u_int)ts->tv_nsec/1000000);
}

static int
perr(const char *s)
{
	if (s)
		(void) fprintf(stderr, "%s: ", procname);
	else
		s = procname;
	perror(s);
	return (1);
}

static	void
tsadd(timestruc_t *result, timestruc_t *a, timestruc_t *b)
{
	result->tv_sec = a->tv_sec + b->tv_sec;
	if ((result->tv_nsec = a->tv_nsec + b->tv_nsec) >= 1000000000) {
		result->tv_nsec -= 1000000000;
		result->tv_sec += 1;
	}
}

static	void
tssub(timestruc_t *result, timestruc_t *a, timestruc_t *b)
{
	result->tv_sec = a->tv_sec - b->tv_sec;
	if ((result->tv_nsec = a->tv_nsec - b->tv_nsec) < 0) {
		result->tv_nsec += 1000000000;
		result->tv_sec -= 1;
	}
}

#if SOMEDAY

static	void
tszero(timestruc_t *a)
{
	a->tv_sec = 0;
	a->tv_nsec = 0;
}

static	int
tsiszero(timestruc_t *a)
{
	return (a->tv_sec == 0 && a->tv_nsec == 0);
}

static	int
tscmp(timestruc_t *a, timestruc_t *b)
{
	if (a->tv_sec > b->tv_sec)
		return (1);
	if (a->tv_sec < b->tv_sec)
		return (-1);
	if (a->tv_nsec > b->tv_nsec)
		return (1);
	if (a->tv_nsec < b->tv_nsec)
		return (-1);
	return (0);
}

#endif	/* SOMEDAY */

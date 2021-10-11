/*
 * Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pwait.c	1.6	98/01/30 SMI"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <stropts.h>
#include <poll.h>
#include <procfs.h>
#include <sys/resource.h>

static int count_my_files();
static char *command;

/* slop to account for extra file descriptors opened by libraries we call */
#define	SLOP	5

main(int argc, char **argv)
{
	unsigned long remain = 0;
	struct pollfd *pollfd;
	struct pollfd *pfd;
	struct rlimit rlim;
	char *arg;
	unsigned i;
	int verbose = 0;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	argc--;
	argv++;

	if (argc > 0 && strcmp(argv[0], "-v") == 0) {
		verbose = 1;
		argc--;
		argv++;
	}

	if (argc <= 0) {
		(void) fprintf(stderr, "usage:\t%s [-v] pid ...\n", command);
		(void) fprintf(stderr, "  (wait for processes to terminate)\n");
		(void) fprintf(stderr,
			"  -v: verbose; report terminations to standard out\n");
		return (2);
	}

	/* make sure we have enough file descriptors */
	if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
		int nfiles = count_my_files();

		if (rlim.rlim_cur < argc + nfiles + SLOP) {
			rlim.rlim_cur = argc + nfiles + SLOP;
			if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
				(void) fprintf(stderr,
					"%s: insufficient file descriptors\n",
					command);
				return (2);
			}
		}
	}

	pollfd = (struct pollfd *)malloc(argc*sizeof (struct pollfd));
	if (pollfd == NULL) {
		perror("malloc");
		return (2);
	}

	for (i = 0; i < argc; i++) {
		char psinfofile[100];

		arg = argv[i];
		if (strchr(arg, '/') != NULL)
			(void) strncpy(psinfofile, arg, sizeof (psinfofile));
		else {
			(void) strcpy(psinfofile, "/proc/");
			(void) strncat(psinfofile, arg, sizeof (psinfofile)-6);
		}
		(void) strncat(psinfofile, "/psinfo",
			sizeof (psinfofile)-strlen(psinfofile));

		pfd = &pollfd[i];
		if ((pfd->fd = open(psinfofile, O_RDONLY)) >= 0) {
			remain++;
			/*
			 * We set POLLPRI to detect system processes.
			 * We will get POLLNVAL below for a POLLPRI
			 * requested event on a system process.
			 */
			pfd->events = POLLPRI;
			pfd->revents = 0;
		} else if (errno == ENOENT) {
			(void) fprintf(stderr, "%s: no such process: %s\n",
				command, arg);
		} else {
			perror(arg);
		}
	}

	while (remain != 0) {
		while (poll(pollfd, argc, INFTIM) < 0) {
			if (errno != EAGAIN) {
				perror("poll");
				return (2);
			}
			(void) sleep(2);
		}
		for (i = 0; i < argc; i++) {
			pfd = &pollfd[i];
			if (pfd->fd < 0 || (pfd->revents & ~POLLPRI) == 0) {
				/*
				 * We don't care if a non-system process
				 * stopped.  Don't check for that again.
				 */
				pfd->events = 0;
				pfd->revents = 0;
				continue;
			}

			if (verbose) {
				arg = argv[i];
				if (pfd->revents & POLLHUP) {
					psinfo_t psinfo;

					if (pread(pfd->fd, &psinfo,
					    sizeof (psinfo), (off_t)0)
					    == sizeof (psinfo)) {
						(void) printf(
					"%s: terminated, wait status 0x%.4x\n",
							arg, psinfo.pr_wstat);
					} else {
						(void) printf(
						    "%s: terminated\n", arg);
					}
				}
				if (pfd->revents & POLLNVAL)
					(void) printf("%s: system process\n",
						arg);
				if (pfd->revents & ~(POLLPRI|POLLHUP|POLLNVAL))
					(void) printf("%s: unknown error\n",
						arg);
			}

			(void) close(pfd->fd);
			pfd->fd = -1;
			remain--;
		}
	}

	return (0);
}

static int
count_my_files()
{
	DIR *dirp;
	struct dirent *dentp;
	int nofiles = 0;

	if ((dirp = opendir("/proc/self/fd")) == NULL)
		return (10);	/* wild-ass guess */

	/* count each open file */
	while ((dentp = readdir(dirp)) != NULL) {
		if (isdigit(dentp->d_name[0]))
			nofiles++;
	}

	(void) closedir(dirp);
	return (nofiles);
}

/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pstop.c	1.7	99/03/23 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <libproc.h>

static	int	stop(char *);
static	int	perr(char *);

static	char	*command;
static	char	*procname;

int
main(int argc, char **argv)
{
	int rc = 0;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	if (argc <= 1) {
		(void) fprintf(stderr, "usage:\t%s pid ...\n", command);
		(void) fprintf(stderr,
			"  (stop processes with /proc request)\n");
		return (2);
	}

	while (--argc > 0)
		rc += stop(*++argv);

	return (rc);
}

static int
stop(char *arg)
{
	char ctlfile[100];
	long ctl[1];
	int ctlfd, gcode;
	pid_t pid;

	procname = arg;		/* for perr() */
	if ((pid = proc_arg_psinfo(arg, PR_ARG_PIDS, NULL, &gcode)) == -1) {
		(void) fprintf(stderr, "%s: cannot control %s: %s\n",
			command, arg, Pgrab_error(gcode));
		return (1);
	}

	(void) sprintf(ctlfile, "/proc/%d/ctl", (int)pid);
	errno = 0;
	if ((ctlfd = open(ctlfile, O_WRONLY)) >= 0) {
		ctl[0] = PCDSTOP;
		(void) write(ctlfd, ctl, sizeof (long));
		(void) close(ctlfd);
	}

	return (perr(NULL));
}

static int
perr(char *s)
{
	if (errno == 0)
		return (0);
	if (s)
		(void) fprintf(stderr, "%s: ", procname);
	else
		s = procname;
	perror(s);
	return (1);
}

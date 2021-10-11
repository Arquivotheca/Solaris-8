/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pwdx.c	1.16	99/12/02 SMI"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libproc.h>
#include <sys/param.h>

static char *command;

static int
show_cwd(const char *arg)
{
	char cwd[MAXPATHLEN], proc[128];
	psinfo_t p;
	int gcode;

	if (proc_arg_psinfo(arg, PR_ARG_PIDS, &p, &gcode) == -1) {
		(void) fprintf(stderr, "%s: cannot examine %s: %s\n",
		    command, arg, Pgrab_error(gcode));
		return (1);
	}

	(void) snprintf(proc, sizeof (proc), "/proc/%d/cwd", (int)p.pr_pid);

	if (proc_dirname(proc, cwd, MAXPATHLEN) == NULL) {
		(void) fprintf(stderr, "%s: cannot resolve cwd for %s: %s\n",
		    command, arg, strerror(errno));
		return (1);
	}

	(void) printf("%d:\t%s\n", (int)p.pr_pid, cwd);
	return (0);
}

int
main(int argc, char **argv)
{
	int retc = 0;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	if (argc <= 1) {
		(void) fprintf(stderr, "usage:\t%s pid ...\n", command);
		(void) fprintf(stderr, "  (show process working directory)\n");
	}

	while (--argc >= 1)
		retc += show_cwd(*++argv);

	return (retc);
}

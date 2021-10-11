/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pcred.c	1.9	99/03/23 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <libproc.h>

static	int	look(char *);
static	int	perr(char *);

static	char	*command;
static	char	*procname;
static 	int	all = 0;

int
main(int argc, char **argv)
{
	int rc = 0;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	if (argc <= 1) {
		(void) fprintf(stderr, "usage:\t%s { pid | core } ...\n"
		    "  (report process credentials)\n", command);
		return (2);
	}

	if (argc > 1 && strcmp(argv[1], "-a") == 0) {
		all = 1;
		argc--;
		argv++;
	}

	while (--argc > 0)
		rc += look(*++argv);

	return (rc);
}

static int
look(char *arg)
{
	uint_t ngroups = NGROUPS_MAX;	/* initial guess at number of groups */
	struct ps_prochandle *Pr;
	prcred_t *prcred;
	int gcode;

	procname = arg;		/* for perr() */

	if ((Pr = proc_arg_grab(arg, PR_ARG_ANY, PGRAB_RETAIN | PGRAB_FORCE |
	    PGRAB_RDONLY | PGRAB_NOSTOP, &gcode)) == NULL) {
		(void) fprintf(stderr, "%s: cannot examine %s: %s\n",
		    command, arg, Pgrab_error(gcode));
		return (1);
	}

	for (;;) {
		prcred = malloc(sizeof (prcred_t) +
			(ngroups - 1) * sizeof (gid_t));
		if (prcred == NULL)
			return (perr("malloc"));
		if (Pcred(Pr, prcred, ngroups) == -1) {
			free(prcred);
			return (perr("cred"));
		}
		if (ngroups >= prcred->pr_ngroups)    /* got all the groups */
			break;
		/* reallocate and try again */
		free(prcred);
		ngroups = prcred->pr_ngroups;
	}

	if (Pstate(Pr) == PS_DEAD)
		(void) printf("core of %d:\t", (int)Pstatus(Pr)->pr_pid);
	else
		(void) printf("%d:\t", (int)Pstatus(Pr)->pr_pid);

	if (!all &&
	    prcred->pr_euid == prcred->pr_ruid &&
	    prcred->pr_ruid == prcred->pr_suid)
		(void) printf("e/r/suid=%d  ",
			(int)prcred->pr_euid);
	else
		(void) printf("euid=%d ruid=%d suid=%d  ",
			(int)prcred->pr_euid,
			(int)prcred->pr_ruid,
			(int)prcred->pr_suid);

	if (!all &&
	    prcred->pr_egid == prcred->pr_rgid &&
	    prcred->pr_rgid == prcred->pr_sgid)
		(void) printf("e/r/sgid=%d\n",
			(int)prcred->pr_egid);
	else
		(void) printf("egid=%d rgid=%d sgid=%d\n",
			(int)prcred->pr_egid,
			(int)prcred->pr_rgid,
			(int)prcred->pr_sgid);

	if (prcred->pr_ngroups != 0 &&
	    (all || prcred->pr_ngroups != 1 ||
	    prcred->pr_groups[0] != prcred->pr_rgid)) {
		int i;

		(void) printf("\tgroups:");
		for (i = 0; i < prcred->pr_ngroups; i++)
			(void) printf(" %d", (int)prcred->pr_groups[i]);
		(void) printf("\n");
	}

	Prelease(Pr, 0);
	free(prcred);
	return (0);
}

static int
perr(char *s)
{
	if (s)
		(void) fprintf(stderr, "%s: ", procname);
	else
		s = procname;
	perror(s);
	return (1);
}

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)renice.c	1.11	98/04/14 SMI"	/* SVr4.0 1.3	*/

/*
 *
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <pwd.h>
#include <nl_types.h>
#include <locale.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

static void usage(void);
static int donice(int which, id_t who, int prio, int increment, char *who_s);
static int parse_obsolete_options(int argc, char **argv);

#define	PRIO_MAX		19
#define	PRIO_MIN		-20
#define	RENICE_DEFAULT_PRIORITY	10
#define	RENICE_PRIO_INCREMENT	1
#define	RENICE_PRIO_ABSOLUTE	0

/*
 * Change the priority (nice) of processes
 * or groups of processes which are already
 * running.
 */

main(int argc, char *argv[])
{
	register int c;
	register int optflag = 0;
	int which = PRIO_PROCESS;
	id_t who = 0;
	int errs = 0;
	char *end_ptr;
	int incr = RENICE_DEFAULT_PRIORITY;
	int prio_type = RENICE_PRIO_INCREMENT;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	if (argc < 2)
		(void) usage();

	/*
	 * There is ambiguity in the renice options spec.
	 * If argv[1] is in the valid range of priority values then
	 * treat it as a priority.  Otherwise, treat it as a pid.
	 */

	if (isdigit(argv[1][0])) {
		if (strtol(argv[1], (char **)NULL, 10) > (PRIO_MAX+1)) {
			argc--;			/* renice pid ... */
			argv++;
			prio_type = RENICE_PRIO_INCREMENT;
		} else {			/* renice priority ... */
			exit(parse_obsolete_options(argc, argv));
		}
	} else if ((argv[1][0] == '-' || argv[1][0] == '+') &&
			isdigit(argv[1][1])) {	/* renice priority ... */

		exit(parse_obsolete_options(argc, argv));

	} else {	/* renice [-n increment] [-g|-p|-u] ID ... */

		while ((c = getopt(argc, argv, "n:gpu")) != -1) {
			switch (c) {
			case 'n':
				incr = strtol(optarg, &end_ptr, 10);
				prio_type = RENICE_PRIO_INCREMENT;
				if (*end_ptr != '\0')
					usage();
				break;
			case 'g':
				which = PRIO_PGRP;
				optflag++;
				break;
			case 'p':
				which = PRIO_PROCESS;
				optflag++;
				break;
			case 'u':
				which = PRIO_USER;
				optflag++;
				break;
			default:
				usage();
			}
		}

		argc -= optind;
		argv += optind;

		if (argc == 0 || (optflag > 1))
			usage();
	}

	for (; argc > 0; argc--, argv++) {
		if ((which == PRIO_USER) && !isdigit(argv[0][0])) {
			register struct passwd *pwd = getpwnam(*argv);

				if (pwd == NULL) {
					(void) fprintf(stderr,
			gettext("renice: unknown user: %s\n"), *argv);
					continue;
				} else
					who = pwd->pw_uid;
		} else {
			who = strtol(*argv, &end_ptr, 10);
			if ((who < 0) || (*end_ptr != '\0')) {
				(void) fprintf(stderr,
			gettext("renice: bad value: %s\n"), *argv);
				continue;
			}
		}
		errs += donice(which, who, incr, prio_type, *argv);
	}
	return (errs != 0);
	/* NOTREACHED */
}


static int
parse_obsolete_options(int argc, char *argv[])
{
	int which = PRIO_PROCESS;
	id_t who = 0;
	int prio;
	int errs = 0;
	char *end_ptr;

	argc--;
	argv++;

	if (argc < 2) {
		usage();
	}

	prio = strtol(*argv, &end_ptr, 10);
	if (*end_ptr != '\0') {
		usage();
	}

	if (prio == 20) {
		(void) fprintf(stderr,
			gettext("renice: nice value 20 rounded down to 19\n"));
	}

	argc--;
	argv++;

	for (; argc > 0; argc--, argv++) {
		if (strcmp(*argv, "-g") == 0) {
			which = PRIO_PGRP;
			continue;
		}
		if (strcmp(*argv, "-u") == 0) {
			which = PRIO_USER;
			continue;
		}
		if (strcmp(*argv, "-p") == 0) {
			which = PRIO_PROCESS;
			continue;
		}
		if (which == PRIO_USER && !isdigit(argv[0][0])) {
			register struct passwd *pwd = getpwnam(*argv);

			if (pwd == NULL) {
				(void) fprintf(stderr,
					gettext("renice: unknown user: %s\n"),
					*argv);
				continue;
			}
			who = pwd->pw_uid;
		} else {
			who = strtol(*argv, &end_ptr, 10);
			if ((who < 0) || (*end_ptr != '\0')) {
				(void) fprintf(stderr,
			gettext("renice: bad value: %s\n"), *argv);
				continue;
			}
		}
		errs += donice(which, who, prio, RENICE_PRIO_ABSOLUTE, *argv);
	}
	return (errs != 0);
}



static int
donice(int which, id_t who, int prio, int increment, char *who_s)
{
	int oldprio;
	extern int errno;

	oldprio = getpriority(which, who);

	if (oldprio == -1 && errno) {
		(void) fprintf(stderr, gettext("renice: %d:"), who);
		perror("getpriority");
		return (1);
	}
	if (increment)
		prio = oldprio + prio;

	if (setpriority(which, who, prio) < 0) {
		(void) fprintf(stderr, gettext("renice: %s:"), who_s);
		if (errno == EPERM && prio < oldprio)
			(void) fprintf(stderr, gettext(
			    " Cannot lower nice value.\n"));
		else
			perror("setpriority");
		return (1);
	}

	return (0);
}

static void
usage()
{
	(void) fprintf(stderr,
		gettext("usage: renice [-n increment] "
		"[-g | -p | -u] ID ...\n"));
	(void) fprintf(stderr,
		gettext("       renice priority "
		"[-p] pid ... [-g pgrp ...] [-p pid ...] [-u user ...]\n"));
	(void) fprintf(stderr,
		gettext("       renice priority "
		" -g pgrp ... [-g pgrp ...] [-p pid ...] [-u user ...]\n"));
	(void) fprintf(stderr,
		gettext("       renice priority "
		" -u user ... [-g pgrp ...] [-p pid ...] [-u user ...]\n"));
	(void) fprintf(stderr,
		gettext("  where %d <= priority <= %d\n"), PRIO_MIN, PRIO_MAX);
	exit(2);
}

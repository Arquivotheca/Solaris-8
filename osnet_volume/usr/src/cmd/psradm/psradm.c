/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)psradm.c	1.7	98/12/07 SMI"

#include <sys/types.h>
#include <sys/procset.h>
#include <sys/processor.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>
#include <utmpx.h>

static char	*cmdname;	/* command name for messages */

static char	verbose;	/* non-zero if the -v option has been given */
static char	all_flag;	/* non-zero if the -a option has been given */
static char	log_open;	/* non-zero if openlog() has been called */

static struct utmpx ut;		/* structure for logging to /etc/wtmpx. */

static char	*basename(char *);

static void
usage(void)
{
	(void) fprintf(stderr, "usage: \n\t%s -f|-n|-i [-v] processor_id ...\n"
		"\t%s -a -f|-n|-i [-v]\n", cmdname, cmdname);
}

/*
 * Find base name of filename.
 */
static char *
basename(char *cp)
{
	char *sp;

	if ((sp = strrchr(cp, '/')) != NULL)
		return (sp + 1);
	return (cp);
}


typedef struct _psr_action {
	int	p_op;
	char	*p_state;
	char	*p_action;
	char	*p_wtmp;
} psr_action_t;

static psr_action_t psr_action[] = {
	{ P_ONLINE,	"on-line",	"brought",	"on"	},
	{ P_OFFLINE,	"off-line",	"taken",	"off"	},
	{ P_NOINTR,	"no-intr",	"set to",	"ni"	},
};

static int	psr_actions = sizeof (psr_action) / sizeof (psr_action_t);

static psr_action_t *
psr_action_lookup(int action)
{
	int	i;

	for (i = 0; i < psr_actions; ++i) {
		if (psr_action[i].p_op == action) {
			return (&psr_action[i]);
		}
	}
	/* NOTREACHED */
}

/*
 * Turn CPU online or offline.
 *	Return non-zero if a processor was found.
 *	Print messages and update wtmp and the system log.
 *	If mustexist is set, it is an error if a processor isn't there.
 */

static int
psr_set_state(processorid_t cpu, int action, psr_action_t *pac, int mustexist)
{
	int	old_state;
	int	err;
	time_t	now;
	char	buf[80];

	old_state = p_online(cpu, action);
	if (old_state < 0) {
		if (errno == EINVAL && !mustexist)
			return (0);	/* no such processor */
		err = errno;		/* in case sprintf smashes errno */
		(void) sprintf(buf, "%s: processor %d", cmdname, cpu);
		errno = err;
		perror(buf);
		return (-1);
	}

	if (old_state == action) {
		if (verbose)
			(void) printf("processor %d already %s.\n", cpu,
			    pac->p_state);
		return (1);		/* no change */
	}

	(void) sprintf(buf, "processor %d %s %s.", cpu, pac->p_action,
	    pac->p_state);

	if (verbose)
		(void) printf("%s\n", buf);

	/*
	 * Log the change.
	 */
	if (!log_open) {
		log_open = 1;
		openlog(cmdname, LOG_CONS, LOG_USER);	/* open syslog */
		(void) setlogmask(LOG_UPTO(LOG_INFO));

		ut.ut_pid = getpid();
		ut.ut_type = USER_PROCESS;
		(void) strncpy(ut.ut_user, "psradm", sizeof (ut.ut_user) - 1);
	}

	syslog(LOG_INFO, buf);

	/*
	 * Update wtmp.
	 */
	(void) sprintf(ut.ut_line, PSRADM_MSG, cpu, pac->p_wtmp);
	(void) time(&now);
	ut.ut_xtime = now;
	updwtmpx(WTMPX_FILE, &ut);

	return (1);	/* the processor exists and no errors occurred */
}


static int
do_range(processorid_t first, processorid_t last, int action,
    psr_action_t *pac)
{
	processorid_t cpu;
	int error = 0;
	int rv;
	int found_one = 0;

	for (cpu = first; cpu <= last; cpu++) {
		if ((rv = psr_set_state(cpu, action, pac, 0)) > 0)
			found_one = 1;
		else if (rv < 0)
			error = 1;
	}
	if (!found_one && error == 0) {
		(void) fprintf(stderr, "%s: no processors in range %d-%d\n",
		    cmdname, first, last);
		error = 1;
	}
	return (error);
}


int
main(int argc, char *argv[])
{
	int	c;
	int	action = 0;
	processorid_t	cpu;
	int	ncpus;
	char	*errptr;
	int	errors;
	psr_action_t	*pac;

	cmdname = basename(argv[0]);

	while ((c = getopt(argc, argv, "afinv")) != EOF) {
		switch (c) {

		case 'a':		/* applies to all possible CPUs */
			all_flag = 1;
			break;

		case 'f':
		case 'i':
		case 'n':
			if (action != 0 && action != c) {
				(void) fprintf(stderr,
				    "%s: options -f, -n and -i are "
				    "mutually exclusive.\n", cmdname);
				usage();
				return (2);
			}
			action = c;
			break;

		case 'v':
			verbose = 1;
			break;

		default:
			usage();
			return (2);
		}
	}

	switch (action) {
	case 'f':
		action = P_OFFLINE;
		break;
	case 'i':
		action = P_NOINTR;
		break;
	case 'n':
		action = P_ONLINE;
		break;
	default:
		(void) fprintf(stderr, "%s: option -f, -n or -i must "
		    "be specified.\n", cmdname);
		usage();
		return (2);
	}

	pac = psr_action_lookup(action);

	errors = 0;
	if (all_flag) {
		if (argc != optind) {
			usage();
			return (2);
		}
		ncpus = sysconf(_SC_NPROCESSORS_CONF);
		if (ncpus < 0) {
			perror("sysconf _SC_NPROCESSORS_CONF");
			return (1);
		}
		for (cpu = 0; ncpus > 0; cpu++) {
			if (psr_set_state(cpu, action, pac, 0))
				ncpus--;
		}
	} else {
		argc -= optind;
		if (argc <= 0) {
			usage();	/* not enough arguments */
			return (2);
		}
		for (argv += optind; argc > 0; argv++, argc--) {
			if (strchr(*argv, '-') == NULL) {
				/* individual processor id */
				cpu = (processorid_t)
				    strtol(*argv, &errptr, 10);
				if (errptr != NULL && *errptr != '\0') {
					(void) fprintf(stderr,
					    "%s: invalid processor"
					    " ID %s\n", cmdname, *argv);
					errors = 2;
					continue;
				}
				if (psr_set_state(cpu, action, pac, 1) <= 0)
					errors = 1;
			} else {
				/* range of processors */
				processorid_t first, last;

				first = (processorid_t)
				    strtol(*argv, &errptr, 10);
				if (*errptr++ != '-') {
					(void) fprintf(stderr,
					    "%s: invalid processor"
					    " range %s\n", cmdname, *argv);
					errors = 2;
					continue;
				}
				last = (processorid_t)
				    strtol(errptr, &errptr, 10);
				if ((errptr != NULL && *errptr != '\0') ||
				    last < first || first < 0) {
					(void) fprintf(stderr,
					    "%s: invalid processor"
					    " range %s\n", cmdname, *argv);
					errors = 2;
					continue;
				}
				if (do_range(first, last, action, pac))
					errors = 1;
			}
		}
	}
	if (log_open) {
		closelog();
	}
	return (errors);
}

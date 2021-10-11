/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)psrset.c	1.5	98/12/07 SMI"

/*
 * psrset - create and manage processor sets
 */

#include <sys/types.h>
#include <sys/procset.h>
#include <sys/processor.h>
#include <sys/pset.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <varargs.h>

#if !defined(TEXT_DOMAIN)		/* should be defined by cc -D */
#define	TEXT_DOMAIN 	"SYS_TEST"	/* Use this only if it wasn't */
#endif

#define	PROCDIR	"/proc"

static char *cmdname = "psrset";

static int	do_cpu(psetid_t pset, processorid_t cpu, int print,
		    int mustexist);
static int	do_range(psetid_t pset, processorid_t first,
		    processorid_t last, int print);
static int	do_info(psetid_t pset);
static int	do_destroy(psetid_t pset);
static int	do_intr(psetid_t pset, int flag);
static void	create_out(psetid_t pset);
static void	assign_out(processorid_t cpu, psetid_t old, psetid_t new);
static void	info_out(psetid_t pset, int type, uint_t numcpus,
		    processorid_t *cpus);
static void	print_out(processorid_t cpu, psetid_t pset);
static void	query_out(id_t pid, psetid_t cpu);
static void	bind_out(id_t pid, psetid_t old, psetid_t new);
static int	info_all(void);
static int	print_all(void);
static int	query_all(void);
static void	errmsg(int err, char *msg, ...);
static void	usage(void);
static int	comparepset(const void *x, const void *y);
static void	exec_cmd(psetid_t pset, char **argv);

int
main(int argc, char *argv[])
{
	extern int optind;
	int	c;
	id_t	pid;
	processorid_t	cpu;
	psetid_t	pset, old_pset;
	char	*errptr;
	char	create = 0;
	char	destroy = 0;
	char	assign = 0;
	char	remove = 0;
	char	info = 0;
	char	bind = 0;
	char	unbind = 0;
	char	query = 0;
	char	print = 0;
	char	enable = 0;
	char	disable = 0;
	char	exec = 0;
	int	errors;

	cmdname = argv[0];	/* put actual command name in messages */

	(void) setlocale(LC_ALL, "");	/* setup localization */
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "cdarpibqunfe")) != EOF) {
		switch (c) {

		case 'c':
			create = 1;
			break;

		case 'd':
			destroy = 1;
			break;

		case 'e':
			exec = 1;
			break;

		case 'a':
			assign = 1;
			break;

		case 'r':
			remove = 1;
			pset = PS_NONE;
			break;

		case 'p':
			print = 1;
			pset = PS_QUERY;
			break;

		case 'i':
			info = 1;
			break;

		case 'b':
			bind = 1;
			break;

		case 'u':
			unbind = 1;
			pset = PS_NONE;
			break;

		case 'q':
			query = 1;
			pset = PS_QUERY;
			break;

		case 'f':
			disable = 1;
			break;

		case 'n':
			enable = 1;
			break;

		default:
			usage();
			return (2);
		}
	}


	/*
	 * Make sure that at most one of the options was specified.
	 */
	c = create + destroy + assign + remove + print +
	    info + bind + unbind + query + disable + enable + exec;
	if (c < 1) {				/* nothing specified */
		info = 1;			/* default is to get info */
	} else if (c > 1) {
		errmsg(0, gettext("options are mutually exclusive"));
		usage();
		return (2);
	}

	errors = 0;
	argc -= optind;
	argv += optind;

	/*
	 * Handle query of all processes.
	 */
	if (query && argc == 0)
		return (query_all());

	/*
	 * Handle print for all processors.
	 */
	if (print && argc == 0)
		return (print_all());

	/*
	 * Handle info for all processors.
	 */
	if (info && argc == 0)
		return (info_all());

	/*
	 * Get processor set id.
	 */
	if (assign || bind || disable || enable || exec) {
		if (argc < 1) {
			/* must specify processor set */
			errmsg(0, gettext("must specify processor set"));
			usage();
			return (2);
		}
		pset = strtol(*argv, &errptr, 10);
		if (errptr != NULL && *errptr != '\0' || pset < 0) {
			errmsg(0, gettext("invalid processor set ID %s"),
			    *argv);
			return (1);
		}
		argv++;
		argc--;
	}

	if (create) {
		if (pset_create(&pset) != 0) {
			errmsg(errno,
			    gettext("could not create processor set"));
			return (-1);
		} else {
			create_out(pset);
			if (argc == 0)
				return (0);
		}
	} else if (info || destroy) {
		if (argc == 0) {
			errmsg(0, gettext("must specify at least one "
			    "processor set"));
			usage();
			return (2);
		}
		/*
		 * Go through listed processor sets.
		 */
		for (; argc > 0; argv++, argc--) {
			pset = (psetid_t)strtol(*argv, &errptr, 10);
			if (errptr != NULL && *errptr != '\0') {
				errmsg(0,
				    gettext("invalid processor set ID %s"),
				    *argv);
				errors = 2;
				continue;
			}
			if (info) {
				errors = do_info(pset);
			} else {
				errors = do_destroy(pset);
			}
		}
	} else if (enable) {
		errors = do_intr(pset, P_ONLINE);
	} else if (disable) {
		errors = do_intr(pset, P_NOINTR);
	} else if (exec) {
		if (argc == 0) {
			errmsg(0, gettext("must specify command"));
			usage();
			return (2);
		}
		exec_cmd(pset, argv);
		/* if returning, must have had an error */
		return (2);
	}

	if (create || assign || remove || print) {
		/*
		 * Perform function for each processor specified.
		 */
		if (argc == 0) {
			errmsg(0,
			    gettext("must specify at least one processor"));
			usage();
			return (2);
		}

		/*
		 * Go through listed processors.
		 */
		for (; argc > 0; argv++, argc--) {
			if (strchr(*argv, '-') == NULL) {
				/* individual processor id */
				cpu = (processorid_t)strtol(*argv, &errptr, 10);
				if (errptr != NULL && *errptr != '\0') {
					errmsg(0,
					    gettext("invalid processor ID %s"),
					    *argv);
					errors = 2;
					continue;
				}
				if (do_cpu(pset, cpu, print, 1))
					errors = 1;
			} else {
				/* range of processors */
				processorid_t first, last;

				first = (processorid_t)
				    strtol(*argv, &errptr, 10);
				if (*errptr++ != '-') {
					errmsg(0, gettext(
					    "invalid processor range %s"),
					    *argv);
					errors = 2;
					continue;
				}
				last = (processorid_t)
				    strtol(errptr, &errptr, 10);
				if ((errptr != NULL && *errptr != '\0') ||
				    last < first || first < 0) {
					errmsg(0, gettext(
					    "invalid processor range %s"),
					    *argv);
					errors = 2;
					continue;
				}
				if (do_range(pset, first, last, print))
					errors = 1;
			}
		}
	} else if (bind || unbind || query) {
		/*
		 * Perform function for each pid specified.
		 */
		if (argc == 0) {
			errmsg(0, gettext("must specify at least one pid"));
			usage();
			return (2);
		}

		/*
		 * Go through listed processes.
		 */
		for (; argc > 0; argv++, argc--) {
			pid = (id_t)strtol(*argv, &errptr, 10);
			if (errptr != NULL && *errptr != '\0') {
				errmsg(0,
				    gettext("invalid process ID (pid) %s"),
				    *argv);
				errors = 2;
				continue;
			}
			if (pset_bind(pset, P_PID, pid, &old_pset)
			    != 0) {
				char	*msg;

				switch (pset) {
				case PS_NONE:
					msg = gettext("cannot unbind pid %d");
					break;
				case PS_QUERY:
					msg = gettext("cannot query pid %d");
					break;
				default:
					msg = gettext("cannot bind pid %d");
					break;
				}
				errmsg(errno, msg, pid);
				errors = 1;
				continue;
			}
			if (query)
				query_out(pid, old_pset);
			else
				bind_out(pid, old_pset, pset);
		}
	}
	return (errors);
}

static int
do_cpu(psetid_t pset, processorid_t cpu, int print, int mustexist)
{
	psetid_t old_pset;
	char *msg;
	int err;

	if (pset_assign(pset, cpu, &old_pset) != 0) {
		if (errno == EINVAL && !mustexist)
			return (EINVAL);
		err = errno;

		switch (pset) {
		case PS_NONE:
			msg = gettext("cannot remove processor %d");
			break;
		case PS_QUERY:
			msg = gettext("cannot query processor %d");
			break;
		default:
			msg = gettext("cannot assign processor %d");
			break;
		}
		errmsg(err, msg, cpu);
		return (err);
	}
	if (print)
		print_out(cpu, old_pset);
	else
		assign_out(cpu, old_pset, pset);
	return (0);
}

static int
do_range(psetid_t pset, processorid_t first, processorid_t last, int print)
{
	processorid_t cpu;
	int error = 0;
	int err;
	int found_one = 0;

	for (cpu = first; cpu <= last; cpu++) {
		if ((err = do_cpu(pset, cpu, print, 0)) == 0)
			found_one = 1;
		else if (err != EINVAL)
			error = 1;
	}
	if (!found_one && error == 0) {
		errmsg(0, gettext("no processors in range %d-%d"),
		    first, last);
		error = 1;
	}
	return (error);
}

static int
do_info(psetid_t pset)
{
	int	type;
	uint_t	numcpus;
	processorid_t	*cpus;

	numcpus = (uint_t)sysconf(_SC_NPROCESSORS_CONF);
	cpus = (processorid_t *)
	    malloc(numcpus * sizeof (processorid_t));
	if (cpus == NULL) {
		errmsg(0, gettext("memory allocation failed"));
		return (1);
	}
	if (pset_info(pset, &type, &numcpus, cpus) != 0) {
		errmsg(errno, gettext("cannot get info for processor set %d"),
		    pset);
		free(cpus);
		return (1);
	}
	info_out(pset, type, numcpus, cpus);
	free(cpus);
	return (0);
}

static int
do_destroy(psetid_t pset)
{
	if (pset_destroy(pset) != 0) {
		errmsg(errno, gettext("could not remove processor set %d"),
		    pset);
		return (1);
	}
	(void) printf("%s %d\n", gettext("removed processor set"), pset);
	return (0);
}

static int
do_intr(psetid_t pset, int flag)
{
	uint_t i, numcpus;
	processorid_t *cpus;
	int error = 0;

	numcpus = (uint_t)sysconf(_SC_NPROCESSORS_CONF);
	cpus = (processorid_t *)
	    malloc(numcpus * sizeof (processorid_t));
	if (cpus == NULL) {
		errmsg(0, gettext("memory allocation failed"));
		return (1);
	}
	if (pset_info(pset, NULL, &numcpus, cpus) != 0) {
		errmsg(errno, gettext(
		    "cannot set interrupt status for processor set %d"),
		    pset);
		free(cpus);
		return (1);
	}
	for (i = 0; i < numcpus; i++) {
		int status = p_online(cpus[i], P_STATUS);
		if (status != P_OFFLINE && status != P_POWEROFF &&
		    status != flag) {
			if (p_online(cpus[i], flag) == -1) {
				errmsg(errno, gettext("processor %d"),
				    cpus[i]);
				error = 1;
			}
		}
	}
	free(cpus);
	return (error);
}

/*
 * Query the type and CPUs for all active processor sets in the system.
 * We do this by getting the processor sets for each CPU in the system,
 * sorting the list, and displaying the information for each unique
 * processor set.  We also try to find any processor sets that may not
 * have processors assigned to them.  This assumes that the kernel will
 * generally assign low-numbered processor set id's and do aggressive
 * recycling of the id's of destroyed sets.  Yeah, this is ugly.
 */
static int
info_all(void)
{
	psetid_t	*psetlist;
	psetid_t	lastpset, pset;
	int	numpsets = 0, maxpsets;
	uint_t	numcpus;
	int	cpuid;
	int	i;
	int	errors = 0;

	numcpus = (uint_t)sysconf(_SC_NPROCESSORS_CONF);
	maxpsets = numcpus * 3;		/* should be "enough" */
	psetlist = (psetid_t *) malloc(sizeof (psetid_t) * maxpsets);
	if (psetlist == NULL) {
		errmsg(0, gettext("memory allocation failed"));
		return (1);
	}
	for (cpuid = 0; numcpus != 0; cpuid++) {
		if (pset_assign(PS_QUERY, cpuid, &psetlist[numpsets])
		    == 0) {
			numcpus--;
			numpsets++;
		}
	}
	for (pset = 0; pset < maxpsets; pset++) {
		if (pset_info(pset, NULL, NULL, NULL) == 0)
			psetlist[numpsets++] = pset;
	}
	qsort(psetlist, (size_t)numpsets, sizeof (psetid_t), comparepset);
	lastpset = PS_NONE;
	for (i = 0; i < numpsets; i++) {
		pset = psetlist[i];
		if (pset == lastpset)
			continue;
		lastpset = pset;
		if (do_info(pset))
			errors = 1;
	}
	free(psetlist);
	return (errors);
}

static int
comparepset(const void *x, const void *y)
{
	psetid_t *a = (psetid_t *)x;
	psetid_t *b = (psetid_t *)y;

	if (*a > *b)
		return (1);
	if (*a < *b)
		return (-1);
	return (0);
}

/*
 * Output for create.
 */
static void
create_out(psetid_t pset)
{
	(void) printf("%s %d\n", gettext("created processor set"), pset);
}

/*
 * Output for assign.
 */
static void
assign_out(processorid_t cpu, psetid_t old, psetid_t new)
{
	if (old == PS_NONE) {
		if (new == PS_NONE)
			(void) printf(gettext("processor %d: was not assigned,"
			    " now not assigned\n"), cpu);
		else
			(void) printf(gettext("processor %d: was not assigned,"
			    " now %d\n"), cpu, new);
	} else {
		if (new == PS_NONE)
			(void) printf(gettext("processor %d: was %d, "
			    "now not assigned\n"), cpu, old);
		else
			(void) printf(gettext("processor %d: was %d, "
			    "now %d\n"), cpu, old, new);
	}
}

/*
 * Output for query.
 */
static void
query_out(id_t pid, psetid_t pset)
{
	if (pset == PS_NONE)
		(void) printf(gettext("process id %d: not bound\n"), pid);
	else
		(void) printf(gettext("process id %d: %d\n"), pid, pset);
}

/*
 * Output for info.
 */
static void
info_out(psetid_t pset, int type, uint_t numcpus, processorid_t *cpus)
{
	int i;
	if (type == PS_SYSTEM)
		(void) printf(gettext("system processor set %d:"), pset);
	else
		(void) printf(gettext("user processor set %d:"), pset);
	if (numcpus == 0)
		(void) printf(gettext(" empty"));
	else if (numcpus > 1)
		(void) printf(gettext(" processors"));
	else
		(void) printf(gettext(" processor"));
	for (i = 0; i < numcpus; i++)
		(void) printf(" %d", cpus[i]);
	(void) printf("\n");
}

/*
 * Output for print.
 */
static void
print_out(processorid_t cpu, psetid_t pset)
{
	if (pset == PS_NONE)
		(void) printf(gettext("processor %d: not assigned\n"), cpu);
	else
		(void) printf(gettext("processor %d: %d\n"), cpu, pset);
}

/*
 * Output for bind.
 */
static void
bind_out(id_t pid, psetid_t old, psetid_t new)
{
	if (old == PS_NONE) {
		if (new == PS_NONE)
			(void) printf(gettext("process id %d: was not bound, "
				"now not bound\n"), pid);
		else
			(void) printf(gettext("process id %d: was not bound, "
				"now %d\n"), pid, new);
	} else {
		if (new == PS_NONE)
			(void) printf(gettext("process id %d: was %d, "
				"now not bound\n"), pid, old);
		else
			(void) printf(gettext("process id %d: was %d, "
				"now %d\n"), pid, old, new);
	}
}

/*
 * Query the processor set assignments for all CPUs in the system.
 */
static int
print_all(void)
{
	psetid_t	pset;
	uint_t	numcpus;
	int	cpuid;
	int	errors = 0;

	numcpus = (uint_t)sysconf(_SC_NPROCESSORS_CONF);
	for (cpuid = 0; numcpus != 0; cpuid++) {
		if (pset_assign(PS_QUERY, cpuid, &pset) == 0) {
			numcpus--;
			if (pset != PS_NONE)
				print_out(cpuid, pset);
		} else if (errno != EINVAL) {
			errmsg(errno, gettext("cannot query processor %d"),
			    cpuid);
			errors = 1;
		}
	}
	return (errors);
}

/*
 * Query the binding for all processes in the system.
 */
static int
query_all(void)
{
	DIR	*procdir;
	struct dirent	*dp;
	pid_t	pid;
	psetid_t binding;
	char	*errptr;
	int	errors = 0;

	procdir = opendir(PROCDIR);

	if (procdir == NULL) {
		errmsg(errno, gettext("cannot open %s"), PROCDIR);
		return (2);
	}

	while ((dp = readdir(procdir)) != NULL) {

		/*
		 * skip . and .. (and anything else like that)
		 */
		if (dp->d_name[0] == '.')
			continue;
		pid = (pid_t)strtol(dp->d_name, &errptr, 10);
		if (errptr != NULL && *errptr != '\0') {
			errmsg(0, gettext("invalid process ID (pid) %s in %s"),
			    dp->d_name, PROCDIR);
			errors = 2;
			continue;
		}
		if (pset_bind(PS_QUERY, P_PID, pid, &binding) < 0) {
			/*
			 * Ignore search errors.  The process may have exited
			 * since we read the directory.
			 */
			if (errno == ESRCH)
				continue;
			errmsg(errno, gettext("cannot query process %ld"),
			    pid);
			errors = 1;
			continue;
		}
		if (binding != PS_NONE)
			query_out(pid, binding);
	}
	return (errors);
}

void
exec_cmd(psetid_t pset, char **argv)
{
	if (pset_bind(pset, P_PID, P_MYID, NULL) != 0) {
		errmsg(errno, gettext("cannot exec in processor set %d"),
		    pset);
		return;
	}

	(void) execvp(argv[0], argv);
	errmsg(errno, gettext("cannot exec command %s"), argv[0]);
}

/*PRINTFLIKE2*/
static void
errmsg(int err, char *msg, ...)
{
	char buf[LINE_MAX];
	va_list adx;

	va_start(adx);
	(void) vsnprintf(buf, LINE_MAX, msg, adx);
	va_end(adx);
	buf[LINE_MAX - 1] = '\0';
	(void) fprintf(stderr, "%s: %s", cmdname, buf);
	if (err)
		(void) fprintf(stderr, ": %s", strerror(err));
	(void) putc('\n', stderr);
}

void
usage(void)
{
	(void) fprintf(stderr, gettext(
		"usage: \n"
		"\t%s -c [processor_id ...]\n"
		"\t%s -d processor_set_id\n"
		"\t%s -n processor_set_id\n"
		"\t%s -f processor_set_id\n"
		"\t%s -e processor_set_id command [argument(s)...]\n"
		"\t%s -a processor_set_id processor_id ...\n"
		"\t%s -r processor_id ...\n"
		"\t%s -p [processorid ...]\n"
		"\t%s -b processor_set_id pid ...\n"
		"\t%s -u pid ...\n"
		"\t%s -q [pid ...]\n"
		"\t%s [-i] [processor_set_id ...]\n"),
		cmdname, cmdname, cmdname, cmdname, cmdname, cmdname,
		cmdname, cmdname, cmdname, cmdname, cmdname, cmdname);
}

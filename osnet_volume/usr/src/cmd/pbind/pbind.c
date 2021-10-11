#ident	"@(#)pbind.c	1.4	92/09/25 SMI"

/*
 * pbind - bind a process to a processor (non-exclusively)
 */

#include <sys/types.h>
#include <sys/procset.h>
#include <sys/processor.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <locale.h>

#if !defined(TEXT_DOMAIN)		/* should be defined by cc -D */
#define	TEXT_DOMAIN 	"SYS_TEST"	/* Use this only if it weren't */
#endif

#define	PROCDIR	"/proc"

static char *cmdname = "pbind";

static void	query_out(id_t pid, processorid_t cpu);
static void	bind_out(id_t pid, processorid_t old, processorid_t new);
static int	query_all(void);
static void	errmsg(char *msg);
static void	usage(void);

int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind, opterr, optopt;
	int	c;
	int	option = 0;
	id_t	pid;
	processorid_t	cpu;
	processorid_t	old_cpu;
	char	*errptr;
	char	bind = 0;
	char	unbind = 0;
	char	query = 0;
	int	errors;
	processor_info_t pi;

	cmdname = argv[0];	/* put actual command name in messages */

	(void) setlocale(LC_ALL, "");	/* setup localization */
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "bqu")) != EOF) {
		switch (c) {

		case 'b':
			bind = 1;
			break;

		case 'u':
			unbind = 1;
			cpu = PBIND_NONE;
			break;

		case 'q':
			query = 1;
			cpu = PBIND_QUERY;
			break;

		default:
			usage();
			return (2);
		}
	}


	/*
	 * Make sure that at most one of the options b, u, or q
	 * was specified.
	 */
	c = bind + unbind + query;
	if (c < 1) {				/* nothing specified */
		query = 1;			/* default to query */
		cpu = PBIND_QUERY;
	} else if (c > 1) {
		errmsg("options -b, -u, and -q are mutually exclusive.");
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
	 * Get processor for binding.
	 */
	if (bind) {
		if (argc < 2) {		/* must specify processor and one pid */
			errmsg("must specify processor and pid");
			usage();
			return (2);
		}
		cpu = strtol(*argv, &errptr, 10);
		if (errptr != NULL && *errptr != '\0' || cpu < 0) {
			fprintf(stderr, "%s: %s %s\n", cmdname,
				gettext("invalid processor ID"), *argv);
			return (1);
		}
		argv++;
		argc--;
	}

	/*
	 * Perform function for each pid specified.
	 */
	if (argc == 0) {
		errmsg("must specify at least one pid");
		usage();
		return (2);
	}

	/*
	 * Go through listed processors.
	 */
	for (; argc > 0; argv++, argc--) {
		pid = (id_t) strtol(*argv, &errptr, 10);
		if (errptr != NULL && *errptr != '\0') {
			fprintf(stderr, "%s: %s %s\n", cmdname,
				gettext("invalid process ID (pid)"), *argv);
			errors = 2;
			continue;
		}
		if (processor_bind(P_PID, pid, cpu, &old_cpu) < 0) {
			char	buf[256];
			char	*msg;

			strcpy(buf, cmdname);
			strcat(buf, ": ");

			switch (cpu) {
			case PBIND_NONE:
				msg = "%s: cannot unbind pid %d";
				break;
			case PBIND_QUERY:
				msg = "%s: cannot query pid %d";
				break;
			default:
				msg = "%s: cannot bind pid %d";
				break;
			}
			sprintf(buf, gettext(msg), cmdname, pid);
			perror(buf);
			errors = 1;
			continue;
		}
		if (query)
			query_out(pid, old_cpu);
		else
			bind_out(pid, old_cpu, cpu);
	}
	return (errors);
}

/*
 * Output for query.
 */
static void
query_out(id_t pid, processorid_t cpu)
{
	if (cpu == PBIND_NONE)
		printf(gettext("process id %d: not bound\n"), pid);
	else
		printf(gettext("process id %d: %d\n"), pid, cpu);
}

/*
 * Output for bind.
 */
static void
bind_out(id_t pid, processorid_t old, processorid_t new)
{
	if (old == PBIND_NONE)
		if (new == PBIND_NONE)
			printf(gettext("process id %d: was not bound, "
				"now not bound\n"), pid);
		else
			printf(gettext("process id %d: was not bound, "
				"now %d\n"), pid, new);
	else
		if (new == PBIND_NONE)
			printf(gettext("process id %d: was %d, "
				"now not bound\n"), pid, old);
		else
			printf(gettext("process id %d: was %d, "
				"now %d\n"), pid, old, new);
}


/*
 * Query the binding for all processes in the system.
 */
static int
query_all(void)
{
	DIR	*procdir;
	struct dirent	*dp;
	char	buf[256];
	id_t	pid;
	processorid_t binding;
	char	*errptr;
	int	errors = 0;

	procdir = opendir(PROCDIR);

	if (procdir == NULL) {
		sprintf(buf, "%s: %s " PROCDIR "\n", cmdname,
			gettext("cannot open"));
		perror(buf);
		return (2);
	}

	while ((dp = readdir(procdir)) != NULL) {

		/*
		 * skip . and .. (and anything else like that)
		 */
		if (dp->d_name[0] == '.')
			continue;
		pid = (id_t) strtol(dp->d_name, &errptr, 10);
		if (errptr != NULL && *errptr != '\0') {
			fprintf(stderr, gettext("%s: invalid process ID "
				"(pid) %s in %s\n"),
				cmdname, dp->d_name, PROCDIR);
			errors = 2;
			continue;
		}
		if (processor_bind(P_PID, pid, PBIND_QUERY, &binding) < 0) {
			/*
			 * Ignore search errors.  The process may have exited
			 * since we read the directory.
			 */
			if (errno == ESRCH)
				continue;
			sprintf(buf, gettext("%s: cannot query process %d\n"),
				cmdname, pid);
			perror(buf);
			errors = 1;
			continue;
		}
		if (binding != PBIND_NONE)
			query_out(pid, binding);
	}
	return (errors);
}

static void
errmsg(char *msg)
{
	fprintf(stderr, "%s: %s\n", cmdname, gettext(msg));
}

void
usage(void)
{
	fprintf(stderr, "usage: \n\t%s -b processor_id pid ...\n"
		"\t%s -u pid ...\n"
		"\t%s [-q] [pid ...]\n", cmdname, cmdname, cmdname);
}

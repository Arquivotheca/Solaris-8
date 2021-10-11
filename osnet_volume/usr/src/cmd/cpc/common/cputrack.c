/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cputrack.c	1.1	99/08/15 SMI"

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <libintl.h>
#include <locale.h>
#include <errno.h>

#include <libcpc.h>

#include "cpucmds.h"

static struct options {
	int debug;
	int verbose;
	int dotitle;
	int dohelp;
	int dotick;
	int cpuver;
	char *pgmname;
	uint_t mseconds;
	uint_t nsamples;
	uint_t nevents;
	cpc_eventset_t *master;
	int followfork;
	int followexec;
	pid_t pid;
	FILE *log;
} __options;

static const struct options *opts = (const struct options *)&__options;

static void
cputrack_errfn(const char *fn, const char *fmt, va_list ap)
{
	(void) fprintf(stderr, "%s: ", opts->pgmname);
	if (opts->debug)
		(void) fprintf(stderr, "%s: ", fn);
	(void) vfprintf(stderr, fmt, ap);
}

static int cputrack(int argc, char *argv[], int optind);

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

int
main(int argc, char *argv[])
{
	struct options *opts = &__options;
	int c, errcnt = 0;
	int nsamples;
	cpc_eventset_t *eset;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	if ((opts->pgmname = strrchr(argv[0], '/')) == NULL)
		opts->pgmname = argv[0];
	else
		opts->pgmname++;

	if (cpc_version(CPC_VER_CURRENT) != CPC_VER_CURRENT) {
		(void) fprintf(stderr, gettext(
		    "%s: library version mismatch: needed %d found %d\n"),
		    opts->pgmname, CPC_VER_CURRENT, cpc_version(CPC_VER_NONE));
		return (1);
	}

	(void) cpc_seterrfn(cputrack_errfn);
	if ((opts->cpuver = cpc_getcpuver()) == -1) {
		(void) fprintf(stderr, gettext(
		    "%s: CPU performance instrumentation hardware not"
		    " available on this machine.\n"), opts->pgmname);
		return (1);
	}

	/*
	 * Establish (non-zero) defaults
	 */
	opts->mseconds = 1000;
	opts->dotitle = 1;
	opts->log = stdout;
	opts->master = cpc_eset_new(opts->cpuver);

	while ((c = getopt(argc, argv, "T:N:Defhntvo:r:c:p:")) != EOF)
		switch (c) {
		case 'T':			/* sample time,	seconds */
			opts->mseconds = (uint_t)(atof(optarg) * 1000.0);
			break;
		case 'N':			/* number of samples */
			nsamples = atoi(optarg);
			if (nsamples < 0)
				errcnt++;
			else
				opts->nsamples = (uint_t)nsamples;
			break;
		case 'D':			/* enable debugging */
			opts->debug++;
			break;
		case 'f':			/* follow fork */
			opts->followfork++;
			break;
		case 'e':			/* follow exec */
			opts->followexec++;
			break;
		case 'n':			/* no titles */
			opts->dotitle = 0;
			break;
		case 't':			/* print %tick */
			opts->dotick = 1;
			break;
		case 'v':
			opts->verbose = 1;	/* more chatty */
			break;
		case 'o':
			if (optarg == NULL) {
				errcnt++;
				break;
			}
			if ((opts->log = fopen(optarg, "w")) == NULL) {
				(void) fprintf(stderr, gettext(
				    "%s: cannot open '%s' for writing\n"),
				    opts->pgmname, optarg);
				return (1);
			}
			break;
		case 'c':			/* specify statistics */
			if ((eset = cpc_eset_newevent(opts->master,
			    optarg, &errcnt)) != NULL)
				opts->master = eset;
			break;
		case 'p':			/* grab given pid */
			if ((opts->pid = atoi(optarg)) <= 0)
				errcnt++;
			break;
		case 'h':
			opts->dohelp = 1;
			break;
		case '?':
		default:
			errcnt++;
			break;
		}

	if (opts->nsamples == 0)
		opts->nsamples = UINT_MAX;

	if (errcnt != 0 ||
	    opts->dohelp ||
	    (argc == optind && opts->pid == 0) ||
	    (argc > optind && opts->pid != 0) ||
	    (opts->nevents = cpc_eset_numevents(opts->master)) == 0) {
		(void) fprintf(opts->dohelp ? stdout : stderr, gettext(
		    "Usage:\n\t%s [-T secs] [-N count] [-Defhnv] [-o file]\n"
		    "\t\t-c events [command [args] | -p pid]\n\n"
		    "\t-T secs\t  seconds between samples, default 1\n"
		    "\t-N count  number of samples, default unlimited\n"
		    "\t-D\t  enable debug mode\n"
		    "\t-e\t  follow exec(2), and execve(2)\n"
		    "\t-f\t  follow fork(2), fork1(2), and vfork(2)\n"
		    "\t-h\t  print extended usage information\n"
		    "\t-n\t  suppress titles\n"
		    "\t-t\t  include virtualized %s register\n"
		    "\t-v\t  verbose mode\n"
		    "\t-o file\t  write cpu statistics to this file\n"
		    "\t-c events specify processor events to be monitored\n"
		    "\t-p pid\t  pid of existing process to capture\n\n"
		    "\tUse cpustat(1M) to monitor system-wide statistics.\n"),
		    opts->pgmname, CPC_TICKREG_NAME);
		if (opts->dohelp) {
			(void) putchar('\n');
			(void) capabilities(stdout, opts->cpuver);
			exit(0);
		}
		exit(2);
	}

	cpc_eset_reset(opts->master);
	(void) setvbuf(opts->log, NULL, _IOLBF, 0);
	return (cputrack(argc, argv, optind));
}

static void
print_title(void)
{
	(void) fprintf(opts->log, "%7s ", "time");
	if (opts->followfork)
		(void) fprintf(opts->log, "%6s ", "pid");
	(void) fprintf(opts->log, "%3s %10s ", "lwp", "event");
	if (opts->dotick)
		(void) fprintf(opts->log, "%9s ", CPC_TICKREG_NAME);
	(void) fprintf(opts->log, "%9s %9s\n", "pic0", "pic1");
	(void) fflush(opts->log);
}

static void
print_exec(float now, pid_t pid, char *name)
{
	if (name == NULL)
		name = "(unknown)";

	(void) fprintf(opts->log, "%7.3f ", now);
	if (opts->followfork)
		(void) fprintf(opts->log, "%6d ", (int)pid);
	(void) fprintf(opts->log, "%3d %10s ", 1, "exec");
	if (opts->dotick)
		(void) fprintf(opts->log, "%9s ", "");
	(void) fprintf(opts->log, "%9s %9s # '%s'\n", "", "", name);
	(void) fflush(opts->log);
}

static void
print_fork(float now, pid_t newpid, id_t lwpid, pid_t oldpid)
{
	(void) fprintf(opts->log, "%7.3f ", now);
	if (opts->followfork)
		(void) fprintf(opts->log, "%6d ", (int)oldpid);
	(void) fprintf(opts->log, "%3d %10s ", (int)lwpid, "fork");
	if (opts->dotick)
		(void) fprintf(opts->log, "%9s ", "");
	(void) fprintf(opts->log, "%9s %9s # %d\n", "", "", (int)newpid);
	(void) fflush(opts->log);
}

static void
print_sample(pid_t pid, id_t lwpid,
    char *pevent, cpc_event_t *event, const char *evname)
{
	(void) fprintf(opts->log, "%7.3f ", mstimestamp(event));
	if (opts->followfork)
		(void) fprintf(opts->log, "%6d ", (int)pid);
	(void) fprintf(opts->log, "%3d %10s ", (int)lwpid, pevent);
	if (opts->dotick)
		(void) fprintf(opts->log, "%9" PRId64 " ", CPC_TICKREG(event));
	(void) fprintf(opts->log, "%9" PRId64 " %9" PRId64,
	    event->ce_pic[0], event->ce_pic[1]);
	if (opts->nevents > 1)
		(void) fprintf(opts->log, " # %s\n", evname);
	else
		(void) fputc('\n', opts->log);
}

struct pstate {
	cpc_eventset_t *accum;
	cpc_eventset_t **esets;
	int maxlwpid;
};

static int
Init_lwp(pctx_t *pctx, pid_t pid, id_t lwpid, void *arg)
{
	struct pstate *state = arg;
	cpc_eventset_t *eset;
	cpc_event_t *event;
	char *errstr;

	if (state->maxlwpid < lwpid) {
		state->esets = realloc(state->esets,
		    lwpid * sizeof (state->esets));
		if (state->esets == NULL) {
			(void) fprintf(stderr, gettext(
			    "%6d: init_lwp: out of memory\n"), (int)pid);
			return (-1);
		}
		while (state->maxlwpid < lwpid) {
			state->esets[state->maxlwpid] = NULL;
			state->maxlwpid++;
		}
	}

	if ((eset = state->esets[lwpid-1]) == NULL) {
		if ((eset = cpc_eset_clone(opts->master)) == NULL) {
			(void) fprintf(stderr, gettext(
			    "%6d: init_lwp: out of memory\n"), (int)pid);
			return (-1);
		}
		state->esets[lwpid-1] = eset;
		event = cpc_eset_getevent(eset);
	} else {
		cpc_eset_reset(eset);
		event = cpc_eset_getevent(eset);
		event->ce_pic[0] = event->ce_pic[1] = INT64_C(0);
	}

	if (cpc_pctx_bind_event(pctx, lwpid, event, 0) != 0 ||
	    cpc_pctx_take_sample(pctx, lwpid, event) != 0) {
		errstr = strerror(errno);
		if (errno == EAGAIN)
			(void) cpc_pctx_rele(pctx, lwpid);
		(void) fprintf(stderr, gettext(
		    "%6d: init_lwp: can't bind perf counters "
		    "to lwp%d - %s\n"), (int)pid, (int)lwpid, errstr);
		return (-1);
	}

bound:
	if (opts->verbose)
		print_sample(pid, lwpid, "init_lwp",
		    event, cpc_eset_getname(eset));
	return (0);
}

static int
Fini_lwp(pctx_t *pctx, pid_t pid, id_t lwpid, void *arg)
{
	struct pstate *state = arg;
	cpc_eventset_t *eset = state->esets[lwpid-1];
	cpc_event_t *event;
	char *errstr;

	event = cpc_eset_getevent(eset);
	if (cpc_pctx_take_sample(pctx, lwpid, event) == 0) {
		if (opts->verbose)
			print_sample(pid, lwpid, "fini_lwp",
			    event, cpc_eset_getname(eset));
		cpc_eset_accum(state->accum, eset);
		if (cpc_pctx_rele(pctx, lwpid) == 0)
			return (0);
	}

broken:
	switch (errno) {
	case EAGAIN:
		(void) fprintf(stderr, gettext("%6d: fini_lwp: "
		    "lwp%d: perf counter contents invalidated\n"),
		    (int)pid, (int)lwpid);
		break;
	default:
		errstr = strerror(errno);
		(void) fprintf(stderr, gettext("%6d: fini_lwp: "
		    "lwp%d: can't access perf counters - %s\n"),
		    (int)pid, (int)lwpid, errstr);
		break;
	}
	return (0);
}

/*ARGSUSED*/
static int
Lwp_create(pctx_t *pctx, pid_t pid, id_t lwpid, void *arg)
{
	cpc_eventset_t *eset = opts->master;

	print_sample(pid, lwpid, "lwp_create",
	    cpc_eset_getevent(eset), cpc_eset_getname(eset));

	return (0);
}

/*ARGSUSED*/
static int
Lwp_exit(pctx_t *pctx, pid_t pid, id_t lwpid, void *arg)
{
	struct pstate *state = arg;
	cpc_eventset_t *eset = state->esets[lwpid-1];
	cpc_event_t *start, *event;

	start = event = cpc_eset_getevent(eset);
	do {
		if (event->ce_hrt == 0)
			continue;
		print_sample(pid, lwpid, "lwp_exit",
		    event, cpc_eset_getname(eset));
	} while ((event = cpc_eset_nextevent(eset)) != start);

	return (0);
}

/*ARGSUSED*/
static int
Exec(pctx_t *pctx, pid_t pid, id_t lwpid, char *name, void *arg)
{
	struct pstate *state = arg;
	float now = 0.0;
	cpc_event_t *start, *event;

	/*
	 * Print the accumulated results from the previous program image
	 */
	cpc_eset_reset(state->accum);
	start = event = cpc_eset_getevent(state->accum);
	do {
		if (event->ce_hrt == 0)
			continue;
		print_sample(pid, lwpid, "exec",
		    event, cpc_eset_getname(state->accum));
		if (now > mstimestamp(event))
			now = mstimestamp(event);
	} while ((event = cpc_eset_nextevent(state->accum)) != start);

	print_exec(now, pid, name);

	if (state->accum != NULL) {
		cpc_eset_free(state->accum);
		state->accum = NULL;
	}

	if (opts->followexec) {
		state->accum = cpc_eset_clone(opts->master);
		return (0);
	}
	return (-1);
}

/*ARGSUSED*/
static void
Exit(pctx_t *pctx, pid_t pid, id_t lwpid, int status, void *arg)
{
	struct pstate *state = arg;
	cpc_event_t *start, *event;

	cpc_eset_reset(state->accum);
	start = event = cpc_eset_getevent(state->accum);
	do {
		if (event->ce_hrt == 0)
			continue;
		print_sample(pid, lwpid, "exit",
		    event, cpc_eset_getname(state->accum));
	} while ((event = cpc_eset_nextevent(state->accum)) != start);

	cpc_eset_free(state->accum);
	state->accum = NULL;

	for (lwpid = 1; lwpid < state->maxlwpid; lwpid++)
		if (state->esets[lwpid-1] != NULL) {
			cpc_eset_free(state->esets[lwpid-1]);
			state->esets[lwpid-1] = NULL;
		}
	free(state->esets);
	state->esets = NULL;
}

static int
Tick(pctx_t *pctx, pid_t pid, id_t lwpid, void *arg)
{
	struct pstate *state = arg;
	cpc_eventset_t *eset = state->esets[lwpid-1];
	cpc_event_t *this = cpc_eset_getevent(eset);
	const char *name = cpc_eset_getname(eset);
	cpc_event_t now, delta;
	char *errstr;

	if (cpc_pctx_take_sample(pctx, lwpid, &now) != 0)
		goto broken;
	if (opts->nevents > 1) {
		cpc_event_t *next = cpc_eset_nextevent(eset);
		if (cpc_pctx_bind_event(pctx, lwpid, next, 0) != 0)
			goto broken;
	}

	cpc_event_diff(&delta, &now, this);
	*this = now;

	print_sample(pid, lwpid, "tick", &delta, name);
	return (0);

broken:
	switch (errno) {
	case EAGAIN:
		(void) fprintf(stderr, gettext(
		    "%6d: tick: lwp%d: perf counter contents invalidated\n"),
		    (int)pid, (int)lwpid);
		break;
	default:
		errstr = strerror(errno);
		(void) fprintf(stderr, gettext(
		    "%6d: tick: lwp%d: can't access perf counter - %s\n"),
		    (int)pid, (int)lwpid, errstr);
		break;
	}
	(void) cpc_pctx_rele(pctx, lwpid);
	return (-1);
}

/*
 * The system has just created a new address space that has a new pid.
 * We're running in a child of the controlling process, with a new
 * pctx handle already opened on the child of the original controlled process.
 */
static void
Fork(pctx_t *pctx, pid_t oldpid, pid_t pid, id_t lwpid, void *arg)
{
	struct pstate *state = arg;

	print_fork(mstimestamp(0), pid, lwpid, oldpid);

	if (!opts->followfork)
		return;

	if (pctx_set_events(pctx,
	    PCTX_SYSC_EXEC_EVENT, Exec,
	    PCTX_SYSC_FORK_EVENT, Fork,
	    PCTX_SYSC_EXIT_EVENT, Exit,
	    PCTX_SYSC_LWP_CREATE_EVENT, Lwp_create,
	    PCTX_INIT_LWP_EVENT, Init_lwp,
	    PCTX_FINI_LWP_EVENT, Fini_lwp,
	    PCTX_SYSC_LWP_EXIT_EVENT, Lwp_exit,
	    PCTX_NULL_EVENT) == 0) {
		state->accum = cpc_eset_clone(opts->master);
		(void) pctx_run(pctx, opts->mseconds, opts->nsamples, Tick);
		if (state->accum) {
			free(state->accum);
			state->accum = NULL;
		}
	}
}

/*
 * Translate the incoming options into actions, and get the
 * tool and the process to control running.
 */
static int
cputrack(int argc, char *argv[], int optind)
{
	struct pstate __state, *state = &__state;
	pctx_t *pctx;
	int err;

	bzero(state, sizeof (*state));

	if (cpc_access() == -1)
		return (1);

	if (opts->pid == 0) {
		if (argc <= optind) {
			(void) fprintf(stderr, "%s: %s\n",
			    opts->pgmname,
			    gettext("no program to start"));
			return (1);
		}
		pctx = pctx_create(argv[optind],
		    &argv[optind], state, 1, cputrack_errfn);
		if (pctx == NULL) {
			(void) fprintf(stderr, "%s: %s '%s'\n",
			    opts->pgmname,
			    gettext("failed to start program"),
			    argv[optind]);
			return (1);
		}
	} else {
		pctx = pctx_capture(opts->pid, state, 1, cputrack_errfn);
		if (pctx == NULL) {
			(void) fprintf(stderr, "%s: %s %d\n",
			    opts->pgmname,
			    gettext("failed to capture pid"),
			    (int)opts->pid);
			return (1);
		}
	}

	err = pctx_set_events(pctx,
	    PCTX_SYSC_EXEC_EVENT, Exec,
	    PCTX_SYSC_FORK_EVENT, Fork,
	    PCTX_SYSC_EXIT_EVENT, Exit,
	    PCTX_SYSC_LWP_CREATE_EVENT, Lwp_create,
	    PCTX_INIT_LWP_EVENT, Init_lwp,
	    PCTX_FINI_LWP_EVENT, Fini_lwp,
	    PCTX_SYSC_LWP_EXIT_EVENT, Lwp_exit,
	    0);

	if (err != 0) {
		(void) fprintf(stderr, "%s: %s\n",
		    opts->pgmname,
		    gettext("can't bind process context ops to process"));
	} else {
		if (opts->dotitle)
			print_title();
		state->accum = cpc_eset_clone(opts->master);
		zerotime();
		err = pctx_run(pctx, opts->mseconds, opts->nsamples, Tick);
		if (state->accum) {
			free(state->accum);
			state->accum = NULL;
		}
	}
	pctx_release(pctx);

	return (err != 0 ? 1 : 0);
}

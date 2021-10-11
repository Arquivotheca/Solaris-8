/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpustat.c	1.2	99/11/20 SMI"

#include <sys/types.h>
#include <sys/processor.h>
#include <sys/pset.h>
#include <sys/lwp.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <thread.h>
#include <errno.h>
#include <libintl.h>
#include <locale.h>

#include <libcpc.h>

#include "cpucmds.h"

static struct options {
	int debug;
	int dotitle;
	int dohelp;
	int dotick;
	int cpuver;
	char *pgmname;
	uint_t mseconds;
	uint_t nsamples;
	uint_t nevents;
	cpc_eventset_t *master;
} __options;

static const struct options *opts = (const struct options *)&__options;

static void
cpustat_errfn(const char *fn, const char *fmt, va_list ap)
{
	(void) fprintf(stderr, "%s: ", opts->pgmname);
	if (opts->debug)
		(void) fprintf(stderr, "%s: ", fn);
	(void) vfprintf(stderr, fmt, ap);
}

static int cpustat(void);

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

int
main(int argc, char *argv[])
{
	struct options *opts = &__options;
	int c, errcnt = 0;
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

	(void) cpc_seterrfn(cpustat_errfn);
	if (cpc_access() == -1 ||
	    (opts->cpuver = cpc_getcpuver()) == -1) {
		(void) fprintf(stderr, gettext(
		    "%s: CPU performance counter hardware not"
		    " available on this machine.\n"), opts->pgmname);
		return (1);
	}

	/*
	 * Establish some defaults
	 */
	opts->mseconds = 5000;
	opts->nsamples = UINT_MAX;
	opts->dotitle = 1;
	opts->master = cpc_eset_new(opts->cpuver);

	while ((c = getopt(argc, argv, "Dc:hnt")) != EOF && errcnt == 0)
		switch (c) {
		case 'D':			/* enable debugging */
			opts->debug++;
			break;
		case 'c':			/* specify statistics */
			if ((eset = cpc_eset_newevent(opts->master,
			    optarg, &errcnt)) != NULL)
				opts->master = eset;
			break;
		case 'n':			/* no titles */
			opts->dotitle = 0;
			break;
		case 't':			/* print %tick */
			opts->dotick = 1;
			break;
		case 'h':			/* help */
			opts->dohelp = 1;
			break;
		case '?':
		default:
			errcnt++;
			break;
		}

	switch (argc - optind) {
	case 0:
		break;
	case 2:
		opts->nsamples = (uint_t)atoi(argv[optind + 1]);
		/*FALLTHROUGH*/
	case 1:
		opts->mseconds = (uint_t)(atof(argv[optind]) * 1000.0);
		break;
	default:
		errcnt++;
		break;
	}

	if (opts->nsamples == 0 || opts->mseconds == 0)
		errcnt++;

	if (errcnt != 0 || opts->dohelp ||
	    (opts->nevents = cpc_eset_numevents(opts->master)) == 0) {
		(void) fprintf(opts->dohelp ? stdout : stderr, gettext(
		    "Usage:\n\t%s [-c events] [-nhD] [interval [count]]\n\n"
		    "\t-c events specify processor events to be monitored\n"
		    "\t-n\t  suppress titles\n"
		    "\t-t\t  include %s register\n"
		    "\t-D\t  enable debug mode\n"
		    "\t-h\t  print extended usage information\n\n"
		    "\tUse cputrack(1) to monitor per-process statistics.\n"),
		    opts->pgmname, CPC_TICKREG_NAME);
		if (opts->dohelp) {
			(void) putchar('\n');
			(void) capabilities(stdout, opts->cpuver);
			exit(0);
		}
		exit(2);
	}

	cpc_eset_reset(opts->master);
	(void) setvbuf(stdout, NULL, _IOLBF, 0);
	return (cpustat());
}

static void
print_title(void)
{
	(void) printf("%7s %3s %5s ", "time", "cpu", "event");
	if (opts->dotick)
		(void) printf("%9s ", CPC_TICKREG_NAME);
	(void) printf("%9s %9s\n", "pic0", "pic1");
}

static void
print_sample(processorid_t cpuid, cpc_event_t *event, const char *evname)
{
	char line[1024];
	int ccnt;

	ccnt = snprintf(line, sizeof (line), "%7.3f %3d %5s ",
	    mstimestamp(event), (int)cpuid, "tick");
	if (opts->dotick)
		ccnt += snprintf(line + ccnt, sizeof (line) - ccnt,
		    "%9" PRId64 " ", CPC_TICKREG(event));
	ccnt += snprintf(line + ccnt, sizeof (line) - ccnt,
	    "%9" PRId64 " %9" PRId64, event->ce_pic[0], event->ce_pic[1]);
	if (opts->nevents > 1)
		ccnt += snprintf(line + ccnt, sizeof (line) - ccnt,
		    " # %s\n", evname);
	else
		ccnt += snprintf(line + ccnt, sizeof (line) - ccnt, "\n");

	if (ccnt > sizeof (line))
		ccnt = sizeof (line);
	if (ccnt > 0)
		(void) write(1, line, ccnt);
}

static void
print_total(int ncpus, cpc_event_t *event, const char *evname)
{
	(void) printf("%7.3f %3d %5s ", mstimestamp(event), ncpus, "total");
	if (opts->dotick)
		(void) printf("%9" PRId64 " ", CPC_TICKREG(event));
	(void) printf("%9" PRId64 " %9" PRId64,
	    event->ce_pic[0], event->ce_pic[1]);
	if (opts->nevents > 1)
		(void) printf(" # %s", evname);
	(void) fputc('\n', stdout);
}

#define	NSECS_PER_MSEC	1000000ll
#define	NSECS_PER_SEC	1000000000ll

struct tstate {
	processorid_t cpuid;
	cpc_eventset_t *eset;
	int status;
};

static void *
gtick(void *arg)
{
	struct tstate *state = arg;
	uint_t nsamples;
	int fd;
	hrtime_t ht, htdelta;
	cpc_eventset_t *eset = state->eset;
	cpc_event_t *this = cpc_eset_getevent(eset);
	const char *name = cpc_eset_getname(eset);
	char *errstr;

	if (processor_bind(P_LWPID, P_MYID, state->cpuid, NULL) == -1) {
		state->status = 1;
		errstr = strerror(errno);
		(void) fprintf(stderr, gettext(
		    "%s: cannot bind lwp to cpu%d - %s\n"),
		    opts->pgmname, (int)state->cpuid, errstr);
		return (NULL);
	}

	if ((fd = cpc_shared_open()) == -1) {
		state->status = 2;
		errstr = strerror(errno);
		(void) fprintf(stderr, gettext(
		    "%s: cannot access performance counters on cpu%d - %s\n"),
		    opts->pgmname, (int)state->cpuid, errstr);
		return (NULL);
	}

	htdelta = NSECS_PER_MSEC * opts->mseconds;
	ht = gethrtime();

	if (cpc_shared_bind_event(fd, this, 0) == -1)
		goto bad;

	for (nsamples = opts->nsamples; nsamples; nsamples--) {
		cpc_event_t now, delta;
		hrtime_t htnow;
		struct timespec ts;

		ht += htdelta;
		htnow = gethrtime();
		if (ht <= htnow)
			continue;
		ts.tv_sec = (time_t)((ht - htnow) / NSECS_PER_SEC);
		ts.tv_nsec = (suseconds_t)((ht - htnow) % NSECS_PER_SEC);

		(void) nanosleep(&ts, NULL);

		if (opts->nevents == 1) {
			if (cpc_shared_take_sample(fd, &now) != 0)
				goto bad;
			cpc_event_diff(&delta, &now, this);
			*this = now;
		} else {
			cpc_event_t *next;

			name = cpc_eset_getname(eset);
			next = cpc_eset_nextevent(eset);

			if (cpc_shared_take_sample(fd, &now) != 0)
				goto bad;
			if (cpc_shared_bind_event(fd, next, 0) != 0)
				goto bad;

			cpc_event_diff(&delta, &now, this);
			*this = now;
			this = next;
		}

		print_sample(state->cpuid, &delta, name);
	}
	cpc_shared_close(fd);
	return (NULL);
bad:
	state->status = 3;
	errstr = strerror(errno);
	(void) fprintf(stderr, gettext("%s: cpu%d - %s\n"),
	    opts->pgmname, state->cpuid, errstr);
	cpc_shared_close(fd);
	return (NULL);
}

static int
cpustat(void)
{
	struct tstate *gstate;
	cpc_eventset_t *accum;
	cpc_event_t *event, *start;
	int fd, c, i, ncpus, retval;
	int lwps = 0;
	psetid_t mypset, cpupset;
	char *errstr;

	/*
	 * You need to be root to access the shared counters (thus
	 * damaging all "regular" counter context in the system)
	 */
	if ((fd = cpc_shared_open()) != -1)
		cpc_shared_close(fd);
	else if (errno != EINVAL) {
		/*
		 * (EINVAL corresponds to an otherwise successful open
		 * but with an unbound lwp)
		 */
		errstr = strerror(errno);
		(void) fprintf(stderr, gettext(
		    "%s: cannot access cpu performance counters - %s\n"),
		    opts->pgmname, errstr);
		return (1);
	}

	ncpus = (int)sysconf(_SC_NPROCESSORS_CONF);
	if ((gstate = calloc(ncpus, sizeof (*gstate))) == NULL) {
		(void) fprintf(stderr, gettext(
		    "%s: out of heap\n"), opts->pgmname);
		return (1);
	}

	/*
	 * Only include processors that are participating in the system
	 */
	for (c = 0, i = 0; i < ncpus; c++)
		switch (p_online(c, P_STATUS)) {
		case P_ONLINE:
		case P_NOINTR:
			gstate[i++].cpuid = c;
			break;
		case P_OFFLINE:
		case P_POWEROFF:
			gstate[i++].cpuid = -1;
			break;
		case -1:
		default:
			break;
		}

	/*
	 * Examine the processor sets; if we're in one, only attempt
	 * to report on the set we're in.
	 */
	if (pset_bind(PS_QUERY, P_PID, P_MYID, &mypset) == -1) {
		errstr = strerror(errno);
		(void) fprintf(stderr, gettext("%s: pset_bind - %s\n"),
		    opts->pgmname, errstr);
	} else {
		for (i = 0; i < ncpus; i++) {
			struct tstate *this = &gstate[i];

			if (this->cpuid == -1)
				continue;

			if (pset_assign(PS_QUERY,
			    this->cpuid, &cpupset) == -1) {
				errstr = strerror(errno);
				(void) fprintf(stderr,
				    gettext("%s: pset_assign - %s\n"),
				    opts->pgmname, errstr);
				continue;
			}

			if (mypset != cpupset)
				this->cpuid = -1;
		}
	}

	if (opts->dotitle)
		print_title();
	zerotime();

	for (i = 0; i < ncpus; i++) {
		struct tstate *this = &gstate[i];
		thread_t tid;

		if (this->cpuid == -1)
			continue;
		this->eset = cpc_eset_clone(opts->master);
		if (this->eset == NULL) {
			this->cpuid = -1;
			continue;
		}
		if (thr_create(NULL, 0, gtick, this,
		    THR_BOUND|THR_NEW_LWP, &tid) == 0)
			lwps++;
		else {
			(void) fprintf(stderr,
			    gettext("%s: cannot create thread for cpu%d\n"),
			    opts->pgmname, this->cpuid);
			this->status = 4;
		}
	}

	if (lwps != 0)
		while (thr_join(NULL, NULL, NULL) == 0)
			;

	if ((accum = cpc_eset_clone(opts->master)) == NULL) {
		(void) fprintf(stderr, gettext("%s: out of heap\n"),
		    opts->pgmname);
		return (1);
	}

	retval = 0;
	for (i = 0; i < ncpus; i++) {
		struct tstate *this = &gstate[i];

		if (this->cpuid == -1)
			continue;
		cpc_eset_accum(accum, this->eset);
		cpc_eset_free(this->eset);
		this->eset = NULL;
		if (this->status != 0)
			retval = 1;
	}

	cpc_eset_reset(accum);
	start = event = cpc_eset_getevent(accum);
	do {
		if (event->ce_hrt == 0)
			continue;
		print_total(lwps, event, cpc_eset_getname(accum));
	} while ((event = cpc_eset_nextevent(accum)) != start);

	cpc_eset_free(accum);
	accum = NULL;

	free(gstate);
	return (retval);
}

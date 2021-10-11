/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mpstat.c	1.5	99/01/08 SMI"

#include <sys/pset.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/sysinfo.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <kstat.h>
#include <poll.h>

#define	MAX(a, b) ((a > b) ? (a) : (b))

static	kstat_ctl_t	*kc;		/* libkstat cookie */
static	int		ncpus = -1;
static	int		max_instance = -1;

typedef struct cpuinfo {
	kstat_t		*cs_kstat;
	cpu_stat_t	cs_old;
	cpu_stat_t	cs_new;
	psetid_t	cs_pset;
} cpuinfo_t;

static	cpuinfo_t	*cpulist = NULL;
static	psetid_t	*setlist = NULL;

#define	DELTA(i, x)	(cpulist[i].cs_new.x - cpulist[i].cs_old.x)

static	const char	cmdname[] = "mpstat";
static	const char	cmd_options[] = "pP:";

static	int	hz, iter = 0, interval = 0, poll_interval = 0;
static	int	lines_until_reprint = 0;

#define	REPRINT	20

static	void	print_header(int);
static	void	show_cpu_usage(int);
static	void	usage(void);
static	int	cpu_stat_init(int);
static	int	cpu_stat_load(void);
static	void	fail(int, char *, ...);
static	void	safe_zalloc(void **, int, int);

int
main(int argc, char **argv)
{
	int c;
	int display_psets = 0;

	while ((c = getopt(argc, argv, cmd_options)) != (int)EOF)
		switch (c) {
			case 'p':
				/*
				 * Display all processor sets.
				 */
				display_psets = -1;
				break;
			case 'P':
				/*
				 * Display specific processor set.
				 */
				display_psets = atoi(optarg);
				break;
			case '?':
				usage();
		}

	if ((kc = kstat_open()) == NULL)
		fail(1, "kstat_open(): can't open /dev/kstat");
	(void) cpu_stat_init(display_psets);
	hz = sysconf(_SC_CLK_TCK);

	if (argc > optind) {
		interval = atoi(argv[optind]);
		poll_interval = 1000 * interval;
		if (interval <= 0)
			usage();
		iter = (1 << 30);
		if (argc > optind + 1) {
			iter = atoi(argv[optind + 1]);
			if (iter <= 0)
				usage();
		}
	}

	show_cpu_usage(display_psets);
	while (--iter > 0) {
		(void) poll(NULL, 0, poll_interval);
		show_cpu_usage(display_psets);
	}
	return (0);
}

static void
print_header(int pset)
{
	(void) printf("CPU minf mjf xcal  intr ithr  csw icsw migr "
	    "smtx  srw syscl  usr sys  wt idl");

	if (pset < 0)
		(void) printf(" set");

	(void) printf("\n");
}

static void
show_cpu_usage(int pset)
{
	int i, c, ticks;
	double etime, percent;

	while (kstat_chain_update(kc) || cpu_stat_load()) {
		if (cpu_stat_init(pset))
			(void) printf("<<State change>>\n");
	}

	if (lines_until_reprint == 0 || ncpus > 1) {
		print_header(pset);
		lines_until_reprint = REPRINT;
	}
	lines_until_reprint--;

	for (c = 0; c < ncpus; c++) {
		if (pset > 0 && cpulist[c].cs_pset != (psetid_t)pset)
			continue;

		ticks = 0;
		for (i = 0; i < CPU_STATES; i++)
			ticks += DELTA(c, cpu_sysinfo.cpu[i]);
		etime = (double)ticks / hz;
		if (etime == 0.0)
			etime = 1.0;
		percent = 100.0 / etime / hz;
		(void) printf("%3d %4d %3d %4d %5d %4d %4d %4d %4d %4d %4d"
		    " %5d  %3.0f %3.0f %3.0f %3.0f",
		    cpulist[c].cs_kstat->ks_instance,
		    (int)((DELTA(c, cpu_vminfo.hat_fault) +
		    DELTA(c, cpu_vminfo.as_fault)) / etime),
		    (int)(DELTA(c, cpu_vminfo.maj_fault) / etime),
		    (int)(DELTA(c, cpu_sysinfo.xcalls) / etime),
		    (int)(DELTA(c, cpu_sysinfo.intr) / etime),
		    (int)(DELTA(c, cpu_sysinfo.intrthread) / etime),
		    (int)(DELTA(c, cpu_sysinfo.pswitch) /etime),
		    (int)(DELTA(c, cpu_sysinfo.inv_swtch) /etime),
		    (int)(DELTA(c, cpu_sysinfo.cpumigrate) /etime),
		    (int)(DELTA(c, cpu_sysinfo.mutex_adenters) /etime),
		    (int)((DELTA(c, cpu_sysinfo.rw_rdfails) +
		    DELTA(c, cpu_sysinfo.rw_wrfails)) / etime),
		    (int)(DELTA(c, cpu_sysinfo.syscall) / etime),
		    DELTA(c, cpu_sysinfo.cpu[CPU_USER]) * percent,
		    DELTA(c, cpu_sysinfo.cpu[CPU_KERNEL]) * percent,
		    DELTA(c, cpu_sysinfo.cpu[CPU_WAIT]) * percent,
		    DELTA(c, cpu_sysinfo.cpu[CPU_IDLE]) * percent);

		if (pset < 0)
			(void) printf(" %3d", cpulist[c].cs_pset);

		(void) printf("\n");
	}
	(void) fflush(stdout);
}

/*
 * Get the KIDs for subsequent cpu_stat_load operations.  Returns 1 or 0
 * depending on whether or not the initialization was actually required.
 */
static int
cpu_stat_init(int show_pset)
{
	kstat_t *ksp;
	int i;
	int old_ncpus = ncpus;
	int old_max_instance = max_instance;
	int changed_psets = 0;

	ncpus = 0;
	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next)
		if (strcmp(ksp->ks_module, "cpu_stat") == 0) {
			int pset;

			ncpus++;
			max_instance = MAX(max_instance, ksp->ks_instance);

			if (pset_assign(PS_QUERY, ksp->ks_instance,
			    (psetid_t *)&pset) == -1)
				fail(1, "processor set assignment failed");
			if (pset == -1)
				/*
				 * Place stubborn or unassigned CPUs in
				 * fictional set 0.
				 */
				pset = 0;

			if (ksp->ks_instance > old_max_instance ||
			    setlist[ksp->ks_instance] != pset)
				changed_psets++;
		}

	if (old_ncpus == ncpus && changed_psets == 0)
		/*
		 * We haven't gained or lost a CPU, or had a set reassignment.
		 */
		return (0);

	safe_zalloc((void **)&cpulist, ncpus * sizeof (cpuinfo_t), 1);
	safe_zalloc((void **)&setlist, max_instance * sizeof (int), 1);

	ncpus = 0;
	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		int pset = 0;

		if (strcmp(ksp->ks_module, "cpu_stat") != 0)
			continue;

		if (pset_assign(PS_QUERY, ksp->ks_instance,
		    (psetid_t *)&pset) == -1)
			fail(1, "processor set assignment failed");
		if (pset == -1)
			pset = 0;

		/*
		 * Insertion sort by processor set (if -p option), CPU id.
		 */
		for (i = ncpus - 1; i >= 0; i--) {
			if ((show_pset >= 0 &&
			    cpulist[i].cs_kstat->ks_instance <
			    ksp->ks_instance) ||
			    (show_pset == -1 &&
			    (cpulist[i].cs_pset < pset ||
			    cpulist[i].cs_pset == pset &&
			    cpulist[i].cs_kstat->ks_instance <
			    ksp->ks_instance)))
				break;
			cpulist[i + 1].cs_kstat = cpulist[i].cs_kstat;
			cpulist[i + 1].cs_pset = cpulist[i].cs_pset;
		}
		cpulist[i + 1].cs_kstat = ksp;
		cpulist[i + 1].cs_pset = pset;

		ncpus++;
	}

	if (ncpus == 0)
		fail(0, "can't find any cpu statistics");

	for (i = 0; i < ncpus; i++)
		setlist[cpulist[i].cs_kstat->ks_instance] = cpulist[i].cs_pset;

	return (1);
}

/*
 * Load per-CPU statistics
 */
static int
cpu_stat_load(void)
{
	int i;

	for (i = 0; i < ncpus; i++) {
		int pset;

		cpulist[i].cs_old = cpulist[i].cs_new;
		if (kstat_read(kc, cpulist[i].cs_kstat,
		    (void *) &cpulist[i].cs_new) == -1)
			return (1);
		if (pset_assign(PS_QUERY, cpulist[i].cs_kstat->ks_instance,
		    (psetid_t *)&pset) == -1)
			fail(1, "processor set assignment failed");
		if ((pset == -1 &&
		    setlist[cpulist[i].cs_kstat->ks_instance] != 0) ||
		    (pset != -1 &&
		    setlist[cpulist[i].cs_kstat->ks_instance] != pset))
			return (1);
	}
	return (0);
}

static void
usage(void)
{
	(void) fprintf(stderr,
	    "Usage: %s [-p | -P processor_set] [interval [count]]\n",
	    cmdname);
	exit(1);
}

static void
fail(int do_perror, char *message, ...)
{
	va_list args;

	va_start(args, message);
	(void) fprintf(stderr, "%s: ", cmdname);
	(void) vfprintf(stderr, message, args);
	va_end(args);
	if (do_perror)
		(void) fprintf(stderr, ": %s", strerror(errno));
	(void) fprintf(stderr, "\n");
	exit(2);
}

static void
safe_zalloc(void **ptr, int size, int free_first)
{
	if (free_first && *ptr != NULL)
		free(*ptr);
	if ((*ptr = (void *) malloc(size)) == NULL)
		fail(1, "malloc failed");
	(void) memset(*ptr, 0, size);
}

/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lockstat.c	1.6	99/07/27 SMI"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/lockstat.h>

static void report_stats(FILE *, lsrec_t **, size_t, uint64_t, uint64_t);
static void report_trace(FILE *, lsrec_t **);

extern int symtab_init(void);
extern char *addr_to_sym(uintptr_t, uintptr_t *, size_t *);
extern uintptr_t sym_to_addr(char *name);
extern size_t sym_size(char *name);
extern char *strtok_r(char *, const char *, char **);

#define	DEFAULT_NRECS	10000
#define	DEFAULT_HZ	97
#define	MAX_HZ		1000

static uint64_t ev_count[LS_MAX_EVENTS + 1];
static uint64_t ev_time[LS_MAX_EVENTS + 1];
static lsctl_t lsctl;
static int stk_depth;
static int watched_locks = 0;
static int watched_funcs = 0;
static int nrecs_used;
static int top_n = INT_MAX;
static hrtime_t elapsed_time;
static int do_rates = 0;
static int pflag = 0;
static int Pflag = 0;
static int wflag = 0;
static int Wflag = 0;
static int cflag = 0;
static int kflag = 0;
static int gflag = 0;

typedef struct ls_event_info {
	char	ev_type;
	char	ev_lhdr[20];
	char	ev_desc[80];
	char	ev_units[10];
} ls_event_info_t;

static ls_event_info_t ls_event_info[LS_MAX_EVENTS] = {
	{ 'C',	"Lock",	"Adaptive mutex spin",			"spin"	},
	{ 'C',	"Lock",	"Adaptive mutex block",			"nsec"	},
	{ 'C',	"Lock",	"Spin lock spin",			"spin"	},
	{ 'C',	"Lock",	"Thread lock spin",			"spin"	},
	{ 'C',	"Lock",	"R/W writer blocked by writer",		"nsec"	},
	{ 'C',	"Lock",	"R/W writer blocked by readers",	"nsec"	},
	{ 'C',	"Lock",	"R/W reader blocked by writer",		"nsec"	},
	{ 'C',	"Lock",	"R/W reader blocked by write wanted",	"nsec"	},
	{ 'C',	"Lock",	"Unknown event (type 8)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 9)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 10)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 11)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 12)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 13)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 14)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 15)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 16)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 17)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 18)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 19)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 20)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 21)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 22)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 23)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 24)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 25)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 26)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 27)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 28)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 29)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 30)",		"units"	},
	{ 'C',	"Lock",	"Unknown event (type 31)",		"units"	},
	{ 'H',	"Lock",	"Adaptive mutex hold",			"nsec"	},
	{ 'H',	"Lock",	"Spin lock hold",			"nsec"	},
	{ 'H',	"Lock",	"R/W writer hold",			"nsec"	},
	{ 'H',	"Lock",	"R/W reader hold",			"nsec"	},
	{ 'H',	"Lock",	"Unknown event (type 36)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 37)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 38)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 39)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 40)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 41)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 42)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 43)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 44)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 45)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 46)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 47)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 48)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 49)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 50)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 51)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 52)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 53)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 54)",		"units"	},
	{ 'H',	"Lock",	"Unknown event (type 55)",		"units"	},
	{ 'I',	"CPU+PIL", "Profiling interrupt",		"nsec"	},
	{ 'I',	"Lock",	"Unknown event (type 57)",		"units"	},
	{ 'I',	"Lock",	"Unknown event (type 58)",		"units"	},
	{ 'I',	"Lock",	"Unknown event (type 59)",		"units"	},
	{ 'E',	"Lock",	"Recursive lock entry detected",	"(N/A)"	},
	{ 'E',	"Lock",	"Lockstat enter failure",		"(N/A)"	},
	{ 'E',	"Lock",	"Lockstat exit failure",		"nsec"	},
	{ 'E',	"Lock",	"Lockstat record failure",		"(N/A)"	},
};

static void
fail(int do_perror, char *message, ...)
{
	va_list args;
	int save_errno = errno;

	va_start(args, message);
	(void) fprintf(stderr, "lockstat: ");
	(void) vfprintf(stderr, message, args);
	va_end(args);
	if (do_perror)
		(void) fprintf(stderr, ": %s", strerror(save_errno));
	(void) fprintf(stderr, "\n");
	exit(2);
}

static void
show_events(char event_type, char *desc)
{
	int i, first = -1, last;

	for (i = 0; i < LS_MAX_EVENTS; i++) {
		ls_event_info_t *evp = &ls_event_info[i];
		if (evp->ev_type != event_type ||
		    strncmp(evp->ev_desc, "Unknown event", 13) == 0)
			continue;
		if (first == -1)
			first = i;
		last = i;
	}

	(void) fprintf(stderr,
	    "\n%s events (lockstat -%c or lockstat -e %d-%d):\n\n",
	    desc, event_type, first, last);

	for (i = first; i <= last; i++)
		(void) fprintf(stderr,
		    "%4d = %s\n", i, ls_event_info[i].ev_desc);
}

static void
usage(void)
{
	(void) fprintf(stderr,
	    "Usage: lockstat [options] command [args]\n"
	    "\nEvent selection options:\n\n"
	    "  -C              watch contention events [on by default]\n"
	    "  -E              watch error events [on by default]\n"
	    "  -H              watch hold events [off by default]\n"
	    "  -I              watch interrupt events [off by default]\n"
	    "  -A              watch all lock events [equivalent to -CEH]\n"
	    "  -e event_list   only watch the specified events (shown below);\n"
	    "                  <event_list> is a comma-separated list of\n"
	    "                  events or ranges of events, e.g. 1,4-7,35\n"
	    "  -i rate         interrupt rate for -I [default: %d Hz]\n"
	    "\nData gathering options:\n\n"
	    "  -b              basic statistics (lock, caller, event count)\n"
	    "  -t              timing for all events [default]\n"
	    "  -h              histograms for event times\n"
	    "  -s depth        stack traces <depth> deep\n"
	    "\nData filtering options:\n\n"
	    "  -n nrecords     maximum number of data records [default: %d]\n"
	    "  -l lock[,size]  only watch <lock>, which can be specified as a\n"
	    "                  symbolic name or hex address; <size> defaults\n"
	    "                  to the ELF symbol size if available, 1 if not\n"
	    "  -f func[,size]  only watch events generated by <func>\n"
	    "  -d duration     only watch events longer than <duration>\n"
	    "  -T              trace (rather than sample) events\n"
	    "\nData reporting options:\n\n"
	    "  -c              coalesce lock data for arrays like pse_mutex[]\n"
	    "  -k              coalesce PCs within functions\n"
	    "  -g              show total events generated by function\n"
	    "  -w              wherever: don't distinguish events by caller\n"
	    "  -W              whichever: don't distinguish events by lock\n"
	    "  -R              display rates rather than counts\n"
	    "  -p              parsable output format (awk(1)-friendly)\n"
	    "  -P              sort lock data by (count * avg_time) product\n"
	    "  -D n            only display top <n> events of each type\n"
	    "  -o filename     send output to <filename>\n",
	    DEFAULT_HZ, DEFAULT_NRECS);

	show_events('C', "Contention");
	show_events('H', "Hold-time");
	show_events('I', "Interrupt");
	show_events('E', "Error");
	(void) fprintf(stderr, "\n");

	exit(1);
}

static int
lockcmp(lsrec_t *a, lsrec_t *b)
{
	int i;

	if (a->ls_event < b->ls_event)
		return (-1);
	if (a->ls_event > b->ls_event)
		return (1);

	for (i = stk_depth - 1; i >= 0; i--) {
		if (a->ls_stack[i] < b->ls_stack[i])
			return (-1);
		if (a->ls_stack[i] > b->ls_stack[i])
			return (1);
	}

	if (a->ls_caller < b->ls_caller)
		return (-1);
	if (a->ls_caller > b->ls_caller)
		return (1);

	if (a->ls_lock < b->ls_lock)
		return (-1);
	if (a->ls_lock > b->ls_lock)
		return (1);

	return (0);
}

static int
countcmp(lsrec_t *a, lsrec_t *b)
{
	if (a->ls_event < b->ls_event)
		return (-1);
	if (a->ls_event > b->ls_event)
		return (1);

	return (b->ls_count - a->ls_count);
}

static int
timecmp(lsrec_t *a, lsrec_t *b)
{
	if (a->ls_event < b->ls_event)
		return (-1);
	if (a->ls_event > b->ls_event)
		return (1);

	if (a->ls_time < b->ls_time)
		return (1);
	if (a->ls_time > b->ls_time)
		return (-1);

	return (0);
}

static int
lockcmp_anywhere(lsrec_t *a, lsrec_t *b)
{
	if (a->ls_event < b->ls_event)
		return (-1);
	if (a->ls_event > b->ls_event)
		return (1);

	if (a->ls_lock < b->ls_lock)
		return (-1);
	if (a->ls_lock > b->ls_lock)
		return (1);

	return (0);
}

static int
lock_and_count_cmp_anywhere(lsrec_t *a, lsrec_t *b)
{
	if (a->ls_event < b->ls_event)
		return (-1);
	if (a->ls_event > b->ls_event)
		return (1);

	if (a->ls_lock < b->ls_lock)
		return (-1);
	if (a->ls_lock > b->ls_lock)
		return (1);

	return (b->ls_count - a->ls_count);
}

static int
sitecmp_anylock(lsrec_t *a, lsrec_t *b)
{
	int i;

	if (a->ls_event < b->ls_event)
		return (-1);
	if (a->ls_event > b->ls_event)
		return (1);

	for (i = stk_depth - 1; i >= 0; i--) {
		if (a->ls_stack[i] < b->ls_stack[i])
			return (-1);
		if (a->ls_stack[i] > b->ls_stack[i])
			return (1);
	}

	if (a->ls_caller < b->ls_caller)
		return (-1);
	if (a->ls_caller > b->ls_caller)
		return (1);

	return (0);
}

static int
site_and_count_cmp_anylock(lsrec_t *a, lsrec_t *b)
{
	int i;

	if (a->ls_event < b->ls_event)
		return (-1);
	if (a->ls_event > b->ls_event)
		return (1);

	for (i = stk_depth - 1; i >= 0; i--) {
		if (a->ls_stack[i] < b->ls_stack[i])
			return (-1);
		if (a->ls_stack[i] > b->ls_stack[i])
			return (1);
	}

	if (a->ls_caller < b->ls_caller)
		return (-1);
	if (a->ls_caller > b->ls_caller)
		return (1);

	return (b->ls_count - a->ls_count);
}

static void
mergesort(int (*cmp)(lsrec_t *, lsrec_t *), lsrec_t **a, lsrec_t **b, int n)
{
	int m = n / 2;
	int i, j;

	if (m > 1)
		mergesort(cmp, a, b, m);
	if (n - m > 1)
		mergesort(cmp, a + m, b + m, n - m);
	for (i = m; i > 0; i--)
		b[i - 1] = a[i - 1];
	for (j = m - 1; j < n - 1; j++)
		b[n + m - j - 2] = a[j + 1];
	while (i < j)
		*a++ = cmp(b[i], b[j]) < 0 ? b[i++] : b[j--];
	*a = b[i];
}

static void
coalesce(int (*cmp)(lsrec_t *, lsrec_t *), lsrec_t **lock, int n)
{
	int i, j;
	lsrec_t *target, *current;

	target = lock[0];

	for (i = 1; i < n; i++) {
		current = lock[i];
		if (cmp(current, target) != 0) {
			target = current;
			continue;
		}
		current->ls_event = LS_MAX_EVENTS;
		target->ls_count += current->ls_count;
		target->ls_refcnt += current->ls_refcnt;
		if (lsctl.lc_recsize < LS_TIME)
			continue;
		target->ls_time += current->ls_time;
		if (lsctl.lc_recsize < LS_HIST)
			continue;
		for (j = 0; j < 64; j++)
			target->ls_hist[j] += current->ls_hist[j];
	}
}

static void
coalesce_symbol(uintptr_t *addrp)
{
	uintptr_t symoff;
	size_t symsize;

	if (addr_to_sym(*addrp, &symoff, &symsize) != NULL && symoff < symsize)
		*addrp -= symoff;
}

int
main(int argc, char **argv)
{
	uint_t data_buf_size;
	char *data_buf;
	lsrec_t *lsp, **current, **first, **sort_buf, **merge_buf;
	FILE *out = stdout;
	char c;
	pid_t child;
	int status;
	int i, fd;
	hrtime_t duration;
	char *addrp, *offp, *sizep, *evp, *lastp;
	uintptr_t addr;
	size_t size, off;
	int events_specified = 0;
	int tracing = 0;
	int exec_errno = 0;
	uint32_t event;

	/*
	 * Silently open and close the lockstat driver to get its
	 * symbols loaded.
	 */
	(void) close(open("/dev/lockstat", O_RDONLY));

	if (symtab_init() == -1)
		fail(1, "can't load kernel symbols");

	lsctl.lc_nrecs = DEFAULT_NRECS;

	/*
	 * Don't consider a pending event to be incomplete
	 * unless it's been around at least 1 second --
	 * otherwise the output will be mostly noise.
	 */
	lsctl.lc_min_duration[LS_EXIT_FAILED] = NANOSEC;

	while ((c = getopt(argc, argv,
	    "bths:n:d:i:l:f:e:ckwWgCHEATID:RpPo:")) != EOF) {
		switch (c) {

		case 'b':
			lsctl.lc_recsize = LS_BASIC;
			break;

		case 't':
			lsctl.lc_recsize = LS_TIME;
			break;

		case 'h':
			lsctl.lc_recsize = LS_HIST;
			break;

		case 's':
			if (!isdigit(optarg[0]))
				usage();
			stk_depth = atoi(optarg);
			if (stk_depth > LS_MAX_STACK_DEPTH)
				fail(0, "max stack depth is %d",
				    LS_MAX_STACK_DEPTH);
			lsctl.lc_recsize = LS_STACK(stk_depth);
			break;

		case 'n':
			if (!isdigit(optarg[0]))
				usage();
			lsctl.lc_nrecs = atoi(optarg);
			break;

		case 'd':
			if (!isdigit(optarg[0]))
				usage();
			duration = atoll(optarg);

			/*
			 * XXX -- durations really should be per event
			 * since the units are different, but it's hard
			 * to express this nicely in the interface.
			 * Not clear yet what the cleanest solution is.
			 */
			for (i = 0; i < LS_MAX_EVENTS; i++)
				if (ls_event_info[i].ev_type != 'E')
					lsctl.lc_min_duration[i] = duration;

			break;

		case 'i':
			if (!isdigit(optarg[0]))
				usage();
			i = atoi(optarg);
			if (i <= 0)
				usage();
			if (i > MAX_HZ)
				fail(0, "max interrupt rate is %d Hz", MAX_HZ);
			lsctl.lc_interval = (hrtime_t)NANOSEC / i;
			break;

		case 'l':
		case 'f':
			addrp = strtok(optarg, ",");
			sizep = strtok(NULL, ",");
			addrp = strtok(optarg, ",+");
			offp = strtok(NULL, ",");

			size = sizep ? strtoul(sizep, NULL, 0) : 1;
			off = offp ? strtoul(offp, NULL, 0) : 0;

			if (addrp[0] == '0') {
				addr = strtoul(addrp, NULL, 16) + off;
			} else {
				addr = sym_to_addr(addrp) + off;
				if (sizep == NULL)
					size = sym_size(addrp) - off;
				if (addr - off == 0)
					fail(0, "symbol '%s' not found", addrp);
				if (size == 0)
					size = 1;
			}

			if (c == 'l') {
				lsctl.lc_wlock[watched_locks].lw_base = addr;
				lsctl.lc_wlock[watched_locks].lw_size = size;
				if (++watched_locks > LS_MAX_WATCH)
					fail(0, "too many watched locks");
			} else {
				lsctl.lc_wfunc[watched_funcs].lw_base = addr;
				lsctl.lc_wfunc[watched_funcs].lw_size = size;
				if (++watched_funcs > LS_MAX_WATCH)
					fail(0, "too many watched functions");
			}
			break;

		case 'e':
			evp = strtok_r(optarg, ",", &lastp);
			while (evp) {
				int ev1, ev2;
				char *evp2;

				(void) strtok(evp, "-");
				evp2 = strtok(NULL, "-");
				ev1 = atoi(evp);
				ev2 = evp2 ? atoi(evp2) : ev1;
				if ((uint_t)ev1 >= LS_MAX_EVENTS ||
				    (uint_t)ev2 >= LS_MAX_EVENTS || ev1 > ev2)
					fail(0, "-e events out of range");
				for (i = ev1; i <= ev2; i++)
					lsctl.lc_event[i] |= LSE_RECORD;
				evp = strtok_r(NULL, ",", &lastp);
			}
			events_specified = 1;
			break;

		case 'c':
			cflag = 1;
			break;

		case 'k':
			kflag = 1;
			break;

		case 'w':
			wflag = 1;
			break;

		case 'W':
			Wflag = 1;
			break;

		case 'g':
			gflag = 1;
			break;

		case 'C':
		case 'E':
		case 'H':
		case 'I':
			for (i = 0; i < LS_MAX_EVENTS; i++)
				if (ls_event_info[i].ev_type == c)
					lsctl.lc_event[i] |= LSE_RECORD;
			events_specified = 1;
			break;

		case 'A':
			for (i = 0; i < LS_MAX_EVENTS; i++)
				if (strchr("CEH", ls_event_info[i].ev_type))
					lsctl.lc_event[i] |= LSE_RECORD;
			events_specified = 1;
			break;

		case 'T':
			for (i = 0; i < LS_MAX_EVENTS; i++)
				lsctl.lc_event[i] |= LSE_TRACE;
			tracing = 1;
			break;

		case 'D':
			if (!isdigit(optarg[0]))
				usage();
			top_n = atoi(optarg);
			break;

		case 'R':
			do_rates = 1;
			break;

		case 'p':
			pflag = 1;
			break;

		case 'P':
			Pflag = 1;
			break;

		case 'o':
			if ((out = fopen(optarg, "w")) == NULL)
				fail(1, "error opening file");
			break;

		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (lsctl.lc_recsize == 0) {
		if (gflag) {
			stk_depth = LS_MAX_STACK_DEPTH;
			lsctl.lc_recsize = LS_STACK(stk_depth);
		} else {
			lsctl.lc_recsize = LS_TIME;
		}
	}

	/*
	 * Make sure the alignment is reasonable
	 */
	lsctl.lc_recsize = -(-lsctl.lc_recsize & -sizeof (uint64_t));

	for (i = 0; i < LS_MAX_EVENTS; i++) {
		/*
		 * If no events were specified, enable -C and -E.
		 */
		if (!events_specified && (ls_event_info[i].ev_type == 'C' ||
		    ls_event_info[i].ev_type == 'E'))
			lsctl.lc_event[i] |= LSE_RECORD;

		/*
		 * For each enabled hold-time event, set LSE_ENTER (unless
		 * recording mode is LS_BASIC) and LSE_EXIT (regardless).
		 * If we're doing LS_BASIC stats we don't need LSE_ENTER
		 * events since we don't have to measure time intervals.
		 */
		if ((lsctl.lc_event[i] & LSE_RECORD) &&
		    ls_event_info[i].ev_type == 'H') {
			if (lsctl.lc_recsize != LS_BASIC)
				lsctl.lc_event[i] |= LSE_ENTER;
			lsctl.lc_event[i] |= LSE_EXIT;
		}

		/*
		 * If interrupt events are enabled and no interrupt
		 * rate has been specified, use the default.
		 */
		if ((lsctl.lc_event[i] & LSE_RECORD) &&
		    ls_event_info[i].ev_type == 'I' && lsctl.lc_interval == 0)
			lsctl.lc_interval = (hrtime_t)NANOSEC / DEFAULT_HZ;
	}

	/*
	 * Make sure there is a child command to execute
	 */
	if (argc <= 0)
		usage();

	/*
	 * start the experiment
	 */
	if ((fd = open("/dev/lockstat", O_RDWR)) == -1)
		fail(1, "cannot open /dev/lockstat");

	data_buf_size = (lsctl.lc_nrecs + 1) * lsctl.lc_recsize;

	if ((data_buf = memalign(sizeof (uint64_t), data_buf_size)) == NULL)
		fail(1, "Memory allocation failed");

	if (write(fd, &lsctl, sizeof (lsctl_t)) != sizeof (lsctl_t))
		fail(1, "Cannot start experiment");

	elapsed_time = -gethrtime();

	/*
	 * Spawn the specified command and wait for it to complete.
	 */
	child = vfork();
	if (child == -1)
		fail(1, "cannot fork");
	if (child == 0) {
		(void) close(fd);
		(void) execvp(argv[0], &argv[0]);
		exec_errno = errno;
		exit(127);
	}
	while (waitpid(child, &status, WEXITED) != child)
		continue;

	elapsed_time += gethrtime();

	if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) != 0) {
			if (exec_errno != 0) {
				errno = exec_errno;
				fail(1, "could not execute %s", argv[0]);
			}
			(void) fprintf(stderr,
			    "lockstat: warning: %s exited with code %d\n",
				argv[0], WEXITSTATUS(status));
		}
	} else {
		(void) fprintf(stderr,
		    "lockstat: warning: %s died on signal %d\n",
			argv[0], WTERMSIG(status));
	}


	/*
	 * Gather the data sample
	 */
	(void) lseek(fd, 0, SEEK_SET);
	if ((nrecs_used = read(fd, data_buf, data_buf_size)) == -1)
		fail(1, "Cannot read from lockstat driver");
	nrecs_used /= lsctl.lc_recsize;
	(void) close(fd);

	if (nrecs_used >= lsctl.lc_nrecs)
		(void) fprintf(stderr, "lockstat: warning: "
		    "ran out of data records (use -n for more)\n");

	lsctl.lc_nrecs = nrecs_used;

	for (i = 0, lsp = (lsrec_t *)data_buf; i < nrecs_used; i++,
	    lsp = (lsrec_t *)((char *)lsp + lsctl.lc_recsize)) {
		ev_count[lsp->ls_event] += lsp->ls_count;
		ev_time[lsp->ls_event] += lsp->ls_time;
	}

	/*
	 * If -g was specified, convert stacks into individual records.
	 */
	if (gflag) {
		lsrec_t *newlsp, *oldlsp;

		newlsp = memalign(sizeof (uint64_t),
		    nrecs_used * LS_TIME * (stk_depth + 1));
		if (newlsp == NULL)
			fail(1, "Cannot allocate space for -g processing");
		lsp = newlsp;
		for (i = 0, oldlsp = (lsrec_t *)data_buf; i < nrecs_used; i++,
		    oldlsp = (lsrec_t *)((char *)oldlsp + lsctl.lc_recsize)) {
			int fr;
			int caller_in_stack = 0;

			if (oldlsp->ls_count == 0)
				continue;

			for (fr = 0; fr < stk_depth; fr++) {
				if (oldlsp->ls_stack[fr] == 0)
					break;
				if (oldlsp->ls_stack[fr] == oldlsp->ls_caller)
					caller_in_stack = 1;
				bcopy(oldlsp, lsp, LS_TIME);
				lsp->ls_caller = oldlsp->ls_stack[fr];
				lsp = (lsrec_t *)((char *)lsp + LS_TIME);
			}
			if (!caller_in_stack) {
				bcopy(oldlsp, lsp, LS_TIME);
				lsp = (lsrec_t *)((char *)lsp + LS_TIME);
			}
		}
		lsctl.lc_nrecs = nrecs_used =
		    ((uintptr_t)lsp - (uintptr_t)newlsp) / LS_TIME;
		lsctl.lc_recsize = LS_TIME;
		stk_depth = 0;
		free(data_buf);
		data_buf = (char *)newlsp;
	}

	if ((sort_buf = calloc(2 * (lsctl.lc_nrecs + 1),
	    sizeof (void *))) == NULL)
		fail(1, "Sort buffer allocation failed");
	merge_buf = sort_buf + (lsctl.lc_nrecs + 1);

	/*
	 * Build the sort buffer, discarding zero-count records along the way.
	 */
	for (i = 0, lsp = (lsrec_t *)data_buf; i < nrecs_used; i++,
	    lsp = (lsrec_t *)((char *)lsp + lsctl.lc_recsize)) {
		if (lsp->ls_count == 0)
			lsp->ls_event = LS_MAX_EVENTS;
		sort_buf[i] = lsp;
	}

	if (nrecs_used == 0)
		exit(0);

	/*
	 * Add a sentinel after the last record
	 */
	sort_buf[i] = lsp;
	lsp->ls_event = LS_MAX_EVENTS;

	if (tracing) {
		report_trace(out, sort_buf);
		return (0);
	}

	/*
	 * Application of -g may have resulted in multiple records
	 * with the same signature; coalesce them.
	 */
	if (gflag) {
		mergesort(lockcmp, sort_buf, merge_buf, nrecs_used);
		coalesce(lockcmp, sort_buf, nrecs_used);
	}

	/*
	 * Coalesce locks within the same symbol if -c option specified.
	 * Coalesce PCs within the same function if -k option specified.
	 */
	if (cflag || kflag) {
		for (i = 0; i < nrecs_used; i++) {
			int fr;
			lsp = sort_buf[i];
			if (cflag)
				coalesce_symbol(&lsp->ls_lock);
			if (kflag) {
				for (fr = 0; fr < stk_depth; fr++)
					coalesce_symbol(&lsp->ls_stack[fr]);
				coalesce_symbol(&lsp->ls_caller);
			}
		}
		mergesort(lockcmp, sort_buf, merge_buf, nrecs_used);
		coalesce(lockcmp, sort_buf, nrecs_used);
	}

	/*
	 * Coalesce callers if -w option specified
	 */
	if (wflag) {
		mergesort(lock_and_count_cmp_anywhere,
		    sort_buf, merge_buf, nrecs_used);
		coalesce(lockcmp_anywhere, sort_buf, nrecs_used);
	}

	/*
	 * Coalesce locks if -W option specified
	 */
	if (Wflag) {
		mergesort(site_and_count_cmp_anylock,
		    sort_buf, merge_buf, nrecs_used);
		coalesce(sitecmp_anylock, sort_buf, nrecs_used);
	}

	/*
	 * Sort data by contention count (ls_count) or total time (ls_time),
	 * depending on Pflag.  Override Pflag if time wasn't measured.
	 */
	if (lsctl.lc_recsize < LS_TIME)
		Pflag = 0;

	if (Pflag)
		mergesort(timecmp, sort_buf, merge_buf, nrecs_used);
	else
		mergesort(countcmp, sort_buf, merge_buf, nrecs_used);

	/*
	 * Display data by event type
	 */
	first = &sort_buf[0];
	while ((event = (*first)->ls_event) < LS_MAX_EVENTS) {
		current = first;
		while ((lsp = *current)->ls_event == event)
			current++;
		report_stats(out, first, current - first, ev_count[event],
		    ev_time[event]);
		first = current;
	}

	return (0);
}

static char *
format_symbol(char *buf, uintptr_t addr, int show_size)
{
	uintptr_t symoff;
	char *symname;
	size_t symsize;

	symname = addr_to_sym(addr, &symoff, &symsize);

	if (show_size && symoff == 0)
		(void) sprintf(buf, "%s[%d]", symname, symsize);
	else if (symoff == 0)
		(void) sprintf(buf, "%s", symname);
	else if (symoff < 16 && bcmp(symname, "cpu[", 4) == 0)	/* CPU+PIL */
		(void) sprintf(buf, "%s+%lu", symname, symoff);
	else if (symoff <= symsize || (symoff < 256 && addr != symoff))
		(void) sprintf(buf, "%s+0x%lx", symname, symoff);
	else
		(void) sprintf(buf, "0x%lx", addr);
	return (buf);
}

static void
report_stats(FILE *out, lsrec_t **sort_buf, size_t nrecs, uint64_t total_count,
	uint64_t total_time)
{
	uint32_t event = sort_buf[0]->ls_event;
	lsrec_t *lsp;
	double ptotal = 0.0;
	double percent;
	int i, j, fr;
	int displayed;
	int first_bin, last_bin, max_bin_count, total_bin_count;
	int rectype;
	char buf[256];
	char lhdr[80], chdr[80];

	rectype = lsctl.lc_recsize;
	if (event == LS_RECORD_FAILED)
		rectype = LS_BASIC;

	if (top_n == 0) {
		(void) fprintf(out, "%20llu %s\n",
		    do_rates == 0 ? total_count :
		    (uint_t)(((uint64_t)total_count * NANOSEC) / elapsed_time),
		    ls_event_info[event].ev_desc);
		return;
	}

	(void) sprintf(lhdr, "%s%s",
	    Wflag ? "Hottest " : "", ls_event_info[event].ev_lhdr);
	(void) sprintf(chdr, "%s%s",
	    wflag ? "Hottest " : "", "Caller");

	if (!pflag)
		(void) fprintf(out,
		    "\n%s: %.0f events in %.3f seconds (%.0f events/sec)\n\n",
		    ls_event_info[event].ev_desc, (double)total_count,
		    (double)elapsed_time / NANOSEC,
		    (double)total_count * NANOSEC / elapsed_time);

	if (!pflag && rectype < LS_HIST) {
		(void) sprintf(buf, "%s", ls_event_info[event].ev_units);
		(void) fprintf(out, "%5s %4s %4s %4s %8s %-22s %-24s\n",
		    do_rates ? "ops/s" : "Count",
		    gflag ? "genr" : "indv",
		    "cuml", "rcnt", rectype >= LS_TIME ? buf : "", lhdr, chdr);
		(void) fprintf(out, "---------------------------------"
		    "----------------------------------------------\n");
	}

	displayed = 0;
	for (i = 0; i < nrecs; i++) {
		lsp = sort_buf[i];

		if (displayed++ >= top_n)
			break;

		if (pflag) {
			int j;

			(void) fprintf(out, "%u %u",
			    lsp->ls_event, lsp->ls_count);
			(void) fprintf(out, " %s",
			    format_symbol(buf, lsp->ls_lock, cflag));
			(void) fprintf(out, " %s",
			    format_symbol(buf, lsp->ls_caller, 0));
			(void) fprintf(out, " %f",
			    (double)lsp->ls_refcnt / lsp->ls_count);
			if (rectype >= LS_TIME)
				(void) fprintf(out, " %llu", lsp->ls_time);
			if (rectype >= LS_HIST) {
				for (j = 0; j < 64; j++)
					(void) fprintf(out, " %u",
					    lsp->ls_hist[j]);
			}
			for (j = 0; j < LS_MAX_STACK_DEPTH; j++) {
				if (rectype <= LS_STACK(j) ||
				    lsp->ls_stack[j] == 0)
					break;
				(void) fprintf(out, " %s",
				    format_symbol(buf, lsp->ls_stack[j], 0));
			}
			(void) fprintf(out, "\n");
			continue;
		}

		if (rectype >= LS_HIST) {
			(void) fprintf(out, "---------------------------------"
			    "----------------------------------------------\n");
			(void) sprintf(buf, "%s",
			    ls_event_info[event].ev_units);
			(void) fprintf(out, "%5s %4s %4s %4s %8s %-22s %-24s\n",
			    do_rates ? "ops/s" : "Count",
			    gflag ? "genr" : "indv",
			    "cuml", "rcnt", buf, lhdr, chdr);
		}

		if (Pflag && total_time != 0)
			percent = (lsp->ls_time * 100.00) / total_time;
		else
			percent = (lsp->ls_count * 100.00) / total_count;

		ptotal += percent;

		if (rectype >= LS_TIME)
			(void) sprintf(buf, "%llu",
			    lsp->ls_time / lsp->ls_count);
		else
			buf[0] = '\0';

		(void) fprintf(out, "%5llu ",
		    do_rates == 0 ? lsp->ls_count :
		    ((uint64_t)lsp->ls_count * NANOSEC) /elapsed_time);

		(void) fprintf(out, "%3.0f%% ", percent);

		if (gflag)
			(void) fprintf(out, "---- ");
		else
			(void) fprintf(out, "%3.0f%% ", ptotal);

		(void) fprintf(out, "%4.2f %8s ",
		    (double)lsp->ls_refcnt / lsp->ls_count, buf);

		(void) fprintf(out, "%-22s ",
		    format_symbol(buf, lsp->ls_lock, cflag));

		(void) fprintf(out, "%-24s\n",
		    format_symbol(buf, lsp->ls_caller, 0));

		if (rectype < LS_HIST)
			continue;

		(void) fprintf(out, "\n");
		(void) fprintf(out, "%10s %31s %-9s %-24s\n",
			ls_event_info[event].ev_units,
			"------ Time Distribution ------",
			do_rates ? "ops/s" : "count",
			rectype > LS_STACK(0) ? "Stack" : "");

		first_bin = 0;
		while (lsp->ls_hist[first_bin] == 0)
			first_bin++;

		last_bin = 63;
		while (lsp->ls_hist[last_bin] == 0)
			last_bin--;

		max_bin_count = 0;
		total_bin_count = 0;
		for (j = first_bin; j <= last_bin; j++) {
			total_bin_count += lsp->ls_hist[j];
			if (lsp->ls_hist[j] > max_bin_count)
				max_bin_count = lsp->ls_hist[j];
		}

		/*
		 * If we went a few frames below the caller, ignore them
		 */
		for (fr = 3; fr > 0; fr--)
			if (lsp->ls_stack[fr] == lsp->ls_caller)
				break;

		for (j = first_bin; j <= last_bin; j++) {
			uint_t depth = (lsp->ls_hist[j] * 30) / total_bin_count;
			(void) fprintf(out, "%10llu |%s%s %-9u ",
			    1ULL << j,
			    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" + 30 - depth,
			    "                              " + depth,
			    do_rates == 0 ? lsp->ls_hist[j] :
			    (uint_t)(((uint64_t)lsp->ls_hist[j] * NANOSEC) /
			    elapsed_time));
			if (rectype <= LS_STACK(fr) || lsp->ls_stack[fr] == 0) {
				(void) fprintf(out, "\n");
				continue;
			}
			(void) fprintf(out, "%-24s\n",
			    format_symbol(buf, lsp->ls_stack[fr], 0));
			fr++;
		}
		while (rectype > LS_STACK(fr) && lsp->ls_stack[fr] != 0) {
			(void) fprintf(out, "%15s %-36s %-24s\n", "", "",
			    format_symbol(buf, lsp->ls_stack[fr], 0));
			fr++;
		}
	}

	if (!pflag)
		(void) fprintf(out, "---------------------------------"
		    "----------------------------------------------\n");

	(void) fflush(out);
}

static void
report_trace(FILE *out, lsrec_t **sort_buf)
{
	lsrec_t *lsp;
	int i, fr;
	int rectype;
	char buf[256], buf2[256];

	rectype = lsctl.lc_recsize;

	if (!pflag) {
		(void) fprintf(out, "%5s  %7s  %11s  %-24s  %-24s\n",
		    "Event", "Time", "Owner", "Lock", "Caller");
		(void) fprintf(out, "---------------------------------"
		    "----------------------------------------------\n");
	}

	for (i = 0; i < lsctl.lc_nrecs; i++) {

		lsp = sort_buf[i];

		if (lsp->ls_event >= LS_MAX_EVENTS || lsp->ls_count == 0)
			continue;

		(void) fprintf(out, "%2d  %10llu  %11p  %-24s  %-24s\n",
		    lsp->ls_event, lsp->ls_time, lsp->ls_next,
		    format_symbol(buf, lsp->ls_lock, 0),
		    format_symbol(buf2, lsp->ls_caller, 0));

		if (rectype <= LS_STACK(0) || lsp->ls_event == LS_RECORD_FAILED)
			continue;

		/*
		 * If we went a few frames below the caller, ignore them
		 */
		for (fr = 3; fr > 0; fr--)
			if (lsp->ls_stack[fr] == lsp->ls_caller)
				break;

		while (rectype > LS_STACK(fr) && lsp->ls_stack[fr] != 0) {
			(void) fprintf(out, "%53s  %-24s\n", "",
			    format_symbol(buf, lsp->ls_stack[fr], 0));
			fr++;
		}
		(void) fprintf(out, "\n");
	}

	(void) fflush(out);
}

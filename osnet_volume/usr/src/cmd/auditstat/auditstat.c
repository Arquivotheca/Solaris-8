#ifndef lint
static char	sccsid[] = "@(#)auditstat.c 1.4 97/10/29 SMI;";
static char	cmw_sccsid[] = "@(#)auditstat.c 2.3 92/01/30 SMI; SunOS CMW";
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <bsm/audit.h>
#include <bsm/libbsm.h>
#include <unistd.h>

/*
 * Display header every HEADER_MOD lines printed
 */
#define		DFLT_HEADER_MOD (20)
#define		ONEK (1024)

#define		CFLG (0x01)
#define		HFLG (0x02)
#define		IFLG (0x04)
#define		NFLG (0x08)
#define		VFLG (0x10)

extern char	*optarg;

static int	count;
static int	flags;
static int	header_mod = DFLT_HEADER_MOD;
static int	interval;

static void	display_stats();
static void	eauditon();
static void	parse_args();
static void	usage_exit();
static int	strisdigit();

main(argc, argv)
int	argc;
char	**argv;
{
	register int	i;
	au_stat_t s;

	(void) setbuf(stdout, (char *)0);
	(void) setbuf(stderr, (char *)0);

	parse_args(argc, argv);

	if (!flags) {
		eauditon(A_GETSTAT, (caddr_t) & s, NULL);
		display_stats(&s, 0);
		exit(0);
	}

	if (flags & VFLG || flags & NFLG)
		eauditon(A_GETSTAT, (caddr_t) & s, NULL);

	if (flags & VFLG)
		(void) printf("version = %d\n", s.as_version);

	if (flags & NFLG)
		(void) printf("number of kernel events = %d\n", s.as_numevent);

	if (!(flags & IFLG))
		exit(0);

	for (i = 0; /* EMPTY */; i++) {
		eauditon(A_GETSTAT, (caddr_t) & s, NULL);
		display_stats(&s, i);
		if ((flags & CFLG) && count)
			if (i == count - 1)
				break;
		sleep(interval);
	}

	exit(0);
}


static void
display_stats(s, cnt)
au_stat_t *s;
{
	int	offset[12];   /* used to line the header up correctly */
	char	buf[512];

	(void) sprintf(buf,
		"%4u %n%4u %n%4u %n%4u %n%4u %n%4u %n%4u %n%4u %n%4u %n%4u %n%4u %n%4u%n",
		s->as_generated, 	&(offset[0]),
		s->as_nonattrib, 	&(offset[1]),
		s->as_kernel, 		&(offset[2]),
		s->as_audit, 		&(offset[3]),
		s->as_auditctl, 	&(offset[4]),
		s->as_enqueue, 		&(offset[5]),
		s->as_written, 		&(offset[6]),
		s->as_wblocked, 	&(offset[7]),
		s->as_rblocked, 	&(offset[8]),
		s->as_dropped, 		&(offset[9]),
		s->as_totalsize / ONEK,	&(offset[10]),
		s->as_memused / ONEK, 	&(offset[11]));

	/* print a properly aligned header every HEADER_MOD lines */
	if (header_mod && (!cnt || !(cnt % header_mod))) {
		(void) printf(
			"%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s\n",
			offset[0] - 1,			"gen",
			offset[1] - offset[0] - 1,	"nona",
			offset[2] - offset[1] - 1,	"kern",
			offset[3] - offset[2] - 1,	"aud",
			offset[4] - offset[3] - 1,	"ctl",
			offset[5] - offset[4] - 1,	"enq",
			offset[6] - offset[5] - 1,	"wrtn",
			offset[7] - offset[6] - 1,	"wblk",
			offset[8] - offset[7] - 1,	"rblk",
			offset[9] - offset[8] - 1,	"drop",
			offset[10] - offset[9] - 1,	"tot",
			offset[11] - offset[10],	"mem");
	}

	(void) puts(buf);

	return;
}


static void
eauditon(cmd, data, length)
int	cmd;
caddr_t data;
int	length;
{
	if (auditon(cmd, data, length) == -1) {
		perror("auditstat: auditon");
		exit(1);
	}

	return;
}


static void
parse_args(argc, argv)
int	argc;
char	**argv;
{
	int	c;

	while ((c = getopt(argc, argv, "c:h:i:vn")) != -1) {
		switch (c) {
		case 'c':
			if (flags & CFLG)
				usage_exit();
			flags |= CFLG;
			if (strisdigit(optarg)) {
				(void) fprintf(stderr,
				"auditstat: invalid count specified.\n");
				exit(1);
			}
			count = atoi(optarg);
			break;
		case 'h':
			if (flags & HFLG)
				usage_exit();
			flags |= HFLG;
			if (strisdigit(optarg)) {
				(void) fprintf(stderr,
				"auditstat: invalid header arg specified.\n");
				exit(1);
			}
			header_mod = atoi(optarg);
			break;
		case 'i':
			if (flags & IFLG)
				usage_exit();
			flags |= IFLG;
			if (strisdigit(optarg)) {
				(void) fprintf(stderr,
				"auditstat: invalid interval specified.\n");
				exit(1);
			}
			interval = atoi(optarg);
			break;
		case 'n':
			if (flags & NFLG)
				usage_exit();
			flags |= NFLG;
			break;
		case 'v':
			if (flags & VFLG)
				usage_exit();
			flags |= VFLG;
			break;
		case '?':
		default:
			usage_exit();
			break;
		}
	}

	return;
}


static void
usage_exit()
{
	(void) fprintf(stderr,
		"auditstat: usage: auditstat [-c count] [-h lines] \
		[-i interval] [-n] [-v]\n");
	exit(1);
}


static int
strisdigit(s)
char	*s;
{
	for (; *s; s++)
		if (!isdigit(*s))
			return (1);

	return (0);
}

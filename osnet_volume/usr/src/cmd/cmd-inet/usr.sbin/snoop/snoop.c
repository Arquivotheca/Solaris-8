/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)snoop.c	1.23	99/12/15 SMI"	/* SunOS	*/

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <varargs.h>
#include <setjmp.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/mman.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/pfmod.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netdb.h>

#include "snoop.h"

int snaplen;
char *device = NULL;

/* Global error recovery variables */
sigjmp_buf jmp_env, ojmp_env;		/* error recovery jmp buf */
int snoop_nrecover;			/* number of recoveries on curr pkt */
int quitting;				/* user termination flag */

extern int encap_levels;		/* variables needing reset on error */
extern unsigned int total_encap_levels;

struct snoop_handler *snoop_hp;		/* global alarm handler head */
struct snoop_handler *snoop_tp;		/* global alarm handler tail */
time_t snoop_nalarm;			/* time of next alarm */

/* protected interpreter output areas */
#define	MAXSUM		8
#define	REDZONE		64
static char *sumline[MAXSUM];
static char *detail_line;
static char *line;
static char *encap;

int audio;
int maxcount;	/* maximum no of packets to capture */
int count;	/* count of packets captured */
int sumcount;
int x_offset = -1;
int x_length = 0x7fffffff;
FILE *namefile;
int Pflg;
boolean_t qflg = B_FALSE;
boolean_t rflg = B_FALSE;
#ifdef	DEBUG
boolean_t zflg = B_FALSE;		/* debugging packet corrupt flag */
#endif
struct Pf_ext_packetfilt pf;

void usage();
void show_count();
void snoop_sigrecover(int sig, siginfo_t *info, void *p);
static char *protmalloc(size_t);
static void resetperm(void);

main(argc, argv)
	int argc; char **argv;
{
	extern char *optarg;
	extern int optind;
	int c;
	int filter = 0;
	int flags = F_SUM;
	struct Pf_ext_packetfilt *fp = NULL;
	char *icapfile = NULL;
	char *ocapfile = NULL;
	int nflg = 0;
	int Nflg = 0;
	int Cflg = 0;
	int first = 1;
	int last  = 0x7fffffff;
	int ppa;
	int use_kern_pf;
	char *p, *p2;
	char names[256];
	char self[64];
	char *argstr = NULL;
	void (*proc)();
	extern void cap_write();
	extern void process_pkt();
	char *audiodev;
	int ret;
	struct sigaction sigact;
	stack_t sigstk;
	char *output_area;
	int nbytes;

	names[0] = '\0';
	/*
	 * Global error recovery: Prepare for interpreter failures
	 * with corrupted packets or confused interpreters.
	 * Allocate protected output and stack areas, with generous
	 * red-zones.
	 */
	nbytes = (MAXSUM + 3) * (MAXLINE + REDZONE);
	output_area = (char *)protmalloc(nbytes);
	if (output_area == NULL) {
		perror("Warning: mmap");
		exit(1);
	}

	/* Allocate protected output areas */
	for (ret = 0; ret < MAXSUM; ret++) {
		sumline[ret] = (char *)output_area;
		output_area += (MAXLINE + REDZONE);
	}
	detail_line = output_area;
	output_area += MAXLINE + REDZONE;
	line = output_area;
	output_area += MAXLINE + REDZONE;
	encap = output_area;
	output_area += MAXLINE + REDZONE;

	/* Initialize an alternate signal stack to increase robustness */
	if ((sigstk.ss_sp = (char *)malloc(SIGSTKSZ+REDZONE)) == NULL) {
		perror("Warning: malloc");
		exit(1);
	}
	sigstk.ss_size = SIGSTKSZ;
	sigstk.ss_flags = 0;
	if (sigaltstack(&sigstk, (stack_t *)NULL) < 0) {
		perror("Warning: sigaltstack");
		exit(1);
	}

	/* Initialize a master signal handler */
	sigact.sa_handler = NULL;
	sigact.sa_sigaction = snoop_sigrecover;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_ONSTACK|SA_SIGINFO;

	/* Register master signal handler */
	if (sigaction(SIGHUP, &sigact, (struct sigaction *)NULL) < 0) {
		perror("Warning: sigaction");
		exit(1);
	}
	if (sigaction(SIGINT, &sigact, (struct sigaction *)NULL) < 0) {
		perror("Warning: sigaction");
		exit(1);
	}
	if (sigaction(SIGQUIT, &sigact, (struct sigaction *)NULL) < 0) {
		perror("Warning: sigaction");
		exit(1);
	}
	if (sigaction(SIGILL, &sigact, (struct sigaction *)NULL) < 0) {
		perror("Warning: sigaction");
		exit(1);
	}
	if (sigaction(SIGTRAP, &sigact, (struct sigaction *)NULL) < 0) {
		perror("Warning: sigaction");
		exit(1);
	}
	if (sigaction(SIGIOT, &sigact, (struct sigaction *)NULL) < 0) {
		perror("Warning: sigaction");
		exit(1);
	}
	if (sigaction(SIGEMT, &sigact, (struct sigaction *)NULL) < 0) {
		perror("Warning: sigaction");
		exit(1);
	}
	if (sigaction(SIGFPE, &sigact, (struct sigaction *)NULL) < 0) {
		perror("Warning: sigaction");
		exit(1);
	}
	if (sigaction(SIGBUS, &sigact, (struct sigaction *)NULL) < 0) {
		perror("Warning: sigaction");
		exit(1);
	}
	if (sigaction(SIGSEGV, &sigact, (struct sigaction *)NULL) < 0) {
		perror("Warning: sigaction");
		exit(1);
	}
	if (sigaction(SIGSYS, &sigact, (struct sigaction *)NULL) < 0) {
		perror("Warning: sigaction");
		exit(1);
	}
	if (sigaction(SIGALRM, &sigact, (struct sigaction *)NULL) < 0) {
		perror("Warning: sigaction");
		exit(1);
	}
	if (sigaction(SIGTERM, &sigact, (struct sigaction *)NULL) < 0) {
		perror("Warning: sigaction");
		exit(1);
	}

	/* Prepare for failure during program initialization/exit */
	if (sigsetjmp(jmp_env, 1)) {
		exit(1);
	}
	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

	while ((c = getopt(argc, argv, "at:CPDSi:o:Nn:s:d:vVp:f:c:x:?rqz"))
				!= EOF) {
		switch (c) {
		case 'a':
			audiodev = getenv("AUDIODEV");
			if (audiodev == NULL)
				audiodev = "/dev/audio";
			audio = open(audiodev, 2);
			if (audio < 0) {
				pr_err("Audio device %s: %m",
					audiodev);
				exit(1);
			}
			break;
		case 't':
			flags |= F_TIME;
			switch (*optarg) {
			case 'r':	flags |= F_RTIME; break;
			case 'a':	flags |= F_ATIME; break;
			case 'd':	break;
			default:	usage();
			}
			break;
		case 'P':
			Pflg++;
			break;
		case 'D':
			flags |= F_DROPS;
			break;
		case 'S':
			flags |= F_LEN;
			break;
		case 'i':
			icapfile = optarg;
			break;
		case 'o':
			ocapfile = optarg;
			break;
		case 'N':
			Nflg++;
			break;
		case 'n':
			nflg++;
			(void) strcpy(names, optarg);
			break;
		case 's':
			snaplen = atoi(optarg);
			break;
		case 'd':
			device = optarg;
			break;
		case 'v':
			flags &= ~(F_SUM);
			flags |= F_DTAIL;
			break;
		case 'V':
			flags |= F_ALLSUM;
			break;
		case 'p':
			p = optarg;
			p2 = strpbrk(p, ",:-");
			if (p2 == NULL) {
				first = last = atoi(p);
			} else {
				*p2++ = '\0';
				first = atoi(p);
				last = atoi(p2);
			}
			break;
		case 'f':
			(void) gethostname(self, sizeof (self));
			p = strchr(optarg, ':');
			if (p) {
				*p = '\0';
				if (strcmp(optarg, self) == 0 ||
				    strcmp(p+1, self) == 0)
				(void) fprintf(stderr,
				"Warning: cannot capture packets from %s\n",
					self);
				*p = ' ';
			} else if (strcmp(optarg, self) == 0)
				(void) fprintf(stderr,
				"Warning: cannot capture packets from %s\n",
					self);
			argstr = optarg;
			break;
		case 'x':
			p = optarg;
			p2 = strpbrk(p, ",:-");
			if (p2 == NULL) {
				x_offset = atoi(p);
				x_length = -1;
			} else {
				*p2++ = '\0';
				x_offset = atoi(p);
				x_length = atoi(p2);
			}
			break;
		case 'c':
			maxcount = atoi(optarg);
			break;
		case 'C':
			Cflg++;
			break;
		case 'q':
			qflg = B_TRUE;
			break;
		case 'r':
			rflg = B_TRUE;
			break;
#ifdef	DEBUG
		case 'z':
			zflg = B_TRUE;
			break;
#endif	/* DEBUG */
		case '?':
		default:
			usage();
		}
	}

	if (argc > optind)
		argstr = (char *)concat_args(&argv[optind], argc - optind);

	/*
	 * Need to know before we decide on filtering method some things
	 * about the interface.  So, go ahead and do part of the initialization
	 * now so we have that data.  Note that if no device is specified,
	 * check_device selects one and returns it.  In an ideal world,
	 * it might be nice if the "correct" interface for the filter
	 * requested was chosen, but that's too hard.
	 */
	if (!icapfile) {
		use_kern_pf = check_device(&device, &ppa);
	} else {
		cap_open_read(icapfile);

		if (!nflg) {
			names[0] = '\0';
			(void) strcpy(names, icapfile);
			(void) strcat(names, ".names");
		}
	}

	/* attempt to read .names file if it exists before filtering */
	if (names[0] != '\0') {
		if (access(names, F_OK) == 0) {
			load_names(names);
		} else if (nflg) {
			(void) fprintf(stderr, "%s not found\n", names);
			exit(1);
		}
	}

	if (argstr) {
		if (!icapfile && use_kern_pf) {
			ret = pf_compile(argstr, Cflg);
			switch (ret) {
			case 0:
				filter++;
				compile(argstr, Cflg);
				break;
			case 1:
				fp = &pf;
				break;
			case 2:
				fp = &pf;
				filter++;
				break;
			}
		} else {
			filter++;
			compile(argstr, Cflg);
		}

		if (Cflg)
			exit(0);
	}

	if (flags & F_SUM)
		flags |= F_WHO;

	/*
	 * If the -o flag is set then capture packets
	 * directly to a file.  Don't attempt to
	 * interpret them on the fly (F_NOW).
	 * Note: capture to file is much less likely
	 * to drop packets since we don't spend cpu
	 * cycles running through the interpreters
	 * and possibly hanging in address-to-name
	 * mappings through the name service.
	 */
	if (ocapfile) {
		cap_open_write(ocapfile);
		proc = cap_write;
	} else {
		flags |= F_NOW;
		proc = process_pkt;
	}


	/*
	 * If the -i flag is set then get packets from
	 * the log file which has been previously captured
	 * with the -o option.
	 */
	if (icapfile) {
		names[0] = '\0';
		(void) strcpy(names, icapfile);
		(void) strcat(names, ".names");

		if (Nflg) {
			namefile = fopen(names, "w");
			if (namefile == NULL) {
				perror(names);
				exit(1);
			}
			flags = 0;
			(void) fprintf(stderr,
				"Creating name file %s\n", names);
		}

		if (flags & F_DTAIL)
			flags = F_DTAIL;
		else
			flags |= F_NUM | F_TIME;

		resetperm();
		cap_read(first, last, filter, proc, flags);

		if (Nflg)
			(void) fclose(namefile);

	} else {
		const int chunksize = 8 * 8192;
		struct timeval timeout;

		/*
		 * If listening to packets on audio
		 * then set the buffer timeout down
		 * to 1/10 sec.  A higher value
		 * makes the audio "bursty".
		 */
		if (audio) {
			timeout.tv_sec = 0;
			timeout.tv_usec = 100000;
		} else {
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
		}

		initdevice(device, snaplen, chunksize, &timeout, fp, ppa);
		if (! qflg && ocapfile)
			show_count();
		resetperm();
		net_read(chunksize, filter, proc, flags);

		if (!(flags & F_NOW))
			printf("\n");
	}

	if (ocapfile)
		cap_close();

	return (0);
}

int tone[] = {
0x034057, 0x026074, 0x136710, 0x126660, 0x147551, 0x034460,
0x026775, 0x141727, 0x127670, 0x156532, 0x036064, 0x030721,
0x134703, 0x126705, 0x046071, 0x030073, 0x036667, 0x140666,
0x126137, 0x064463, 0x031064, 0x072677, 0x135652, 0x141734,
0x036463, 0x027472, 0x137333, 0x127257, 0x152534, 0x033065,
0x027723, 0x136313, 0x127735, 0x053473, 0x035470, 0x052666,
0x167260, 0x140535, 0x045471, 0x034474, 0x132711, 0x132266,
0x047127, 0x027077, 0x043705, 0x141676, 0x134110, 0x063400,
};

/*
 * Make a sound on /dev/audio according
 * to the length of the packet.  The tone
 * data above is a piece of waveform from
 * a Pink Floyd track. The amount of waveform
 * used is a function of packet length e.g.
 * a series of small packets is heard as
 * clicks, whereas a series of NFS packets
 * in an 8k read sounds like a "WHAAAARP".
 *
 * Note: add 4 constant bytes to sound segments
 * to avoid an artifact of DBRI/MMCODEC that
 * results in a screech due to underrun (bug 114552).
 */
void
click(len)
	int len;
{
	len /= 8;
	len = len ? len : 4;

	if (audio) {
		write(audio, tone, len);
		write(audio, "\377\377\377\377", 4);
	}
}

/* Display a count of packets */
void
show_count()
{
	static int prev = -1;

	if (count == prev)
		return;

	prev = count;
	(void) fprintf(stderr, "\r%d ", count);
}

#define	ENCAP_LEN	16	/* Hold "(NN encap)" */

/*
 * Display data that's external to the packet.
 * This constitutes the first half of the summary
 * line display.
 */
void
show_pktinfo(flags, num, src, dst, ptvp, tvp, drops, len)
	int flags, num, drops, len;
	char *src, *dst;
	struct timeval *ptvp, *tvp;
{
	struct tm *tm;
	static struct timeval tvp0;
	int sec, usec;
	char *lp = line;
	int i, start;

	if (flags & F_NUM) {
		sprintf(lp, "%3d ", num);
		lp += strlen(lp);
	}
	tm = localtime(&tvp->tv_sec);

	if (flags & F_TIME) {
		if (flags & F_ATIME) {
			sprintf(lp, "%d:%02d:%d.%05d ",
				tm->tm_hour, tm->tm_min, tm->tm_sec,
				tvp->tv_usec / 10);
			lp += strlen(lp);
		} else {
			if (flags & F_RTIME) {
				if (tvp0.tv_sec == 0) {
					tvp0.tv_sec = tvp->tv_sec;
					tvp0.tv_usec = tvp->tv_usec;
				}
				ptvp = &tvp0;
			}
			sec  = tvp->tv_sec  - ptvp->tv_sec;
			usec = tvp->tv_usec - ptvp->tv_usec;
			if (usec < 0) {
				usec += 1000000;
				sec  -= 1;
			}
			sprintf(lp, "%3d.%05d ", sec, usec / 10);
			lp += strlen(lp);
		}
	}

	if (flags & F_WHO) {
		sprintf(lp, "%12s -> %-12s ", src, dst);
		lp += strlen(lp);
	}

	if (flags & F_DROPS) {
		sprintf(lp, "drops: %d ", drops);
		lp += strlen(lp);
	}

	if (flags & F_LEN) {
		sprintf(lp, "length: %4d  ", len);
		lp += strlen(lp);
	}

	if (flags & F_SUM) {
		if (flags & F_ALLSUM)
			printf("________________________________\n");

		start = flags & F_ALLSUM ? 0 : sumcount - 1;
		sprintf(encap, "  (%d encap)", total_encap_levels - 1);
		printf("%s%s%s\n", line, sumline[start],
		    ((flags & F_ALLSUM) || (total_encap_levels == 1)) ? "" :
			encap);

		for (i = start + 1; i < sumcount; i++)
			printf("%s%s\n", line, sumline[i]);

		sumcount = 0;
	}

	if (flags & F_DTAIL) {
		printf("%s\n\n", detail_line);
		detail_line[0] = '\0';
	}
}

/*
 * The following two routines are called back
 * from the interpreters to display their stuff.
 * The theory is that when snoop becomes a window
 * based tool we can just supply a new version of
 * get_sum_line and get_detail_line and not have
 * to touch the interpreters at all.
 */
char *
get_sum_line()
{
	int tsumcount = sumcount;

	if (sumcount >= MAXSUM) {
		sumcount = 0;			/* error recovery */
		pr_err(
		    "get_sum_line: sumline overflow (sumcount=%d, MAXSUM=%d)\n",
			tsumcount, MAXSUM);
	}

	sumline[sumcount][0] = '\0';
	return (sumline[sumcount++]);
}

/*ARGSUSED*/
char *
get_detail_line(off, len)
	int off, len;
{
	if (detail_line[0]) {
		printf("%s\n", detail_line);
		detail_line[0] = '\0';
	}
	return (detail_line);
}

/*
 * Print an error.
 * Works like printf (fmt string and variable args)
 * except that it will subsititute an error message
 * for a "%m" string (like syslog) and it calls
 * long_jump - it doesn't return to where it was
 * called from - it goes to the last setjmp().
 */
void
pr_err(fmt, va_alist)
	char *fmt;
	va_dcl
{
	va_list ap;
	char buf[BUFSIZ], *p2;
	char *p1;
	extern int errno;

	strcpy(buf, "snoop: ");
	p2 = buf + strlen(buf);

	for (p1 = fmt; *p1; p1++) {
		if (*p1 == '%' && *(p1+1) == 'm') {
			char *errstr;

			if ((errstr = strerror(errno)) != (char *)NULL) {
				(void) strcpy(p2, errstr);
				p2 += strlen(p2);
			}
			p1++;
		} else {
			*p2++ = *p1;
		}
	}
	if (p2 > buf && *(p2-1) != '\n')
		*p2++ = '\n';
	*p2 = '\0';

	va_start(ap);
	(void) vfprintf(stderr, buf, ap);
	va_end(ap);
	snoop_sigrecover(-1, NULL, NULL);	/* global error recovery */
}

/*
 * Ye olde usage proc
 * PLEASE keep this up to date!
 * Naive users *love* this stuff.
 */
void
usage()
{
	(void) fprintf(stderr, "\nUsage:  snoop\n");
	(void) fprintf(stderr,
	"\t[ -a ]			# Listen to packets on audio\n");
	(void) fprintf(stderr,
	"\t[ -d device ]		# settable to le?, ie?, bf?, tr?\n");
	(void) fprintf(stderr,
	"\t[ -s snaplen ]		# Truncate packets\n");
	(void) fprintf(stderr,
	"\t[ -c count ]		# Quit after count packets\n");
	(void) fprintf(stderr,
	"\t[ -P ]			# Turn OFF promiscuous mode\n");
	(void) fprintf(stderr,
	"\t[ -D ]			# Report dropped packets\n");
	(void) fprintf(stderr,
	"\t[ -S ]			# Report packet size\n");
	(void) fprintf(stderr,
	"\t[ -i file ]		# Read previously captured packets\n");
	(void) fprintf(stderr,
	"\t[ -o file ]		# Capture packets in file\n");
	(void) fprintf(stderr,
	"\t[ -n file ]		# Load addr-to-name table from file\n");
	(void) fprintf(stderr,
	"\t[ -N ]			# Create addr-to-name table\n");
	(void) fprintf(stderr,
	"\t[ -t  r|a|d ]		# Time: Relative, Absolute or Delta\n");
	(void) fprintf(stderr,
	"\t[ -v ]			# Verbose packet display\n");
	(void) fprintf(stderr,
	"\t[ -V ]			# Show all summary lines\n");
	(void) fprintf(stderr,
	"\t[ -p first[,last] ]	# Select packet(s) to display\n");
	(void) fprintf(stderr,
	"\t[ -x offset[,length] ]	# Hex dump from offset for length\n");
	(void) fprintf(stderr,
	"\t[ -C ]			# Print packet filter code\n");
	(void) fprintf(stderr,
	"\t[ -q ]			# Suppress printing packet count\n");
	(void) fprintf(stderr,
	"\t[ -r ]			# Do not resolve address to name\n");
	(void) fprintf(stderr,
	"\n\t[ filter expression ]\n");
	(void) fprintf(stderr, "\nExample:\n");
	(void) fprintf(stderr, "\tsnoop -o saved  host fred\n\n");
	(void) fprintf(stderr, "\tsnoop -i saved -tr -v -p19\n");
	exit(1);
}

/*
 * sdefault: default global alarm handler. Causes the current packet
 * to be skipped.
 */
static void
sdefault(void)
{
	snoop_nrecover = SNOOP_MAXRECOVER;
}

/*
 * snoop_alarm: register or unregister an alarm handler to be called after
 * s_sec seconds. Because snoop wasn't written to tolerate random signal
 * delivery, periodic SIGALRM delivery (or SA_RESTART) cannot be used.
 *
 * s_sec argument of 0 seconds unregisters the handler.
 * s_handler argument of NULL registers default handler sdefault(), or
 * unregisters all signal handlers (for error recovery).
 *
 * Variables must be volatile to force the compiler to not optimize
 * out the signal blocking.
 */
/*ARGSUSED*/
int
snoop_alarm(int s_sec, void (*s_handler)())
{
	volatile time_t now;
	volatile time_t nalarm = 0;
	volatile struct snoop_handler *sh = NULL;
	volatile struct snoop_handler *hp, *tp, *next;
	volatile sigset_t s_mask;
	volatile int ret = -1;

	sigemptyset((sigset_t *)&s_mask);
	sigaddset((sigset_t *)&s_mask, SIGALRM);
	if (s_sec < 0)
		return (-1);

	/* register an alarm handler */
	now = time(NULL);
	if (s_sec) {
		sh = malloc(sizeof (struct snoop_handler));
		sh->s_time = now + s_sec;
		if (s_handler == NULL)
			s_handler = sdefault;
		sh->s_handler = s_handler;
		sh->s_next = NULL;
		(void) sigprocmask(SIG_BLOCK, (sigset_t *)&s_mask, NULL);
		if (snoop_hp == NULL) {
			snoop_hp = snoop_tp = (struct snoop_handler *)sh;

			snoop_nalarm = sh->s_time;
			alarm(sh->s_time - now);
		} else {
			snoop_tp->s_next = (struct snoop_handler *)sh;
			snoop_tp = (struct snoop_handler *)sh;

			if (sh->s_time < snoop_nalarm) {
				snoop_nalarm = sh->s_time;
				(void) alarm(sh->s_time - now);
			}
		}
		(void) sigprocmask(SIG_UNBLOCK, (sigset_t *)&s_mask, NULL);

		return (0);
	}

	/* unregister an alarm handler */
	(void) sigprocmask(SIG_BLOCK, (sigset_t *)&s_mask, NULL);
	tp = (struct snoop_handler *)&snoop_hp;
	for (hp = snoop_hp; hp; hp = next) {
		next = hp->s_next;
		if (s_handler == NULL || hp->s_handler == s_handler) {
			ret = 0;
			tp->s_next = hp->s_next;
			if (snoop_tp == hp) {
				if (tp == (struct snoop_handler *)&snoop_hp)
					snoop_tp = NULL;
				else
					snoop_tp = (struct snoop_handler *)tp;
			}
			free((void *)hp);
		} else {
			if (nalarm == 0 || nalarm > hp->s_time)
				nalarm = now < hp->s_time ? hp->s_time :
					now + 1;
			tp = hp;
		}
	}
	/*
	 * Stop or adjust timer
	 */
	if (snoop_hp == NULL) {
		snoop_nalarm = 0;
		(void) alarm(0);
	} else if (nalarm > 0 && nalarm < snoop_nalarm) {
		snoop_nalarm = nalarm;
		(void) alarm(nalarm - now);
	}

	(void) sigprocmask(SIG_UNBLOCK, (sigset_t *)&s_mask, NULL);
	return (ret);
}

/*
 * snoop_recover: reset snoop's output area, and any internal variables,
 * to allow continuation.
 * XXX: make this an interface such that each interpreter can
 * register a reset routine.
 */
void
snoop_recover(void)
{
	int i;

	/* Error recovery: reset output_area and associated variables */
	for (i = 0; i < MAXSUM; i++)
		sumline[i][0] = '\0';
	detail_line[0] = '\0';
	line[0] = '\0';
	encap[0] = '\0';
	sumcount = 0;

	/* stacking/unstacking cannot be relied upon */
	encap_levels = 0;
	total_encap_levels = 1;

	/* remove any pending timeouts */
	(void) snoop_alarm(0, NULL);
}

/*
 * snoop_sigrecover: global sigaction routine to manage recovery
 * from catastrophic interpreter failures while interpreting
 * corrupt trace files/packets. SIGALRM timeouts, program errors,
 * and user termination are all handled. In the case of a corrupt
 * packet or confused interpreter, the packet will be skipped, and
 * execution will continue in scan().
 *
 * Global alarm handling (see snoop_alarm()) is managed here.
 *
 * Variables must be volatile to force the compiler to not optimize
 * out the signal blocking.
 */
/*ARGSUSED*/
void
snoop_sigrecover(int sig, siginfo_t *info, void *p)
{
	volatile time_t now;
	volatile time_t nalarm = 0;
	volatile struct snoop_handler *hp;

	/*
	 * Invoke any registered alarms. This involves first calculating
	 * the time for the next alarm, setting it up, then progressing
	 * through handler invocations. Note that since handlers may
	 * use siglongjmp(), in the worst case handlers may be serviced
	 * at a later time.
	 */
	if (sig == SIGALRM) {
		now = time(NULL);
		/* Calculate next alarm time */
		for (hp = snoop_hp; hp; hp = hp->s_next) {
			if (hp->s_time) {
				if ((hp->s_time - now) > 0) {
					if (nalarm == 0 || nalarm > hp->s_time)
						nalarm = now < hp->s_time ?
							hp->s_time : now + 1;
				}
			}
		}
		/* Setup next alarm */
		if (nalarm) {
			snoop_nalarm = nalarm;
			alarm(nalarm - now);
		} else {
			snoop_nalarm = 0;
		}

		/* Invoke alarm handlers (may not return) */
		for (hp = snoop_hp; hp; hp = hp->s_next) {
			if (hp->s_time) {
				if ((now - hp->s_time) >= 0) {
					hp->s_time = 0;	/* only invoke once */
					if (hp->s_handler)
						hp->s_handler();
				}
			}
		}
	} else {
		snoop_nrecover++;
	}

	/*
	 * Exit if a signal has occurred after snoop has begun the process
	 * of quitting.
	 */
	if (quitting)
		exit(1);

	/*
	 * If an alarm handler has timed out, and snoop_nrecover has
	 * reached SNOOP_MAXRECOVER, skip to the next packet.
	 *
	 * If any other signal has occurred, and snoop_nrecover has
	 * reached SNOOP_MAXRECOVER, give up.
	 */
	if (sig == SIGALRM) {
		if (snoop_nrecover >= SNOOP_MAXRECOVER) {
			fprintf(stderr,
				"snoop: WARNING: skipping from packet %d\n",
				count);
			snoop_nrecover = 0;
		} else {
			/* continue trying */
			return;
		}
	} else if (snoop_nrecover >= SNOOP_MAXRECOVER) {
		fprintf(stderr,
			"snoop: ERROR: cannot recover from packet %d\n", count);
		exit(1);
	}

#ifdef DEBUG
	fprintf(stderr, "snoop_sigrecover(%d, %p, %p)\n", sig, info, p);
#endif /* DEBUG */

	/*
	 * Prepare to quit. This allows final processing to occur
	 * after first terminal interruption.
	 */
	if (sig == SIGTERM || sig == SIGHUP || sig == SIGINT) {
		quitting = 1;
		return;
	} else if (sig != -1 && sig != SIGALRM) {
		/* Inform user that snoop has taken a fault */
		fprintf(stderr, "WARNING: received signal %d from packet %d\n",
				sig, count);
	}

	/* Reset interpreter variables */
	snoop_recover();

	/* Continue in scan() with the next packet */
	siglongjmp(jmp_env, 1);
	/*NOTREACHED*/
}

/*
 * protmalloc: protected malloc for global error recovery: Prepare
 * for interpreter failures with corrupted packets or confused
 * interpreters. Allocate protected output stack areas.
 * 3 pages are needed, in case mmap() returns an area not on a
 * page boundary.
 */
static char *
protmalloc(size_t nbytes)
{
	int psz = getpagesize();
	char *start;
	char *end;

	nbytes = (nbytes + psz - 1) & ~(psz - 1);
	start = (char *)mmap(NULL, nbytes + psz * 3, PROT_READ|PROT_WRITE,
		MAP_PRIVATE|MAP_ANON, -1, 0);
	if (start == NULL) {
		perror("Warning: protmalloc: mmap");
		return (NULL);
	}
	/* Protect the first and last red-zone pages to catch pointer errors */
	start = (char *)((int)(start + psz - 1) & ~(psz - 1));
	if (mprotect(start, psz, PROT_NONE) < 0) {
		perror("Warning: mprotect");
		return (NULL);
	}
	start += psz;
	end = start + nbytes;
	if (mprotect(end, psz, PROT_NONE) < 0) {
		perror("Warning: mprotect");
		return (NULL);
	}
	return (start);
}

/*
 * resetperm - reduce security vulnerabilities by resetting
 * owner/group/permissions. Always attempt setuid() - if we have
 * permission to drop our privilege level, do so.
 */
void
resetperm(void)
{
	if (geteuid() == 0) {
		(void) setgid(GID_NOBODY);
		(void) setuid(UID_NOBODY);
	}
}

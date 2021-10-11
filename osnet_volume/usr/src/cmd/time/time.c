/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)time.c	1.12	97/01/28 SMI"	/* SVr4.0 1.6	*/
/*
**	Time a command
*/

#include	<stdio.h>
#include	<signal.h>
#include	<errno.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<libintl.h>
#include	<locale.h>
#include	<limits.h>
#include	<sys/types.h>
#include	<sys/times.h>
#include	<sys/wait.h>

/*
 * The following use of HZ/10 will work correctly only if HZ is a multiple
 * of 10.  However the only values for HZ now in use are 100 for the 3B
 * and 60 for other machines.

 * The first value was HZ/10. Since HZ should be gotten from sysconf()
 * it is dynamically initialized at entry to the main program.
 */
static clock_t quant[] = { 10, 10, 10, 6, 10, 6, 10, 10, 10 };
static char *pad  = "000      ";
static char *sep  = "\0\0.\0:\0:\0\0";
static char *nsep = "\0\0.\0 \0 \0\0";

static void usage(void);
static void printt(char *, clock_t);
extern char *sys_errlist[];

main(argc, argv)
char **argv;
{
	struct tms	buffer;
	register	pid_t p;
	extern		errno;
	int		status;
	int		pflag		= 0;
	int		c;
	int		clock_tick	= CLK_TCK;
	clock_t		before, after;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "p")) != EOF)
		switch (c) {
		case 'p':
			pflag++;
			break;
		case '?':
			usage();
		}

	argc -= optind;
	argv += optind;

	/*
	 * time(1) is only accurate to a tenth of a second.  We need to
	 * determine the number of clock ticks in a tenth of a second in
	 * order to later divide away what we don't care about.
	 */
	quant[0] = clock_tick/10;

	before = times(&buffer);
	if (argc < 1)
		usage();
	p = fork();
	if (p == (pid_t)-1) {
		perror("time");
		exit(2);
	}
	if (p == (pid_t)0) {
		(void) execvp(argv[0], &argv[0]);
		perror(argv[0]);
		if (errno == ENOENT)
			exit(127);
		else
			exit(126);
	}
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);
	while (wait(&status) != p);
	if ((status & 0377) != '\0')
		(void) fprintf(stderr, "time: %s\n",
		    gettext("command terminated abnormally."));
	after = times(&buffer);
	(void) fprintf(stderr, "\n");
	if (pflag)
		(void) fprintf(stderr, "real %.2f\nuser %.2f\nsys %.2f\n",
		    (double) (after-before)/clock_tick,
		    (double) buffer.tms_cutime/clock_tick,
		    (double) buffer.tms_cstime/clock_tick);
	else {
		printt("real", (after-before));
		printt("user", buffer.tms_cutime);
		printt("sys ", buffer.tms_cstime);
	}

	return ((status & 0xff00)
		? (status >> 8)
		: ((status & 0x00ff) ? ((status & ~WCOREFLG) | 0200) : 0));
}


static void
printt(s, a)
char *s;
clock_t a;
{
	register i;
	char	digit[9];
	char	c;
	int	nonzero;

	a /= quant[0];	/* Divide away the accuracy we don't care about */

	/*
	 * We now have the number of tenths of seconds elapsed in terms of
	 * ticks. Loop through to determine the actual digits.
	 */
	for (i = 1; i < 9; i++) {
		digit[i] = a % quant[i];
		a /= quant[i];
	}
	(void) fprintf(stderr, s);
	nonzero = 0;
	while (--i > 0) {
		c = (digit[i] != 0) ? digit[i]+'0' : (nonzero ? '0': pad[i]);
		if (c != '\0')
			(void) putc(c, stderr);
		nonzero |= digit[i];
		c = nonzero?sep[i]:nsep[i];
		if (c != '\0')
			(void) putc(c, stderr);
	}
	(void) fprintf(stderr, "\n");
}

static void
usage(void)
{
	(void) fprintf(stderr,
	    gettext("usage: time [-p] utility [argument...]\n"));
	exit(1);
}

/*
 *	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 */

#ident 	"@(#)last.c	1.11	99/08/16 SMI"	/* SVr4.0 1.3 */

/*
 *
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 */

/*
 * last
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <signal.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <utmpx.h>
#include <locale.h>

/*
 * NMAX, LMAX and HMAX are set to these values for now. They
 * should be much higher because of the max allowed limit in
 * utmpx.h
 */
#define	NMAX	8
#define	LMAX	12
#define	HMAX	(sizeof (((struct utmpx *)0)->ut_host))
#define	SECDAY	(24*60*60)

#define	lineq(a, b)	(strncmp(a, b, LMAX) == 0)
#define	nameq(a, b)	(strncmp(a, b, NMAX) == 0)
#define	hosteq(a, b)	(strncmp(a, b, HMAX) == 0)

#define	MAXTTYS 256
#define	USAGE 	"usage: last [-n number] [-f filename] [-a ] [name | tty]\n"

static char	**argv;
static int	argc;
static int	nameargs;

static struct	utmpx buf[128];
static char	ttnames[MAXTTYS][LMAX+1];
static time_t	logouts[MAXTTYS];
static char	timef[128];
static char	hostf[HMAX + 1];

static char *strspl(char *, char *);
static void onintr(int);
static int want(struct utmpx *);
static void record_time(time_t *, int *, int, struct utmpx *);

int
main(int ac, char **av)
{
	int i;
	int aflag = 0;
	int fpos;	/* current position in time format buffer */
	int chrcnt;	/* # of chars formatted by current sprintf */
	int bl, wtmp;
	char *ct;
	struct utmpx *bp;
	time_t otime;
	struct stat stb;
	int print = 0;
	char *crmsg = (char *)0;
	long outrec = 0;
	long maxrec = 0x7fffffffL;
	char *wtmpfile = "/var/adm/wtmpx";

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)		/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"		/* Use this only if it weren't. */
#endif
	(void) textdomain(TEXT_DOMAIN);

	(void) time(&buf[0].ut_xtime);
	ac--, av++;
	nameargs = argc = ac;
	argv = av;
	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '-') {
			/* -[0-9]*   sets max # records to print */
			if (argv[i][1] >= '0' && argv[i][1] <= '9') {
				maxrec = atoi(argv[i]+1);
				nameargs--;
				continue;
			}
			/* -f name   sets filename of wtmp file */
			else if (argv[i][1] == 'f' && i < argc) {
				wtmpfile = argv[++i];
				nameargs -= 2;
				continue;
			}
			/* -n number sets max # records to print */
			else if (argv[i][1] == 'n' && i < argc) {
				if (! argv[++i] || *argv[i] < '0' ||
				    *argv[i] > '9') {
					(void) fprintf(stderr,
					    gettext("last: argument to -n "
						"is not a number\n"));
					(void) fprintf(stderr, gettext(USAGE));
					exit(1);
				}
				maxrec = atoi(argv[i]);
				nameargs -= 2;
				continue;
			}
			/* -a displays hostname last on the line */
			else if (argv[i][1] == 'a' && i < argc) {
				aflag++;
				nameargs--;
			} else {
				(void) fprintf(stderr, gettext(USAGE));
				exit(1);
			}
		}

		if (strlen(argv[i]) > 2)
			continue;
		if (strcmp(argv[i], "~") == 0)
			continue;
		if (strcmp(argv[i], "ftp") == 0)
			continue;
		if (strcmp(argv[i], "uucp") == 0)
			continue;
		if (getpwnam(argv[i]))
			continue;
		argv[i] = strspl("tty", argv[i]);
	}
	wtmp = open(wtmpfile, 0);
	if (wtmp < 0) {
		perror(wtmpfile);
		exit(1);
	}
	(void) fstat(wtmp, &stb);
	bl = (stb.st_size + sizeof (buf)-1) / sizeof (buf);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
		(void) signal(SIGINT, onintr);
		(void) signal(SIGQUIT, onintr);
	}
	for (bl--; bl >= 0; bl--) {
		(void) lseek(wtmp, (off_t)(bl * sizeof (buf)), 0);
		bp = &buf[read(wtmp, buf, sizeof (buf)) / sizeof (buf[0]) - 1];
		for (; bp >= buf; bp--) {
			if (want(bp)) {
				for (i = 0; i < MAXTTYS; i++) {
					if (ttnames[i][0] == 0) {
						(void) strncpy(ttnames[i],
						    bp->ut_line,
						    sizeof (bp->ut_line));
						record_time(&otime,
						    &print, i, bp);
						break;
					} else if (lineq(ttnames[i],
					    bp->ut_line)) {
						record_time(&otime,
						    &print, i, bp);
						break;
					}
				}
			}
			if (print) {
				(void) bzero(&timef, sizeof (timef));
				(void) bzero(&hostf, sizeof (hostf));
				ct = ctime(&bp->ut_xtime);
				(void) printf(gettext("%-*.*s  %-*.*s "),
				    NMAX, NMAX, bp->ut_name,
				    LMAX, LMAX, bp->ut_line);
				sprintf(hostf, "%-*.*s", strlen(bp->ut_host),
				    strlen(bp->ut_host), bp->ut_host);
				fpos = sprintf(timef, "%10.10s %5.5s ",
				    ct, 11 + ct);
				
				if (lineq(bp->ut_line, "system boot")) {
					/* do nothing */
				} else if (otime == 0 &&
				    bp->ut_type == USER_PROCESS) {
					chrcnt = sprintf(timef + fpos,
					    gettext("  still logged in"));
					fpos += chrcnt;
				} else {
					time_t delta;
					if (otime < 0) {
						otime = -otime;
						/*
						 * TRANSLATION_NOTE
						 * See other notes on "down"
						 * and "- %5.5s".
						 * "-" means "until".  This
						 * is displayed after the
						 * starting time as in:
						 * 	16:20 - down
						 * You probably don't want to
						 * translate this.  Should you
						 * decide to translate this,
						 * translate "- %5.5s" too.
						 */
						chrcnt = sprintf(timef + fpos,
						    gettext("- %s"), crmsg);
						fpos += chrcnt;
					} else {
						chrcnt = sprintf(timef + fpos,
						    gettext("- %5.5s"),
						    ctime(&otime) + 11);
						fpos += chrcnt;
					}
					delta = otime - bp->ut_xtime;
					if (delta < SECDAY) {
						chrcnt = sprintf(timef + fpos,
						    gettext("  (%5.5s)"),
						    asctime(gmtime(
							&delta)) + 11);
						fpos += chrcnt;
					} else {
						chrcnt = sprintf(timef + fpos,
						    gettext(" (%ld+%5.5s)"),
						    delta / SECDAY,
						    asctime(gmtime(
							&delta)) + 11);
						fpos += chrcnt;
					}
				}
				if (aflag)
					(void) printf("%-35.35s %-.*s\n",
					    timef, strlen(hostf), hostf);
				else
					(void) printf("%-16.16s %-.35s\n",
					    hostf, timef);
			
				(void) fflush(stdout);
				if (++outrec >= maxrec)
					exit(0);
			}
			/*
			 * when the system is down or crashed.
			 */
			if (bp->ut_type == BOOT_TIME) {
				for (i = 0; i < MAXTTYS; i++)
					logouts[i] = -bp->ut_xtime;
				/*
				 * TRANSLATION_NOTE
				 * Translation of this "down " will replace
				 * the %s in "- %s".  "down" is used instead
				 * of the real time session was ended, probably
				 * because the session ended by a sudden crash.
				 */
				crmsg = gettext("down ");
			}
			print = 0;	/* reset the print flag */
		}
	}
	ct = ctime(&buf[0].ut_xtime);
	(void) printf(gettext("\nwtmp begins %10.10s %5.5s \n"), ct, ct + 11);
	return (0);
}

static void
onintr(int signo)
{
	char *ct;

	if (signo == SIGQUIT)
		(void) signal(SIGQUIT, (void(*)())onintr);
	ct = ctime(&buf[0].ut_xtime);
	(void) printf(gettext("\ninterrupted %10.10s %5.5s \n"), ct, ct + 11);
	(void) fflush(stdout);
	if (signo == SIGINT)
		exit(1);
}

static int
want(struct utmpx *bp)
{
	char **av;
	int ac;

	if (bp->ut_type == BOOT_TIME)
		(void) strcpy(bp->ut_user, "reboot");
	if (strncmp(bp->ut_line, "ftp", 3) == 0)
		bp->ut_line[3] = '\0';
	if (strncmp(bp->ut_line, "uucp", 4) == 0)
		bp->ut_line[4] = '\0';

	if (bp->ut_type != USER_PROCESS && bp->ut_type != DEAD_PROCESS &&
	    bp->ut_type != BOOT_TIME)
		return (0);

	if (bp->ut_user[0] == '.')
		return (0);

	if (nameargs == 0) {
		if (bp->ut_line[0] != '\0')
			return (1);
	} else {
		if (lineq(bp->ut_line, "ftp"))
			return (0);
		av = argv;
		for (ac = 0; ac < argc; ac++, av++) {
			if (av[0][0] == '-')
				continue;
			if (nameq(*av, bp->ut_name) ||
			    lineq(*av, bp->ut_line)) {
				return (1);
			}
		}
	}
	return (0);
}

static char *
strspl(char *left, char *right)
{
	char *res = malloc((size_t)(strlen(left) + strlen(right) + 1));

	(void) strcpy(res, left);
	(void) strcat(res, right);
	return (res);
}

static void
record_time(time_t *otime, int *print, int i, struct utmpx *bp)
{
	*otime = logouts[i];
	logouts[i] = bp->ut_xtime;
	if ((bp->ut_type == USER_PROCESS && bp->ut_user[0] != '\0') ||
	    bp->ut_type == BOOT_TIME)
		*print = 1;
	else
		*print = 0;
}

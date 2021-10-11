/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)nohup.c	1.13	97/01/28 SMI"	/* SVr4.0 1.5	*/

#include <stdio.h>
#include <stdlib.h>
#include <nl_types.h>
#include <locale.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

static void usage();

main(argc, argv)
char **argv;
{
	char	*home;
	FILE	*temp;
	char	nout[PATH_MAX] = "nohup.out";

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	if (getopt(argc, argv, "") != EOF) {	/* no options to parse */
		usage();
		return (127);
	}

	if (optind == argc) {			/* at least one, a cmd */
		usage();
		return (127);
	}

	argv[argc] = 0;
	(void) signal(SIGHUP, SIG_IGN);		/* POSIX.2 only SIGHUP */
#if !defined(XPG4)
	(void) signal(SIGQUIT, SIG_IGN);	/* Solaris compatibility */
#endif /* !XPG4 */
	if (isatty(1)) {
		if ((temp = fopen(nout, "a")) == NULL) {
			if ((home = getenv("HOME")) == NULL) {
				(void) fprintf(stderr, gettext(
				"nohup: cannot open/create nohup.out\n"));
				return (127);
			}
			(void) strcpy(nout, home);
			(void) strcat(nout, "/nohup.out");
			if (freopen(nout, "a", stdout) == NULL) {
				(void) fprintf(stderr, gettext(
				"nohup: cannot open/create nohup.out\n"));
				return (127);
			}
		} else {
			(void) fclose(temp);
			(void) freopen(nout, "a", stdout);
		}
#ifdef XPG4
		(void) chmod(nout, S_IRUSR | S_IWUSR);
#else /*CSTYLED*/
		;	/* Solaris compatibility, leave -rw-rw-rw- */
#endif /* XPG4 */
		(void) fprintf(stderr, gettext("Sending output to %s\n"), nout);
	}
	if (isatty(2)) {
		(void) close(2);
		(void) dup(1);
	}
	(void) execvp(argv[optind], argv + optind);

	/* It failed, so print an error */
	(void) freopen("/dev/tty", "w", stderr);
	(void) fprintf(stderr,
		"%s: %s: %s\n", argv[0], argv[1], strerror(errno));
	/*
	 * POSIX.2 exit status:
	 * 127 if utility is not found.
	 * 126 if utility cannot be invoked.
	 */
	return (errno == ENOENT ? 127 : 126);
}

static void
usage()
{
	(void) fprintf(stderr,
	    gettext("usage: nohup command [argument ...]\n"));
}

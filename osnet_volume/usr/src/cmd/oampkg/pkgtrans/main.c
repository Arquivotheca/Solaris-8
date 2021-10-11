/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)main.c	1.16	93/05/28 SMI"	/* SVr4.0  1.4.3.1	*/

#include <locale.h>
#include <libintl.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pkgtrans.h>
#include <pkglib.h>
#include "libadm.h"
#include "libinst.h"

static int	options;
static void	usage(void), quit(int retcode), trap(int signo);

main(int argc, char *argv[])
{
	int	c;
	void	(*func)();
	extern char	*optarg;
	extern int	optind;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	(void) set_prog_name(argv[0]);

	while ((c = getopt(argc, argv, "snio?")) != EOF) {
		switch (c) {
		    case 'n':
			options |= PT_RENAME;
			break;

		    case 'i':
			options |= PT_INFO_ONLY;
			break;

		    case 'o':
			options |= PT_OVERWRITE;
			break;

		    case 's':
			options |= PT_ODTSTREAM;
			break;

		    default:
			usage();
		}
	}
	func = signal(SIGINT, trap);
	if (func != SIG_DFL)
		(void) signal(SIGINT, func);
	(void) signal(SIGHUP, trap);
	(void) signal(SIGQUIT, trap);
	(void) signal(SIGTERM, trap);
	(void) signal(SIGPIPE, trap);
#ifndef SUNOS41
	(void) signal(SIGPWR, trap);
#endif

	if ((argc-optind) < 2)
		usage();

	quit(pkgtrans(flex_device(argv[optind], 1),
	    flex_device(argv[optind+1], 1), &argv[optind+2], options));
	/*NOTREACHED*/
#ifdef lint
	return (0);
#endif	/* lint */
}

static void
quit(int retcode)
{
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGHUP, SIG_IGN);
	(void) ds_close(1);
	exit(retcode);
}

static void
trap(int signo)
{
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGHUP, SIG_IGN);

	if (signo == SIGINT) {
		progerr(gettext("aborted at user request.\n"));
		quit(3);
	}
	progerr(gettext("aborted by signal %d\n"), signo);
	quit(1);
}

static void
usage(void)
{
	(void) fprintf(stderr,
		gettext("usage: %s [-ions] srcdev dstdev [pkg [pkg...]]\n"),
		get_prog_name());
	exit(1);
}

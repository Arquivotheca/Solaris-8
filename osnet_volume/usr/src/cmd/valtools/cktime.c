/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)cktime.c	1.6	99/06/04 SMI"	/* SVr4.0 1.2.1.5 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <locale.h>
#include <libintl.h>
#include <limits.h>
#include "usage.h"
#include "libadm.h"
#include "pkglib.h"

#define	BADPID	(-2)

static char	*prog;
static char	*deflt = NULL, *prompt = NULL, *error = NULL, *help = NULL;
static int	kpid = BADPID;
static int	signo;
static char	*fmt;

static const char	vusage[] = "f";
static const char	husage[] = "fWh";
static const char	eusage[] = "fWe";

#define	MYFMT \
	"%s:ERROR:invalid format\n" \
	"valid format descriptors are:\n" \
	"\t%%H  #hour (00-23)\n" \
	"\t%%I  #hour (00-12)\n" \
	"\t%%M  #minute (00-59)\n" \
	"\t%%p  #AM, PM, am or pm\n" \
	"\t%%r  #time as %%I:%%M:%%S %%p\n" \
	"\t%%R  #time as %%H:%%M (default)\n" \
	"\t%%S  #seconds (00-59)\n" \
	"\t%%T  #time as %%H:%%M:%%S\n"

static void
usage(void)
{
	switch (*prog) {
	default:
		(void) fprintf(stderr,
			gettext("usage: %s [options] [-f format]\n"),
			prog);
		(void) fprintf(stderr, gettext(OPTMESG));
		(void) fprintf(stderr, gettext(STDOPTS));
		break;

	case 'v':
		(void) fprintf(stderr,
			gettext("usage: %s [-f format] input\n"), prog);
		break;

	case 'h':
		(void) fprintf(stderr,
			gettext("usage: %s [options] [-f format]\n"),
			prog);
		(void) fprintf(stderr, gettext(OPTMESG));
		(void) fprintf(stderr,
			gettext("\t-W width\n\t-h help\n"));
		break;

	case 'e':
		(void) fprintf(stderr,
			gettext("usage: %s [options] [-f format]\n"),
			prog);
		(void) fprintf(stderr, gettext(OPTMESG));
		(void) fprintf(stderr,
			gettext("\t-W width\n\t-e error\n"));
		break;
	}
	exit(1);
}

void
main(int argc, char **argv)
{
	int c, n;
	char *tod;
	size_t	len;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	prog = set_prog_name(argv[0]);

	while ((c = getopt(argc, argv, "f:d:p:e:h:k:s:QW:?")) != EOF) {
		/* check for invalid option */
		if ((*prog == 'v') && !strchr(vusage, c))
			usage();
		if ((*prog == 'e') && !strchr(eusage, c))
			usage();
		if ((*prog == 'h') && !strchr(husage, c))
			usage();

		switch (c) {
		case 'Q':
			ckquit = 0;
			break;

		case 'W':
			ckwidth = atoi(optarg);
			if (ckwidth < 0) {
				progerr(gettext(
					"negative display width specified"));
				exit(1);
			}
			break;

		case 'f':
			fmt = optarg;
			break;

		case 'd':
			deflt = optarg;
			break;

		case 'p':
			prompt = optarg;
			break;

		case 'e':
			error = optarg;
			break;

		case 'h':
			help = optarg;
			break;

		case 'k':
			kpid = atoi(optarg);
			break;

		case 's':
			signo = atoi(optarg);
			break;

		default:
			usage();
		}
	}

	if (signo) {
		if (kpid == BADPID)
			usage();
	} else
		signo = SIGTERM;

	if (*prog == 'v') {
		if (argc != (optind+1))
			usage();
		n = cktime_val(fmt, argv[optind]);
		/*
		 * TRANSLATION_NOTE
		 * In the below, "AM", "PM", "am", and "pm" are
		 * keywords.  So, do not translate them.
		 */
		if (n == 4)
			(void) fprintf(stderr, gettext(MYFMT), prog);
		exit(n);
	}

	if (optind != argc)
		usage();

	if (*prog == 'e') {
		ckindent = 0;
		if (cktime_err(fmt, error)) {
			(void) fprintf(stderr, gettext(MYFMT), prog);
			exit(4);
		} else
			exit(0);
	} else if (*prog == 'h') {
		ckindent = 0;
		if (cktime_hlp(fmt, help)) {
			(void) fprintf(stderr, gettext(MYFMT), prog);
			exit(4);
		} else
			exit(0);
	}

	if (deflt) {
		len = strlen(deflt) + 1;
		if (len < MAX_INPUT)
			len = MAX_INPUT;
	} else {
		len = MAX_INPUT;
	}
	tod = (char *)malloc(len);
	if (!tod) {
		(void) fprintf(stderr,
			gettext("Not enough memory\n"));
		exit(1);
	}
	n = cktime(tod, fmt, deflt, error, help, prompt);
	if (n == 3) {
		if (kpid > -2)
			(void) kill(kpid, signo);
		(void) puts("q");
	} else if (n == 0)
		(void) fputs(tod, stdout);
	if (n == 4)
		(void) fprintf(stderr, gettext(MYFMT), prog);
	exit(n);
}

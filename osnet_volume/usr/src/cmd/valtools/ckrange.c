/*
 * Copyright (c) 1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)ckrange.c	1.7	99/06/04	SMI"

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <locale.h>
#include <libintl.h>
#include "usage.h"
#include "libadm.h"
#include "pkglib.h"

#define	BADPID	(-2)

static char	*prog;
static char	*deflt = NULL, *prompt = NULL, *error = NULL, *help = NULL;
static int	kpid = BADPID;
static int	signo;
static int	base = 10;
static char	*upper;
static char	*lower;

static const char	vusage[] = "bul";
static const char	husage[] = "bulWh";
static const char	eusage[] = "bulWe";

#define	USAGE	"[-l lower] [-u upper] [-b base]"

static void
usage(void)
{
	switch (*prog) {
	default:
		(void) fprintf(stderr,
			gettext("usage: %s [options] %s\n"),
			prog, USAGE);
		(void) fprintf(stderr, gettext(OPTMESG));
		(void) fprintf(stderr, gettext(STDOPTS));
		break;

	case 'v':
		(void) fprintf(stderr,
			gettext("usage: %s %s input\n"), prog, USAGE);
		break;

	case 'h':
		(void) fprintf(stderr,
			gettext("usage: %s [options] %s\n"),
			prog, USAGE);
		(void) fprintf(stderr, gettext(OPTMESG));
		(void) fprintf(stderr,
			gettext("\t-W width\n\t-h help\n"));
		break;

	case 'e':
		(void) fprintf(stderr,
			gettext("usage: %s [options] %s\n"),
			prog, USAGE);
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
	int	c, n;
	long	lvalue, uvalue, intval;
	char	*ptr = 0;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	prog = set_prog_name(argv[0]);

	while ((c = getopt(argc, argv, "l:u:b:d:p:e:h:k:s:QW:?")) != EOF) {
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
				progerr("negative display width specified");
				exit(1);
			}
			break;

		case 'b':
			base = atoi(optarg);
			if ((base < 2) || (base > 36)) {
				progerr("base must be between 2 and 36");
				exit(1);
			}
			break;

		case 'u':
			upper = optarg;
			break;

		case 'l':
			lower = optarg;
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

	if (upper) {
		uvalue = strtol(upper, &ptr, base);
		if (ptr == upper) {
			progerr("invalid upper value specification");
			exit(1);
		}
	} else
		uvalue = LONG_MAX;
	if (lower) {
		lvalue =  strtol(lower, &ptr, base);
		if (ptr == lower) {
			progerr("invalid lower value specification");
			exit(1);
		}
	} else
		lvalue = LONG_MIN;

	if (uvalue < lvalue) {
		progerr("upper value is less than lower value");
		exit(1);
	}

	if (*prog == 'v') {
		if (argc != (optind+1))
			usage();
		exit(ckrange_val(lvalue, uvalue, base, argv[optind]));
	}

	if (optind != argc)
		usage();

	if (*prog == 'e') {
		ckindent = 0;
		ckrange_err(lvalue, uvalue, base, error);
		exit(0);
	} else if (*prog == 'h') {
		ckindent = 0;
		ckrange_hlp(lvalue, uvalue, base, help);
		exit(0);
	}

	n = ckrange(&intval, lvalue, uvalue, (short) base,
		deflt, error, help, prompt);	/* libadm interface */
	if (n == 3) {
		if (kpid > -2)
			(void) kill(kpid, signo);
		(void) puts("q");
	} else if (n == 0)
		(void) printf("%ld", intval);
	exit(n);
}

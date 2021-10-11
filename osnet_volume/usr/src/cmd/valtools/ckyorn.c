/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ckyorn.c	1.6	99/06/04 SMI"	/* SVr4.0 1.2.1.2 */

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
static char	*deflt, *prompt, *error, *help;
static int	kpid = BADPID;
static int	signo;

static const char	husage[] = "Wh";
static const char	eusage[] = "We";

static void
usage(void)
{
	switch (*prog) {
	default:
		(void) fprintf(stderr,
			gettext("usage: %s [options]\n"), prog);
		(void) fprintf(stderr, gettext(OPTMESG));
		(void) fprintf(stderr, gettext(STDOPTS));
		break;

	case 'v':
		(void) fprintf(stderr,
			gettext("usage: %s input\n"), prog);
		break;

	case 'h':
		(void) fprintf(stderr,
			gettext("usage: %s [options]\n"), prog);
		(void) fprintf(stderr, gettext(OPTMESG));
		(void) fprintf(stderr,
			gettext("\t-W width\n\t-h help\n"));
		break;

	case 'e':
		(void) fprintf(stderr,
			gettext("usage: %s [options]\n"), prog);
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
	char	*ynval;
	size_t	len;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	prog = set_prog_name(argv[0]);

	while ((c = getopt(argc, argv, "d:p:e:h:k:s:QW:?")) != EOF) {
		/* check for invalid option */
		if (*prog == 'v')
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
		if (argc != (optind + 1))
			usage();
		if (ckyorn_val(argv[optind]))
			exit(1);
		else
			exit(0);
	}

	if (optind != argc)
		usage();

	if (*prog == 'e') {
		ckindent = 0;
		ckyorn_err(error);
		exit(0);
	} else if (*prog == 'h') {
		ckindent = 0;
		ckyorn_hlp(help);
		exit(0);
	}

	if (deflt) {
		len = strlen(deflt) + 1;
		if (len < MAX_INPUT)
			len = MAX_INPUT;
	} else {
		len = MAX_INPUT;
	}
	ynval = (char *)malloc(len);
	if (!ynval) {
		(void) fprintf(stderr,
			gettext("Not enough memory\n"));
		exit(1);
	}
	n = ckyorn(ynval, deflt, error, help, prompt);
	if (n == 3) {
		if (kpid > -2) {
			if (kill(kpid, signo)) {
				(void) fprintf(stderr, gettext(
				"Failed to send signal %d to process %d\n"),
				signo, kpid);
			}
		}
		(void) puts("q");
	} else if (n == 0)
		(void) fputs(ynval, stdout);
	exit(n);
}

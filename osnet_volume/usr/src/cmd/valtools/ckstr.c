/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ckstr.c	1.6	99/06/04 SMI"	/* SVr4.0 1.2.1.4 */

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
#define	MAXREGEXP	128

static char	*prog;
static char	*deflt = NULL, *prompt = NULL, *error = NULL, *help = NULL;
static int	kpid = BADPID;
static int	signo, length;

static const char	vusage[] = "rl";
static const char	husage[] = "rlWh";
static const char	eusage[] = "rlWe";

#define	USAGE	"[-l length] [[-r regexp] [...]]"

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
	int c, n;
	char	*strval;
	char	**regexp;
	size_t	len;
	size_t	maxregexp = MAXREGEXP;
	size_t	nregexp = 0;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	prog = set_prog_name(argv[0]);

	regexp = (char **)calloc(maxregexp, sizeof (char *));
	if (!regexp) {
		(void) fprintf(stderr,
			gettext("Not enough memory\n"));
		exit(1);
	}
	while ((c = getopt(argc, argv, "r:l:d:p:e:h:k:s:QW:?")) != EOF) {
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

		case 'r':
			regexp[nregexp++] = optarg;
			if (nregexp == maxregexp) {
				maxregexp += MAXREGEXP;
				regexp = (char **)realloc(regexp,
					maxregexp * sizeof (char *));
				if (!regexp) {
					(void) fprintf(stderr,
						gettext("Not enough memory\n"));
					exit(1);
				}
				(void) memset(regexp + nregexp, 0,
					(maxregexp - nregexp) *
					sizeof (char *));
			}
			break;

		case 'l':
			length = atoi(optarg);
			if ((length <= 0) || (length > 128)) {
				progerr("length must be between 1 and 128");
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
		if (argc != (optind+1))
			usage();
		if (ckstr_val(regexp, length, argv[optind]))
			exit(1);
		exit(0);
	}

	if (*prog == 'e') {
		if (argc > (optind+1))
			usage(); /* too many args */
		ckindent = 0;
		ckstr_err(regexp, length, error, argv[optind]);
		exit(0);
	}

	if (optind != argc)
		usage();

	if (*prog == 'h') {
		ckindent = 0;
		ckstr_hlp(regexp, length, help);
		exit(0);
	}

	regexp[nregexp] = NULL;

	if (deflt) {
		len = strlen(deflt) + 1;
		if (len < MAX_INPUT)
			len = MAX_INPUT;
	} else {
		len = MAX_INPUT;
	}
	strval = (char *)malloc(len);
	if (!strval) {
		(void) fprintf(stderr,
			gettext("Not enough memory\n"));
		exit(1);
	}
	n = ckstr(strval, regexp, length, deflt, error, help, prompt);
	if (n == 3) {
		if (kpid > -2)
			(void) kill(kpid, signo);
		(void) puts("q");
	} else if (n == 0)
		(void) fputs(strval, stdout);
	exit(n);
}

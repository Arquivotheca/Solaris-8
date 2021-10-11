/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ckgid.c	1.6	99/06/04 SMI"	/* SVr4.0 1.2.1.4 */

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
static char	*deflt = NULL, *error = NULL, *help = NULL, *prompt = NULL;
static int	kpid = BADPID;
static int	signo, disp;

static const char	eusage[] = "mWe";
static const char	husage[] = "mWh";

static void
usage(void)
{
	switch (*prog) {
	default:
		(void) fprintf(stderr,
			gettext("usage: %s [options] [-m]\n"), prog);
		(void) fprintf(stderr, gettext(OPTMESG));
		(void) fprintf(stderr, gettext(STDOPTS));
		break;

	case 'd':
		(void) fprintf(stderr,
			gettext("usage: %s\n"), prog);
		break;

	case 'v':
		(void) fprintf(stderr,
			gettext("usage: %s input\n"), prog);
		break;

	case 'h':
		(void) fprintf(stderr,
			gettext("usage: %s [options] [-m]\n"), prog);
		(void) fprintf(stderr, gettext(OPTMESG));
		(void) fprintf(stderr,
			gettext("\t-W width\n\t-h help\n"));
		break;

	case 'e':
		(void) fprintf(stderr,
			gettext("usage: %s [options] [-m]\n"), prog);
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
	char	*gid;
	size_t	len;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	prog = set_prog_name(argv[0]);

	while ((c = getopt(argc, argv, "md:p:e:h:k:s:QW:?")) != EOF) {
		/* check for invalid option */
		if ((*prog == 'v') || (*prog == 'd'))
			usage(); /* no valid options */
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

		case 'm':
			disp = 1;
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
		exit(ckgid_val(argv[optind]));
	}

	if (argc != optind)
		usage();


	if (*prog == 'e') {
		ckindent = 0;
		ckgid_err(disp, error);
		exit(0);
	} else if (*prog == 'h') {
		ckindent = 0;
		ckgid_hlp(disp, help);
		exit(0);
	} else if (*prog == 'd') {
		exit(ckgid_dsp());
	}

	if (deflt) {
		len = strlen(deflt) + 1;
		if (len < MAX_INPUT)
			len = MAX_INPUT;
	} else {
		len = MAX_INPUT;
	}
	gid = (char *)malloc(len);
	if (!gid) {
		(void) fprintf(stderr,
			gettext("Not enough memory\n"));
		exit(1);
	}
	n = ckgid(gid, disp, deflt, error, help, prompt);
	if (n == 3) {
		if (kpid > -2)
			(void) kill(kpid, signo);
		(void) puts("q");
	} else if (n == 0)
		(void) fputs(gid, stdout);
	exit(n);
}

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ckkeywd.c	1.6	99/06/04 SMI"	/* SVr4.0 1.2.1.2 */

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

static char	**keyword;
static int	nkeyword = 0;

static void
usage(void)
{
	(void) fprintf(stderr,
		gettext("usage: %s [options] keyword [...]\n"), prog);
	(void) fprintf(stderr, gettext(OPTMESG));
	(void) fprintf(stderr, gettext(STDOPTS));
	exit(1);
}

void
main(int argc, char **argv)
{
	int	c, n;
	int	i;
	char	*strval;
	size_t	len;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	prog = set_prog_name(argv[0]);


	while ((c = getopt(argc, argv, "d:p:e:h:k:s:QW:?")) != EOF) {
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

	if (optind >= argc)
		usage(); /* must be at least one keyword */

	nkeyword = argc - optind;
	keyword = (char **)malloc(sizeof (char *) * (nkeyword + 1));
	if (!keyword) {
		(void) fprintf(stderr,
			gettext("Not enough memory\n"));
		exit(1);
	}
	for (i = 0; i < nkeyword; i++)
		keyword[i] = argv[optind++];
	keyword[nkeyword] = NULL;

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
	n = ckkeywd(strval, keyword, deflt, error, help, prompt);
	if (n == 3) {
		if (kpid > -2)
			(void) kill(kpid, signo);
		(void) puts("q");
	} else if (n == 0)
		(void) fputs(strval, stdout);
	exit(n);
}

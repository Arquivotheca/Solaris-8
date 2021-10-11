/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)puttext.c	1.6	99/06/04 SMI"	/* SVr4.0 1.1 */

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <libintl.h>
#include "libadm.h"
#include "pkglib.h"

static char	*prog;
static int	nflag;
static int	lmarg, rmarg;

static void
usage(void)
{
	(void) fprintf(stderr,
		gettext("usage: %s [-r rmarg] [-l lmarg] string\n"),
		prog);
	exit(1);
}

void
main(int argc, char **argv)
{
	int c;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	prog = set_prog_name(argv[0]);

	while ((c = getopt(argc, argv, "nr:l:?")) != EOF) {
		switch (c) {
		case 'n':
			nflag++;
			break;

		case 'r':
			rmarg = atoi(optarg);
			break;

		case 'l':
			lmarg = atoi(optarg);
			break;

		default:
			usage();
		}
	}

	if ((optind + 1) != argc)
		usage();

	(void) puttext(stdout, argv[optind], lmarg, rmarg);
	if (!nflag)
		(void) fputc('\n', stdout);
	exit(0);
}

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ckpath.c	1.6	99/06/04 SMI"	/* SVr4.0 1.3.1.2 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <valtools.h>
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
static int	signo, pflags;

static const char	vusage[] = "abcfglrtwxyzno";
static const char	eusage[] = "abcfglrtwxyznoWe";
static const char	husage[] = "abcfglrtwxyznoWh";

#define	USAGE "[-[a|l][b|c|f|y][n|[o|z]]rtwx]"
#define	MYOPTS	\
	"\t-a  #absolute path\n" \
	"\t-b  #block special device\n" \
	"\t-c  #character special device\n" \
	"\t-f  #ordinary file\n" \
	"\t-l  #relative path\n" \
	"\t-n  #must not exist (new)\n" \
	"\t-o  #must exist (old)\n" \
	"\t-r  #read permission\n" \
	"\t-t  #permission to create (touch)\n" \
	"\t-w  #write permission\n" \
	"\t-x  #execute permisiion\n" \
	"\t-y  #directory\n" \
	"\t-z  #non-zero length\n"

static void
usage(void)
{
	switch (*prog) {
	default:
		(void) fprintf(stderr,
			gettext("usage: %s [options] %s\n"),
			prog, USAGE);
		(void) fprintf(stderr, gettext(MYOPTS));
		(void) fprintf(stderr, gettext(OPTMESG));
		(void) fprintf(stderr, gettext(STDOPTS));
		break;

	case 'v':
		(void) fprintf(stderr,
			gettext("usage: %s %s input\n"),
			prog, USAGE);
		(void) fprintf(stderr, gettext(OPTMESG));
		(void) fprintf(stderr, gettext(MYOPTS));
		break;

	case 'h':
		(void) fprintf(stderr,
			gettext("usage: %s [options] %s\n"),
			prog, USAGE);
		(void) fprintf(stderr, gettext(MYOPTS));
		(void) fprintf(stderr, gettext(OPTMESG));
		(void) fprintf(stderr,
			gettext("\t-W width\n\t-h help\n"));
		break;

	case 'e':
		(void) fprintf(stderr,
			gettext("usage: %s [options] %s [input]\n"),
			prog, USAGE);
		(void) fprintf(stderr, gettext(MYOPTS));
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
	char *pathval;
	size_t	len;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	prog = set_prog_name(argv[0]);

	while ((c = getopt(argc, argv, "abcfglrtwxyznod:p:e:h:k:s:QW:?"))
		!= EOF) {
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

		case 'a':
			pflags |= P_ABSOLUTE;
			break;

		case 'b':
			pflags |= P_BLK;
			break;

		case 'c':
			pflags |= P_CHR;
			break;

		case 'f':
		case 'g': /* outdated */
			pflags |= P_REG;
			break;

		case 'l':
			pflags |= P_RELATIVE;
			break;

		case 'n':
			pflags |= P_NEXIST;
			break;

		case 'o':
			pflags |= P_EXIST;
			break;

		case 't':
			pflags |= P_CREAT;
			break;

		case 'r':
			pflags |= P_READ;
			break;

		case 'w':
			pflags |= P_WRITE;
			break;

		case 'x':
			pflags |= P_EXEC;
			break;

		case 'y':
			pflags |= P_DIR;
			break;

		case 'z':
			pflags |= P_NONZERO;
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

	if (ckpath_stx(pflags)) {
		(void) fprintf(stderr, gettext(
			"ERROR: mutually exclusive options used\n"));
		exit(4);
	}

	if (*prog == 'v') {
		if (argc != (optind+1))
			usage(); /* too many paths listed */
		exit(ckpath_val(argv[optind], pflags));
	} else if (*prog == 'e') {
		if (argc > (optind+1))
			usage();
		ckindent = 0;
		ckpath_err(pflags, error, argv[optind]);
		exit(0);
	}

	if (optind != argc)
		usage();

	if (*prog == 'h') {
		ckindent = 0;
		ckpath_hlp(pflags, help);
		exit(0);
	}

	if (deflt) {
		len = strlen(deflt) + 1;
		if (len < MAX_INPUT)
			len = MAX_INPUT;
	} else {
		len = MAX_INPUT;
	}
	pathval = (char *)malloc(len);
	if (!pathval) {
		(void) fprintf(stderr,
			gettext("Not enough memory\n"));
		exit(1);
	}
	n = ckpath(pathval, pflags, deflt, error, help, prompt);
	if (n == 3) {
		if (kpid > -2)
			(void) kill(kpid, signo);
		(void) puts("q");
	} else if (n == 0)
		(void) fputs(pathval, stdout);
	exit(n);
}

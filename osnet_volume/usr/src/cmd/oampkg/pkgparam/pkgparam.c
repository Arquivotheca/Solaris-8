/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)pkgparam.c	1.18	93/12/20 SMI"	/* SVr4.0  1.7.1.1	*/

/*  6-3-92	added newroot function */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pkgstrct.h>
#include <pkginfo.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>
#include "libadm.h"
#include "libinst.h"

extern char	*pkgfile;

#define	ERR_ROOT_SET	"Could not set install root from the environment."
#define	ERR_ROOT_CMD	"Command line install root contends with environment."
#define	ERR_MESG	"unable to locate parameter information for \"%s\""
#define	ERR_FLT		"parsing error in parameter file"
#define	ERR_USAGE	"usage:\n" \
			"\t%s [-v] [-d device] pkginst [param [param ...]]\n" \
			"\t%s [-v] -f file [param [param ...]]\n"

static char	*device = NULL;
static int	errflg = 0;
static int	vflag = 0;

static void
usage(void)
{
	char	*prog = get_prog_name();

	(void) fprintf(stderr, gettext(ERR_USAGE), prog, prog);
	exit(1);
}

main(int argc, char *argv[])
{
	char *ir = NULL;
	char *value, *pkginst;
	char *param, parambuf[128];
	int c;
	extern char	*optarg;
	extern int	optind;

	pkgfile = NULL;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	(void) set_prog_name(argv[0]);

	if (!set_inst_root(getenv("PKG_INSTALL_ROOT"))) {
		progerr(gettext(ERR_ROOT_SET));
		exit(1);
	}

	while ((c = getopt(argc, argv, "R:vd:f:?")) != EOF) {
		switch (c) {
		    case 'v':
			vflag++;
			break;

		    case 'f':
			/* -f could specify filename to get parameters from */
			pkgfile = optarg;
			break;

		    case 'd':
			/* -d could specify stream or mountable device */
			device = flex_device(optarg, 1);
			break;

		    case 'R':
			if (!set_inst_root(optarg)) {
				progerr(gettext(ERR_ROOT_CMD));
				exit(1);
			}
			break;

		    default:
		    case '?':
			usage();
		}
	}

	set_PKGpaths(get_inst_root());

	if (pkgfile) {
		if (device)
			usage();
		pkginst = pkgfile;
	} else {
		if ((optind+1) > argc)
			usage();

		if (pkghead(device))
			return (1); /* couldn't obtain info about device */
		pkginst = argv[optind++];
	}

	do {
		param = argv[optind];
		if (!param) {
			param = parambuf;
			*param = '\0';
		}
		value = pkgparam(pkginst, param);
		if (value == NULL) {
			if (errno == EFAULT) {
				progerr(gettext(ERR_FLT));
				errflg++;
				break;
			} else if (errno != EINVAL) {
				/*
				 * some other error besides no value for this
				 * particular parameter
				 */
				progerr(gettext(ERR_MESG), pkginst);
				errflg++;
				break;
			}
			if (!argv[optind])
				break;
			continue;
		}
		if (vflag) {
			(void) printf("%s='", param);
			while (*value) {
				if (*value == '\'') {
					(void) printf("'\"'\"'");
					value++;
				} else
					(void) putchar(*value++);
			}
			(void) printf("'\n");
		} else
			(void) printf("%s\n", value);

	} while (!argv[optind] || (++optind < argc));
	(void) pkgparam(NULL, NULL); /* close open FDs so umount won't fail */

	(void) pkghead(NULL);
	return (errflg ? 1 : 0);
}

quit(int retval)
{
	exit(retval);
	/*NOTREACHED*/
#ifdef lint
	return (0);
#endif	/* lint */
}

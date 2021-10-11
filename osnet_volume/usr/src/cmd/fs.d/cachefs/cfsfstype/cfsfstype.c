/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cfsfstype.c   1.6     96/06/03 SMI"

/*
 *
 *			cfsfstype.c
 *
 * Cache FS admin utility.  Used to glean information out of the
 * rootfs, frontfs, and backfs variables in the kernel.
 */

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <ftw.h>
#include <fcntl.h>
#include <ctype.h>
#include <varargs.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/filio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/fs/cachefs_fs.h>
#include <sys/fs/cachefs_dir.h>


void pr_err(char *fmt, ...);
void usage(char *);

/*
 *
 *			main
 *
 * Description:
 *	Main routine for the cfsfstype program.
 * Arguments:
 *	argc	number of command line arguments
 *	argv	command line arguments
 * Returns:
 *	Returns 0 for failure, > 0 for an error.
 * Preconditions:
 */

int
main(int argc, char **argv)
{
	int c;
	int which;

	char *path;

	int bflag;
	int fflag;
	int rflag;
	int dflag;
	int nflag;
	int cflag;
	struct statvfs64 svb;
	struct cachefs_cnvt_mnt cnvt;
	struct cachefs_boinfo cboi;
	char bovalue[MAXPATHLEN];
	int fd, err;

	/* verify root running command */
	if (getuid() != 0) {
		pr_err(gettext("must be run by root"));
		return (1);
	}

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	/* set defaults for command line options */
	bflag = fflag = rflag = dflag = nflag = cflag = 0;

	/* parse the command line arguments */
	while ((c = getopt(argc, argv, "bfrdnc")) != EOF) {
		switch (c) {

		case 'b':		/* back */
			bflag = 1;
			which = CFS_BOI_BACKFS;
			break;

		case 'f':		/* front */
			fflag = 1;
			which = CFS_BOI_FRONTFS;
			break;

		case 'r':		/* root */
			rflag = 1;
			which = CFS_BOI_ROOTFS;
			break;

		case 'd':		/* device */
			dflag = 1;
			break;

		case 'n':
			nflag = 1;
			break;

		case 'c':
			cflag = 1;
			break;

		default:
			usage(gettext("illegal option"));
			return (1);
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 1) {
		usage(gettext("too many file names specified"));
		return (1);
	}

	/* if a path is specified, just statvfs it */
	if ((argc == 1) && (! cflag) && (! nflag)) {
		if (bflag || fflag || rflag || dflag || nflag) {
			usage(gettext(
				"cannot specify options with a file name"));
			return (1);
		}
		if (statvfs64(*argv, &svb) < 0) {
			pr_err(gettext("Cannot open %s: %s"), *argv,
				strerror(errno));
			return (1);
		}
		(void) printf("%s\n", svb.f_basetype);
		return (0);
	}

	if ((bflag + fflag + rflag + nflag + cflag) != 1) {
		usage(gettext("must specify one of -b, -f, -r, -n or -c"));
		return (1);
	}
	if ((cflag) && (argc != 1)) {
		usage(gettext("need path with -c flag"));
		return (1);
	}

	fd = open("/", O_RDONLY);
	if (fd < 0) {
		perror(gettext("Open of root directory"));
		return (1);
	}

	if (nflag) {
		if (argc == 1) {
			close(fd);
			fd = open(*argv, O_RDONLY);
			if (fd < 0) {
				perror(gettext("open of specified directory"));
				return (1);
			}
		}
		err = ioctl(fd, _FIOSTOPCACHE);
	} else if (cflag) {
		cnvt.cm_op = CFS_CM_FRONT;
		cnvt.cm_namelen = strlen(*argv) + 1;
		cnvt.cm_name = *argv;
		err = ioctl(fd, _FIOCNVTMNT, &cnvt);
	} else {
		cboi.boi_which = which;
		cboi.boi_device = dflag;
		cboi.boi_len = sizeof (bovalue);
		cboi.boi_value = bovalue;
		err = ioctl(fd, _FIOBOINFO, &cboi);
	}
	if (err) {
		perror(gettext("Convert ioctl fault"));
		return (1);
	}
	if (! (nflag || cflag))
		(void) printf("%s\n", cboi.boi_value);
	return (0);
}

/*
 *
 *			usage
 *
 * Description:
 *	Prints a usage message for this utility.
 * Arguments:
 *	msgp	message to include with the usage message
 * Returns:
 * Preconditions:
 *	precond(msgp)
 */

void
usage(char *msgp)
{
	fprintf(stderr, gettext("cfsfstype: %s\n"), msgp);
	fprintf(stderr, gettext("usage: cfsfstype file\n"));
	fprintf(stderr, gettext("       cfsfstype [-d] -r\n"));
	fprintf(stderr, gettext("       cfsfstype [-d] -f\n"));
	fprintf(stderr, gettext("       cfsfstype [-d] -b\n"));
}

/*
 *
 *			pr_err
 *
 * Description:
 *	Prints an error message to stderr.
 * Arguments:
 *	fmt	printf style format
 *	...	arguments for fmt
 * Returns:
 * Preconditions:
 *	precond(fmt)
 */

void
pr_err(char *fmt, ...)
{
	va_list ap;

	va_start(ap);
	(void) fprintf(stderr, gettext("cfsfstype: "));
	(void) vfprintf(stderr, fmt, ap);
	(void) fprintf(stderr, "\n");
	va_end(ap);
}

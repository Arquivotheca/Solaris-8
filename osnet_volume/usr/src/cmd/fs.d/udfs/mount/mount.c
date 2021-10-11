/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mount.c	1.10	99/10/18 SMI"

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>	/* defines F_LOCK for lockf */
#include <stdlib.h>	/* for getopt(3) */
#include <signal.h>
#include <locale.h>
#include <fslib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mnttab.h>
#include <sys/mount.h>

#define	FSTYPE		"udfs"
#define	MNTOPT_DEV	"dev"
#define	NAME_MAX	64

static int roflag = 0;
static int rwflag = 0;
static int nosuidflag = 0;	/* don't allow setuid execution */
static int remount = 0;
static int mflag = 0;
static int Oflag = 0;
static int qflag = 0;

static char fstype[] = FSTYPE;

static char typename[NAME_MAX], *myname;
static char *myopts[] = {
	"ro",
	"rw",
	"nosuid",
	"remount",
	"m",
	NULL
};

#define	READONLY	0
#define	READWRITE	1
#define	NOSUID 		2
#define	REMOUNT		3
#define	MAKE		4


static void do_mount(char *, char *, int);
static void rpterr(char *, char *);
static void usage(void);

int
main(int argc, char **argv)
{
	char *options = NULL;
	char *value;
	char *special, *mountp;
	char *saveopt;
	int flags = 0;
	int sum_flags = 0, c;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	myname = strrchr(argv[0], '/');
	if (myname) {
		myname++;
	} else {
		myname = argv[0];
	}
	(void) sprintf(typename, "%s %s", fstype, myname);
	argv[0] = typename;

	/* check for proper arguments */

	while ((c = getopt(argc, argv, "mo:rOq")) != EOF) {
		switch (c) {
			case 'm':
				mflag++;
				break;
			case 'o':
				options = optarg;
				break;
			case 'r':
				roflag++;
				break;
			case 'O':
				Oflag++;
				break;
			case 'q':
				qflag = 1;
				break;
			default :
				break;
		}
	}

	if ((argc - optind) != 2)
		usage();

	special = argv[optind++];
	mountp = argv[optind++];

	while (options != NULL && *options != '\0') {
		saveopt = options;
		switch (getsubopt(&options, myopts, &value)) {
			case READONLY:
				roflag++;
				break;
			case READWRITE:
				rwflag++;
				break;
			case NOSUID:
				nosuidflag++;
				break;
			case REMOUNT:
				remount++;
				break;
			case MAKE:
				mflag++;
				break;
			default:
				if (!qflag) {
					(void) fprintf(stderr, gettext(
					    "mount: %s on %s - Warning "
					    "unknown options \"%s\"\n"),
					    special, mountp, saveopt);
				}
				break;
		}
	}

	sum_flags = roflag + rwflag;
	if (sum_flags > 1) {
		usage();
	}
	if (sum_flags == 0) {
		roflag = 0;
		rwflag = 1;
	}

	if (remount &&
		(!rwflag)) {
		usage();
	}

	if (geteuid() != 0) {
		(void) fprintf(stderr,
			gettext("%s: not super user\n"), myname);
		exit(31+2);
	}

	if (roflag) {
		flags = MS_RDONLY;
	}

	flags |= (nosuidflag ? MS_NOSUID : 0);
	flags |= (remount ? MS_REMOUNT : 0);
	flags |= (Oflag ? MS_OVERLAY : 0);
	flags |= (mflag ? MS_NOMNTTAB : 0);


	(void) signal(SIGHUP,  SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);
	(void) signal(SIGINT,  SIG_IGN);
	/*
	 *	Perform the mount.
	 *	Only the low-order bit of "roflag" is used by the system
	 *	calls (to denote read-only or read-write).
	 */
	do_mount(special, mountp, flags);
	return (0);
}


static void
rpterr(char *bs, char *mp)
{
	switch (errno) {
	case EPERM:
		(void) fprintf(stderr,
			gettext("%s: not super user\n"), myname);
		break;
	case ENXIO:
		(void) fprintf(stderr,
			gettext("%s: %s no such device\n"), myname, bs);
		break;
	case ENOTDIR:
		(void) fprintf(stderr,
			gettext("%s: %s not a directory\n\t"
				"or a component of %s is not a directory\n"),
		    myname, mp, bs);
		break;
	case ENOENT:
		(void) fprintf(stderr,
			gettext("%s: %s or %s, no such file or directory\n"),
			myname, bs, mp);
		break;
	case EINVAL:
		(void) fprintf(stderr,
			gettext("%s: %s is not an udfs file system.\n"),
			typename, bs);
		break;
	case EBUSY:
		(void) fprintf(stderr,
			gettext("%s: %s is already mounted, %s is busy,\n"),
			myname, bs, mp);
		(void) fprintf(stderr,
			gettext("\tor allowable number of "
				"mount points exceeded\n"));
		break;
	case ENOTBLK:
		(void) fprintf(stderr,
			gettext("%s: %s not a block device\n"), myname, bs);
		break;
	case EROFS:
		(void) fprintf(stderr,
			gettext("%s: %s write-protected\n"),
			myname, bs);
		break;
	case ENOSPC:
		(void) fprintf(stderr,
			gettext("%s: %s is corrupted. needs checking\n"),
			myname, bs);
		break;
	default:
		perror(myname);
		(void) fprintf(stderr,
			gettext("%s: cannot mount %s\n"), myname, bs);
	}
}


static void
do_mount(char *special, char *mountp, int flag)
{
	if (mount(special, mountp, flag | MS_DATA,
		fstype, NULL, 0) == -1) {
		rpterr(special, mountp);
		exit(31+2);
	}
}


static void
usage()
{
	(void) fprintf(stdout, gettext("udfs usage:\n"
			"mount [-F udfs] [generic options] "
			"[-o suboptions] {special | mount_point}\n"));
	(void) fprintf(stdout, gettext("\tsuboptions are: \n"
			"\t	ro,rw,nosuid,remount,m\n"));
	(void) fprintf(stdout, gettext(
			"\t	only one of ro, rw can be "
			"used at the same time\n"));
	(void) fprintf(stdout, gettext(
			"\t	remount can be used only with rw\n"));

	exit(32);
}

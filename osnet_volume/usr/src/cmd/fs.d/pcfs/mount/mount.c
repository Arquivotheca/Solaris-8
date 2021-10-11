#pragma ident	"@(#)mount.c	1.14	99/09/23 SMI"

/*
 * Copyright (c) 1988 Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <locale.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <sys/mount.h>
#include <sys/fs/pc_fs.h>
#include <fslib.h>

extern int	daylight;

extern int errno;

/*
 * The "hidden/nohidden" option is private. It is not approved for
 * use (see PSARC/1996/443), which is why it is not in the usage message.
 */
#define	RO		0
#define	RW		1
#define	HIDDEN		2
#define	NOHIDDEN	3
#define	FOLDCASE	4
#define	NOFOLDCASE	5

int roflag = 0;
static char *myopts[] = {
	MNTOPT_RO,
	MNTOPT_RW,
	MNTOPT_PCFS_HIDDEN,
	MNTOPT_PCFS_NOHIDDEN,
	MNTOPT_PCFS_FOLDCASE,
	MNTOPT_PCFS_NOFOLDCASE,
	NULL
};

struct pcfs_args tz;

main(int argc, char *argv[])
{
	char *mnt_special;
	char *mnt_mountp;
	int c;
	char *myname;
	char typename[64];
	char *options = NULL;
	char *value;
	extern int optind;
	extern char *optarg;
	int error = 0;
	int verbose = 0;
	int mflg = 0;
	int hidden = 0;
	int foldcase = 0;
	int qflg = 0;
	int optcnt = 0;

	myname = strrchr(argv[0], '/');
	myname = myname ? myname + 1 : argv[0];
	(void) sprintf(typename, "%s_%s", MNTTYPE_PCFS, myname);
	argv[0] = typename;

	while ((c = getopt(argc, argv, "Vvmr?o:Oq")) != EOF) {
		switch (c) {
		case 'V':
		case 'v':
			verbose++;
			break;
		case '?':
			error++;
			break;
		case 'r':
			roflag++;
			break;
		case 'm':
			mflg |= MS_NOMNTTAB;
			break;
		case 'o':
			options = optarg;
			break;
		case 'O':
			mflg |= MS_OVERLAY;
			break;
		case 'q':
			qflg = 1;
			break;
		}
	}

	if (verbose && !error) {
		char *optptr;

		(void) fprintf(stderr, "%s", typename);
		for (optcnt = 1; optcnt < argc; optcnt++) {
			optptr = argv[optcnt];
			if (optptr)
				(void) fprintf(stderr, " %s", optptr);
		}
		(void) fprintf(stderr, "\n");
	}

	while (options != NULL && *options != '\0') {
		switch (getsubopt(&options, myopts, &value)) {
		case RO:
			roflag++;
			break;
		case RW:
			break;
		case HIDDEN:
			hidden++;
			break;
		case NOHIDDEN:
			hidden = 0;
			break;
		case FOLDCASE:
			foldcase++;
			break;
		case NOFOLDCASE:
			foldcase = 0;
			break;
		default:
			if (!qflg)
				(void) fprintf(stderr,
			    	   gettext("%s: illegal -o suboption -- %s\n"),
		    		   typename, value);
			break;
		}
	}

	if (argc - optind != 2 || error) {
		/*
		 * don't hint at options yet (none are really supported)
		 */
		(void) fprintf(stderr, gettext(
		    "Usage: %s [generic options] [-o suboptions] "
		    "special mount_point\n"), typename);
		(void) fprintf(stderr, gettext(
		    "\tsuboptions are:\n"
		    "\t     ro,rw\n"
		    "\t     foldcase,nofoldcase\n"));
		exit(32);
	}

	mnt_special = argv[optind++];
	mnt_mountp = argv[optind++];

	(void) tzset();
	tz.secondswest = timezone;
	tz.dsttime = daylight;
	mflg |= MS_DATA;
	if (roflag)
		mflg |= MS_RDONLY;
	if (hidden)
		tz.flags |= PCFS_MNT_HIDDEN;
	else
		tz.flags &= ~PCFS_MNT_HIDDEN;
	if (foldcase)
		tz.flags |= PCFS_MNT_FOLDCASE;
	else
		tz.flags &= ~PCFS_MNT_FOLDCASE;

	signal(SIGHUP,  SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT,  SIG_IGN);

	if (verbose) {
		(void) fprintf(stderr, "mount(%s, \"%s\", %d, %s",
		    mnt_special, mnt_mountp, mflg, MNTTYPE_PCFS);
	}
	if (mount(mnt_special, mnt_mountp, mflg, MNTTYPE_PCFS,
	    (char *)&tz, sizeof (struct pcfs_args))) {
		if (errno == EBUSY) {
			fprintf(stderr, gettext(
			    "mount: %s is already mounted, %s is busy,\n\tor"
			    " allowable number of mount points exceeded\n"),
			    mnt_special, mnt_mountp);
		} else if (errno == EINVAL) {
			fprintf(stderr, gettext(
			    "mount: %s is not a DOS filesystem.\n"),
			    mnt_special);
		} else {
			perror("mount");
		}
		exit(32);
	}

	return (0);
}

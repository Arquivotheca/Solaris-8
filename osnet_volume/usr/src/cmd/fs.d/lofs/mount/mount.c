#pragma ident	"@(#)mount.c	1.13	99/09/23 SMI"

/*
 * Copyright (c) 1985 Sun Microsystems, Inc.
 */

#define	LOFS
#define	MNTTYPE_LOFS "lofs"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <libintl.h>
#include <sys/errno.h>
#include <sys/fstyp.h>
#include <sys/fsid.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <fslib.h>

#define	RET_OK		0
#define	RET_ERR		33

extern int errno;
extern int optind;
extern char *optarg;

void usage();
char *hasmntopt();


char *myopts[] = {
#define	RO 0
	"ro",
#define	RW 1
	"rw",
	NULL
};

char *myname;
char typename[64];
int  qflg = 0;

int errflag;

char fstype[] = MNTTYPE_LOFS;

/*
 * usage: mount -F lofs [-r] [-o options] fsname dir
 *
 * This mount program is exec'ed by /usr/sbin/mount if '-F lofs' is
 * specified.
 */
main(argc, argv)
	int argc;
	char **argv;
{
	int c;
	char *fsname;		/* Directory being mounted */
	char *dir;		/* Directory being mounted on */
	int flags = 0;
	char *options = NULL;
	char *value, *saveopt;

	myname = strrchr(argv[0], '/');
	myname = myname ? myname+1 : argv[0];
	(void) sprintf(typename, "%s %s", MNTTYPE_LOFS, myname);
	argv[0] = typename;

	while ((c = getopt(argc, argv, "o:rmOq")) != EOF) {
		switch (c) {
		case '?':
			errflag++;
			break;

		case 'o':
			options = optarg;
			break;
		case 'O':
			flags |= MS_OVERLAY;
			break;
		case 'r':
			flags |= MS_RDONLY;
			break;

		case 'm':
			flags |= MS_NOMNTTAB;
			break;

		case 'q':
			qflg = 1;
			break;

		default:
			usage();
		}
	}
	if ((argc - optind != 2) || errflag) {
		usage();
	}
	fsname = argv[argc - 2];
	dir = argv[argc - 1];

	while (options != NULL && *options != '\0') {
		saveopt = options;
		switch (getsubopt(&options, myopts, &value)) {
		case RO:
			flags |= MS_RDONLY;
			break;
		case RW:
			flags &= ~(MS_RDONLY);
			break;
		default:
			if (!qflg) {
				(void) fprintf(stderr, gettext(
				    "mount: %s on %s - WARNING unknown "
				    "option \"%s\"\n"), fsname, dir, 
				    saveopt);
			}
		}
	}

	if (geteuid() != 0) {
		fprintf(stderr, "%s, not super user\n", myname);
		exit(RET_ERR);
	}

	signal(SIGHUP,  SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT,  SIG_IGN);

	if (mount(fsname, dir, flags | MS_DATA, fstype, 0, 0) < 0) {
		(void) fprintf(stderr, "mount: ");
		perror(fsname);
		exit(RET_ERR);
	}

	exit(0);
	/* NOTREACHED */
}

void
usage()
{
	(void) fprintf(stderr,
	    "Usage: mount -F lofs [-r] [-o ro | rw] dir mountpoint\n");
	exit(RET_ERR);
}

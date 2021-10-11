#pragma ident	"@(#)mount.c	1.22	99/09/23 SMI"

/*
 * Copyright (c) 1988 Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/vfs.h>
#include <sys/types.h>
#include <locale.h>
#include <sys/stat.h>
#include <fslib.h>

#define	MNTTYPE_TMPFS	"tmpfs"

#define	MNTOPT_DEV	"dev"


extern time_t	time();
extern int errno;

#define	FSSIZE	0
#define	VERBOSE	1
#define	RO	2
#define	RW	3
#define	NOSUID	4
#define	SUID	5

char *myopts[] = {
	"size",
	"vb",
	"ro",
	"rw",
	"nosuid",
	"suid",
	NULL
};

int	nmflg = 0;
int	rwflg = 0;
int	qflg = 0;

main(argc, argv)
	int argc;
	char *argv[];
{
	/* mount information */
	char *special;
	char *mountp;
	char *fstype;

	int c;
	char *myname;
	char typename[64];
	extern int optind;
	extern char *optarg;
	int error = 0;
	int verbose = 0;
	int mflg = 0;
	int optcnt = 0;
	extern int getopt();
	extern int getsubopt();

	char optbuf[MAX_MNTOPT_STR];
	int optsize = 0;
	char *saveoptbuf;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	myname = strrchr(argv[0], '/');
	myname = myname ? myname + 1 : argv[0];
	(void) sprintf(typename, "%s_%s", MNTTYPE_TMPFS, myname);
	argv[0] = typename;
	optbuf[0] = '\0';

	while ((c = getopt(argc, argv, "?o:VmOq")) != EOF) {
		switch (c) {
		case 'V':
			verbose++;
			break;
		case '?':
			error++;
			break;
		case 'm':
			nmflg++;
			break;
		case 'O':
			mflg |= MS_OVERLAY;
			break;
		case 'o':
			(void) strncpy(optbuf, optarg, MAX_MNTOPT_STR);
			optbuf[MAX_MNTOPT_STR - 1] = '\0';
			optsize = strlen(optbuf);

			if (verbose)
				(void) fprintf(stderr, "optsize:%d optbuf:%s\n",
				    optsize, optbuf);
			break;
		case 'q':
			qflg++;
			break;
		}
	}

	if (geteuid() != 0) {
		(void) fprintf(stderr, gettext("Must be root to use mount\n"));
		exit(32);
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

	if (argc - optind != 2 || error) {
		(void) fprintf(stderr,
		    gettext("Usage: %s [-o size] swap mount_point\n"),
		    typename);
		exit(32);
	}

	special = argv[optind++];
	mountp = argv[optind++];
	fstype = MNTTYPE_TMPFS;
	mflg |= MS_OPTIONSTR;
	mflg |= (nmflg ? MS_NOMNTTAB : 0);

	signal(SIGHUP,  SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT,  SIG_IGN);

	if (verbose) {
		(void) fprintf(stderr, "mount(%s, \"%s\", %d, %s",
		    special, mountp, mflg, MNTTYPE_TMPFS);
		if (optsize)
			(void) fprintf(stderr, ", \"%s\", %d)\n",
			    optbuf, strlen(optbuf));
		else
			(void) fprintf(stderr, ")\n");
	}
	if (optsize) {
		if ((saveoptbuf = strdup(optbuf)) == NULL) {
			(void) fprintf(stderr, gettext("%s: out of memory\n"),
				"mount");
			exit(1);
		}
	}
	if (mount(special, mountp, mflg, MNTTYPE_TMPFS, NULL, 0,
	    optbuf, MAX_MNTOPT_STR)) {
		if (errno == EBUSY)
			(void) fprintf(stderr,
			    gettext("mount: %s already mounted\n"),
			    mountp);
		else
			perror("mount");
		exit(32);
	}

	if (optsize && !qflg)
		cmp_requested_to_actual_options(saveoptbuf, optbuf,
			special, mountp);
	exit(0);
	/* NOTREACHED */
}


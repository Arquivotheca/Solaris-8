#pragma ident	"@(#)mount.c	1.4	99/07/15 SMI"

/*
 * Copyright (c) 1999 Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mnttab.h>
#include <sys/mount.h>
#include <sys/fs/xmem.h>
#include <sys/types.h>
#include <locale.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <fslib.h>
#include <stdlib.h>

#define	MNTTYPE_XMEMFS	"xmemfs"

extern time_t	time();
extern int errno;

enum {
	FSSIZE,
	VERBOSE,
	NOSUID,
	SUID,
	LARGEBSIZE,
#ifdef DEBUG
	NOLARGEBSIZE,
	BSIZE,
	RESERVEMEM,
	NORESERVEMEM,
#endif
	XOPTSZ
};

static
char *myopts[] = {
	"size",			/* required */
	"vb",
	"nosuid",
	"suid",
	"largebsize",
#ifdef DEBUG
	"nolargebsize",		/* default */
	"bsize",		/* internal use only */
	"reservemem",		/* default */
	"noreservemem",
#endif
	NULL
};

offset_t
atosz(char *optarg)
{
	offset_t	off;
	char		*endptr;

	off = strtoll(optarg, &endptr, 0);

	switch (*endptr) {
	case 't': case 'T':
		off *= 1024;
	case 'g': case 'G':
		off *= 1024;
	case 'm': case 'M':
		off *= 1024;
	case 'k': case 'K':
		off *= 1024;
	default:
		break;
	}
	return (off);
}



extern int		getopt();
extern int		getsubopt();

main(int argc, char *argv[])
{
	struct mnttab		mnt;
	int			c;
	char			*myname;
	char			optbuf[MAX_MNTOPT_STR];
	char			typename[64];
	char			*options, *value;
	extern int		optind;
	extern char		*optarg;
	int			error = 0;
	int			verbose = 0;
	int			nosuid = 0;
	int			nmflg = 0;
	offset_t		fssize = 0;
	offset_t		bsize = 0;
	int			optsize = sizeof (struct xmemfs_args);
	int			mflg = 0;
	int			optcnt = 0;
	struct xmemfs_args	xargs = {
		0,			/* xa_fssize - file system sz */
		0,			/* xa_bsize - blk sz */
		XARGS_RESERVEMEM	/* xa_flags */
	};

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	myname = strrchr(argv[0], '/');
	myname = myname ? myname + 1 : argv[0];
	(void) sprintf(typename, "%s_%s", MNTTYPE_XMEMFS, myname);
	argv[0] = typename;

	(void) strcpy(optbuf, "rw");	/* RO xmemfs not supported... */

	while ((c = getopt(argc, argv, "?o:mO")) != EOF) {
		switch (c) {
		case 'V':
			verbose++;
			break;
		case '?':
			error++;
			break;
		case 'm':
			nmflg++;
			mflg |= MS_NOMNTTAB;
			break;
		case 'O':
			mflg |= MS_OVERLAY;
			break;
		case 'o':
			options = optarg;
			while (*options != '\0') {
				switch (getsubopt(&options, myopts, &value)) {
				case LARGEBSIZE:
					xargs.xa_flags |= XARGS_LARGEPAGES;
					break;
				case FSSIZE:
					if (value) {
						fssize = atosz(value);
						if (!fssize) {
							(void) fprintf(stderr,
gettext("%s: value %s for option \"%s\" is invalid\n"),
typename, value, myopts[FSSIZE]);
							error++;
							break;
						}
						xargs.xa_fssize = fssize;
						optcnt++;
						if (verbose)
							(void) fprintf(stderr,
gettext("setting fssize to %d\n"), fssize);
					} else {
						(void) fprintf(stderr,
gettext("%s: option \"%s\" requires value\n"), typename, myopts[FSSIZE]);
						error++;
					}
					break;
#ifdef DEBUG
				case RESERVEMEM:
					xargs.xa_flags |= XARGS_RESERVEMEM;
					break;
				case NORESERVEMEM:
					xargs.xa_flags &= ~XARGS_RESERVEMEM;
					break;
				case NOLARGEBSIZE:
					xargs.xa_flags &= ~XARGS_LARGEPAGES;
					break;
				case BSIZE:	/* file system block size */
					if (value) {
						bsize = atosz(value);
						if (!bsize) {
							(void) fprintf(stderr,
gettext("%s: value %s for option \"%s\" is invalid\n"),
typename, value, myopts[FSSIZE]);
							error++;
							break;
						}
						xargs.xa_bsize = bsize;
						optcnt++;
						if (verbose)
							(void) fprintf(stderr,
gettext("setting bsize to %d\n"), bsize);
					} else {
						(void) fprintf(stderr,
gettext("%s: option \"%s\" requires value\n"), typename, myopts[BSIZE]);
						error++;
					}
					break;
#endif

				case VERBOSE:
					verbose++;
					break;
				case NOSUID:
					mflg |= MS_NOSUID;
					nosuid++;
					break;
				case SUID:
					mflg &= ~MS_NOSUID;
					nosuid = 0;
					break;
				default:
					(void) fprintf(stderr,
gettext("%s: illegal -o suboption -- %s\n"), typename, value);
					error++;
					break;
				}
			}
			if (nosuid)
				(void) strcat(optbuf, ",nosuid");
			if (bsize) {
				(void) sprintf(optbuf, "%s,bsize=%d", optbuf,
				    bsize);
				if (--optcnt)
					(void) strcat(optbuf, ",");
				if (verbose)
					(void) fprintf(stderr, "optbuf:%s\n",
					    optbuf);
			}
			if (fssize) {
				(void) sprintf(optbuf, "%s,size=%d", optbuf,
				    fssize);
				if (--optcnt)
					(void) strcat(optbuf, ",");
				if (verbose)
					(void) fprintf(stderr, "optbuf:%s\n",
					    optbuf);
			} else {
				error++;
			}
			if (options[0] && !error) {
				(void) strcat(optbuf, options);
				if (verbose)
					(void) fprintf(stderr, "optbuf:%s\n",
					    optbuf);
			}
			if (verbose)
				(void) fprintf(stderr, "optsize:%d optbuf:%s\n",
				    optsize, optbuf);
			break;
		}
	}

	if (geteuid() != 0) {
		(void) fprintf(stderr, gettext("Must be root to use mount\n"));
		exit(1);
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
		    gettext("Usage: %s -o[largebsize,]size=sz"
				" xmem mount_point\n"), typename);
		exit(1);
	}

	mnt.mnt_special = argv[optind++];
	mnt.mnt_mountp = argv[optind++];
	mnt.mnt_fstype = MNTTYPE_XMEMFS;
	mflg |= MS_DATA | MS_OPTIONSTR;
	mnt.mnt_mntopts = optbuf;

	(void) signal(SIGHUP,  SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);
	(void) signal(SIGINT,  SIG_IGN);

	if (verbose) {
		(void) fprintf(stderr, "mount(%s, \"%s\", %d, %s",
		    mnt.mnt_special, mnt.mnt_mountp, mflg, MNTTYPE_XMEMFS);
		if (optsize)
			(void) fprintf(stderr, ", \"%s\", %d)\n",
			    optbuf, strlen(optbuf));
		else
			(void) fprintf(stderr, ")\n");
	}
	if (mount(mnt.mnt_special, mnt.mnt_mountp, mflg, MNTTYPE_XMEMFS,
		    &xargs, optsize, optbuf, MAX_MNTOPT_STR)) {
		if (errno == EBUSY)
			(void) fprintf(stderr,
			    gettext("mount: %s already mounted\n"),
			    mnt.mnt_mountp);
		else
			perror("mount");
		exit(1);
	} else {
		struct statvfs		vfstat;

		if (statvfs(mnt.mnt_mountp, &vfstat) == 0) {

			offset_t	sz, bsz;

			/* put actual fs and blk sz in optbuf */

			bsz = vfstat.f_bsize / 1024;
			sz = vfstat.f_blocks * bsz;

			sprintf(optbuf, "rw,size=%lldK,bsize=%lldK", sz, bsz);
		}
	}

	exit(0);
	/* NOTREACHED */
	return (0);
}

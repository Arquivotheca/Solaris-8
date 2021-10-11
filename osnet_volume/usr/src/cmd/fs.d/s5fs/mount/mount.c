/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "@(#)mount.c	1.5	99/07/29 SMI"

#include	<stdio.h>
#include	<sys/signal.h>
#include	<unistd.h>	/* defines F_LOCK for lockf */
#include	<sys/errno.h>
#include	<sys/mnttab.h>
#include	<sys/mount.h>
#include	<sys/types.h>
#include	<sys/statvfs.h>
#include	<fslib.h>

#define	NAME_MAX	64	/* sizeof "fstype myname" */

#define	FSTYPE		"s5fs"

#define	RO_BIT		1

#define	READONLY	0
#define	READWRITE	1
#define	SUID		2
#define	NOSUID		3
#define	REMOUNT		4

extern int	errno;
extern int	optind;
extern char	*optarg;

extern char	*strrchr();
extern time_t	time();

int	roflag = 0;

char	typename[NAME_MAX], *myname;

char	temp[] = "/etc/mnttab.tmp";
char	fstype[] = FSTYPE;
char	*myopts[] = {
	"ro",
	"rw",
	"suid",
	"nosuid",
	"remount",
	NULL
};

main(argc, argv)
	int	argc;
	char	**argv;
{
	char	*special, *mountp;
	char	*options, *value;
	int	errflag = 0;
	int	confflag = 0;
	int 	suidflg = 0;
	int 	nosuidflg = 0;
	int	remountflg = 0;
	int	mntflag = 0;
	int	cc, rwflag = 0;
	struct mnttab	mm;
	char mops[MAX_MNTOPT_STR];

	myname = strrchr(argv[0], '/');
	if (myname)
		myname++;
	else
		myname = argv[0];
	sprintf(typename, "%s %s", fstype, myname);
	argv[0] = typename;

	/*
	 *	check for proper arguments
	 */

	while ((cc = getopt(argc, argv, "?o:r")) != -1)
		switch (cc) {
		case 'r':
			if ((roflag & RO_BIT) || rwflag)
				confflag = 1;
			else if (rwflag)
				confflag = 1;
			else {
				roflag |= RO_BIT;
				mntflag |= MS_RDONLY;
			}
			break;
		case 'o':
			options = optarg;
			while (*options != '\0')
				switch (getsubopt(&options, myopts, &value)) {
				case READONLY:
					if (rwflag)
						confflag = 1;
					else {
						roflag |= RO_BIT;
						mntflag |= MS_RDONLY;
					}
					break;
				case READWRITE:
					if (roflag & RO_BIT)
						confflag = 1;
					else if (rwflag)
						errflag = 1;
					else
						rwflag = 1;
					break;
				case SUID:
					if (nosuidflg)
						confflag = 1;
					else if (suidflg)
						errflag = 1;
					else
						suidflg++;
					break;
				case NOSUID:
					if (suidflg)
						confflag = 1;
					else if (nosuidflg)
						errflag = 1;
					else {
						mntflag |= MS_NOSUID;
						nosuidflg++;
					}
					break;
				case REMOUNT:
					if (remountflg)
						errflag = 1;
					else if (roflag & RO_BIT)
						confflag = 1;
					else {
						remountflg++;
						mntflag |= MS_REMOUNT;
					}
					break;
				default:
					fprintf(stderr,
				"%s: illegal -o suboption -- %s\n",
						typename, value);
					errflag++;
				}
			break;
		case '?':
			errflag = 1;
			break;
		}

	/*
	 *	There must be at least 2 more arguments, the
	 *	special file and the directory.
	 */

	if (confflag) {
		fprintf(stderr, "s5fs %s: warning: conflicting suboptions\n",
			myname);
		usage();
	}

	if (((argc - optind) != 2) || (errflag))
		usage();

	special = argv[optind++];
	mountp = argv[optind++];

	if (geteuid() != 0) {
		fprintf(stderr, "%s: not super user\n", myname);
		exit(31+2);
	}

	mm.mnt_special = special;
	mm.mnt_mountp = mountp;
	mm.mnt_fstype = fstype;
	mm.mnt_mntopts = mops;
	*mops = '\0';
	if (roflag & RO_BIT)
		strcat(mm.mnt_mntopts, "ro");
	else
		strcat(mm.mnt_mntopts, "rw");
	if (nosuidflg)
		strcat(mm.mnt_mntopts, ",nosuid");
	else
		strcat(mm.mnt_mntopts, ",suid");

	signal(SIGHUP,  SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT,  SIG_IGN);

	/*
	 *	Perform the mount.
	 *	Only the low-order bit of "roflag" is used by the system
	 *	calls (to denote read-only or read-write).
	 */
	do_mount(special, mountp, mntflag, mm.mnt_mntopts);

	exit(0);
}

rpterr(bs, mp)
	register char *bs, *mp;
{
	switch (errno) {
	case EPERM:
		fprintf(stderr, "%s: not super user\n", myname);
		break;
	case ENXIO:
		fprintf(stderr, "%s: %s no such device\n", myname, bs);
		break;
	case ENOTDIR:
		fprintf(stderr,
"%s: %s not a directory\n\tor a component of %s is not a directory\n",
			myname, mp, bs);
		break;
	case ENOENT:
		fprintf(stderr,
"%s: %s or %s, no such file or directory or no previous mount was performed\n",
			myname, bs, mp);
		break;
	case EINVAL:
		fprintf(stderr, "s5fs %s: %s is not an s5 file system.\n",
			myname, bs);
		break;
	case EBUSY:
		fprintf(stderr,
			"%s: %s is already mounted, %s is busy,\n\tor allowable number of mount points exceeded\n",
			myname, bs, mp);
		break;
	case ENOTBLK:
		fprintf(stderr, "%s: %s not a block device\n", myname, bs);
		break;
	case EROFS:
		fprintf(stderr, "%s: %s write-protected\n", myname, bs);
		break;
	case ENOSPC:
		fprintf(stderr, "%s: %s is corrupted. needs checking\n",
			myname, bs);
		break;
	case ENODEV:
		fprintf(stderr, "%s no such device or write-protected\n", bs);
		break;
	default:
		perror(myname);
		fprintf(stderr, "%s: cannot mount %s\n", myname, bs);
	}
}

do_mount(special, mountp, flag, opts)
	char	*special, *mountp;
	int	flag;
	char	*opts;
{
	register char *ptr;
	struct statvfs stbuf;
	int i;

	if (mount(special, mountp, flag | MS_DATA | MS_OPTIONSTR, fstype,
		NULL, 0, opts, MAX_MNTOPT_STR)) {
		rpterr(special, mountp);
		exit(31+2);
	}

	/*
	 *	compare the basenames of the mount point
	 *	and the volume name, warning if they differ.
	 */

	if (statvfs(mountp, &stbuf) == -1)
		return;

	ptr = stbuf.f_fstr;
	while (*ptr == '/')
		ptr++;

	if (strncmp(strrchr(mountp, '/') + 1, ptr, 6))
		fprintf(stderr, "%s: warning: <%.6s> mounted as <%s>\n",
			myname, stbuf.f_fstr, mountp);
}

usage()
{
	fprintf(stderr,
		"%s Usage:\n%s [-F %s] [generic_options] [-r] [-o {[rw|ro],[suid|nosuid],[remount]}] {special | mount_point}\n%s [-F %s] [generic_options] [-r] [-o {[rw|ro],[suid|nosuid],[remount]}] special mount_point\n",
		fstype, myname, fstype, myname, fstype);
	exit(31+1);
}

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)mount.c	1.14	99/09/23 SMI"	/* SVr4.0 1.4	*/

#include	<stdio.h>
#include	<signal.h>
#include	<unistd.h>	/* defines F_LOCK for lockf */
#include	<errno.h>
#include	<sys/mnttab.h>
#include	<sys/mount.h>
#include	<sys/types.h>
#include 	<locale.h>
#include	<sys/stat.h>
#include	<fslib.h>
#define	NAME_MAX	64	/* sizeof "fstype myname" */

#define	MNTOPT_DEV	"dev"

#define	FSTYPE		"proc"

#define	RO_BIT		1

#define	READONLY	0
#define	READWRITE	1

extern int	errno;
extern int	optind;
extern char	*optarg;

extern char	*strrchr();
extern time_t	time();

int	roflag = 0;
int	mflg = 0;	/* don't update /etc/mnttab flag */
int	Oflg = 0;
int	qflg = 0;

char	typename[NAME_MAX], *myname;
char	fstype[] = FSTYPE;
char	*myopts[] = {
	"ro",
	"rw",
	NULL
};

main(argc, argv)
	int	argc;
	char	**argv;
{
	char	*special, *mountp;
	char	*options = NULL;
	char	*value;
	int	errflag = 0;
	int	cc, rwflag = 0;
	struct mnttab	mm;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

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

	while ((cc = getopt(argc, argv, "?o:rmOq")) != -1)
		switch (cc) {
		case 'm':
			mflg++;
			break;
		case 'r':
			if (roflag & RO_BIT)
				errflag = 1;
			else
				roflag |= RO_BIT;
			break;
		case 'O':
			Oflg++;
			break;
		case 'o':
			options = optarg;
			break;
		case 'q':
			qflg = 1;
			break;
		case '?':
			errflag = 1;
			break;
		}

	/* Process sub-options */
	while (options != NULL && *options != '\0')
		switch (getsubopt(&options, myopts, &value)) {
		case READONLY:
			if ((roflag & RO_BIT) || rwflag)
				errflag = 1;
			else
				roflag |= RO_BIT;
			break;
		case READWRITE:
			if ((roflag & RO_BIT) || rwflag)
				errflag = 1;
			else
				rwflag = 1;
			break;
		default:
			if (!qflg)
				fprintf(stderr, gettext(
					"%s: illegal -o suboption -- %s\n"),
					typename, value);
		}

	/*
	 *	There must be at least 2 more arguments, the
	 *	special file and the directory.
	 */

	if (((argc - optind) != 2) || (errflag))
		usage();

	special = argv[optind++];
	mountp = argv[optind++];

	if (geteuid() != 0) {
		fprintf(stderr, gettext("%s: not super user\n"), myname);
		exit(2);
	}

	signal(SIGHUP,  SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT,  SIG_IGN);

	/*
	 *	Perform the mount.
	 *	Only the low-order bit of "roflag" is used by the system
	 *	calls (to denote read-only or read-write).
	 */
	do_mount(special, mountp, roflag & RO_BIT);

	exit(0);
}

rpterr(bs, mp)
	register char *bs, *mp;
{
	switch (errno) {
	case EPERM:
		fprintf(stderr, gettext("%s: not super user\n"), myname);
		break;
	case ENXIO:
		fprintf(stderr, gettext("%s: %s no such device\n"), myname, bs);
		break;
	case ENOTDIR:
		fprintf(stderr,
gettext("%s: %s not a directory\n\tor a component of %s is not a directory\n"),
			myname, mp, bs);
		break;
	case ENOENT:
		fprintf(stderr,
			gettext("%s: %s or %s, no such file or directory\n"),
			myname, bs, mp);
		break;
	case EINVAL:
		fprintf(stderr, gettext("%s: %s is not this fstype.\n"),
			myname, bs);
		break;
	case EBUSY:
		fprintf(stderr,
			gettext("%s: %s is already mounted, %s is busy,\n\tor allowable number of mount points exceeded\n"),
			myname, bs, mp);
		break;
	case ENOTBLK:
		fprintf(stderr, gettext("%s: %s not a block device\n"),
				myname, bs);
		break;
	case EROFS:
		fprintf(stderr, gettext("%s: %s write-protected\n"),
				myname, bs);
		break;
	case ENOSPC:
		fprintf(stderr,
			gettext("%s: the state of %s is not okay\n\tand it was attempted to mount read/write\n"),
			myname, bs);
		break;
	default:
		perror(myname);
		fprintf(stderr, gettext("%s: cannot mount %s\n"), myname, bs);
	}
}

do_mount(special, mountp, rflag)
	char	*special, *mountp;
	int	rflag;
{

	if (Oflg)
		rflag |= MS_OVERLAY;
	if (mflg)
		rflag |= MS_NOMNTTAB;
	if (mount(special, mountp, rflag | MS_DATA, fstype, (char *)NULL, 0)) {
		rpterr(special, mountp);
		exit(2);
	}
}

usage()
{
	fprintf(stderr,
		gettext("%s usage:\n%s [-F %s] [-r] [-o specific_options] {special | mount_point}\n%s [-F %s] [-r] [-o specific_options] special mount_point\n"),
		fstype, myname, fstype, myname, fstype);
	exit(1);
}

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)quotaon.c	1.23	98/06/15 SMI"	/* SVr4.0 1.9 */
/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * Copyright (c) 1986,1987,1988,1989,1996,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *                All rights reserved.
 *
 */

/*
 * Turn quota on/off for a filesystem.
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mntent.h>

#define	bcopy(f, t, n)    memcpy(t, f, n)
#define	bzero(s, n)	memset(s, 0, n)
#define	bcmp(s, d, n)	memcmp(s, d, n)

#define	index(s, r)	strchr(s, r)
#define	rindex(s, r)	strrchr(s, r)

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/file.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/fs/ufs_quota.h>
#include <stdio.h>
#include <sys/mnttab.h>
#include <sys/errno.h>
#include <sys/vfstab.h>

int	vflag;		/* verbose */
int	aflag;		/* all file systems */

#define	QFNAME "quotas"
#define	CHUNK	50
char	quotafile[MAXPATHLEN + 1];
char	**listbuf;
char	*mntopt(), *hasvfsopt(), *hasmntopt();
char	*whoami;

static void fixmntent();
static void mnterror();
static void usage();
static int oneof();
static int quotaonoff();
static int quotactl();

extern int	optind;
extern char	*optarg;
extern int errno;

void
main(argc, argv)
	int argc;
	char **argv;
{
	struct mnttab mntp;
	struct vfstab vfsbuf;
	char **listp;
	int listcnt;
	FILE *mtab, *vfstab, *tmp;
	int offmode = 0;
	int listmax = 0;
	int errs = 0;
	char *tmpname = "/etc/mnttab.temp";
	int		status;
	int		opt;
	mode_t		oldumask;
	struct stat	statbuf;

	whoami = (char *)rindex(*argv, '/') + 1;
	if (whoami == (char *)1)
		whoami = *argv;
	if (strcmp(whoami, "quotaoff") == 0)
		offmode++;
	else if (strcmp(whoami, "quotaon") != 0) {
		fprintf(stderr, "Name must be quotaon or quotaoff not %s\n",
			whoami);
		exit(31+1);
	}
	if ((listbuf = (char **)malloc(sizeof (char *) * CHUNK)) == NULL) {
		fprintf(stderr, "Can't alloc lisbuf array.");
		exit(31+1);
	}
	listmax = CHUNK;
	while ((opt = getopt(argc, argv, "avV")) != EOF) {
		switch (opt) {

		case 'v':
			vflag++;
			break;

		case 'a':
			aflag++;
			break;

		case 'V':		/* Print command line */
			{
				char		*opt_text;
				int		opt_cnt;

				(void) fprintf(stdout, "%s -F UFS ", whoami);
				for (opt_cnt = 1; opt_cnt < argc; opt_cnt++) {
					opt_text = argv[opt_cnt];
					if (opt_text)
						(void) fprintf(stdout, " %s ",
							opt_text);
				}
				(void) fprintf(stdout, "\n");
			}
			break;

		case '?':
			usage(whoami);
		}
	}
	if (argc <= optind && !aflag) {
		usage(whoami);
	}
	/*
	 * If aflag go through vfstab and make a list of appropriate
	 * filesystems.
	 */
	if (aflag) {

		listp = listbuf;
		listcnt = 0;

		vfstab = fopen(VFSTAB, "r");
		if (vfstab == NULL) {
			fprintf(stderr, "Can't open %s\n", VFSTAB);
			perror(VFSTAB);
			exit(31+1);
		}

		while ((status = getvfsent(vfstab, &vfsbuf)) == NULL) {
			if (strcmp(vfsbuf.vfs_fstype, MNTTYPE_UFS) != 0 ||
			    (vfsbuf.vfs_mntopts == 0) ||
			    hasvfsopt(&vfsbuf, MNTOPT_RO) ||
			    (!hasvfsopt(&vfsbuf, MNTOPT_RQ) &&
			    !hasvfsopt(&vfsbuf, MNTOPT_QUOTA)))
				continue;
			*listp = malloc(strlen(vfsbuf.vfs_special) + 1);
			strcpy(*listp, vfsbuf.vfs_special);
			listp++;
			listcnt++;
			/* grow listbuf if needed */
			if (listcnt >= listmax) {
				listmax += CHUNK;
				listbuf = (char **)realloc(listbuf,
					sizeof (char *) * listmax);
				if (listbuf == NULL) {
					fprintf(stderr,
						"Can't grow listbuf.\n");
					exit(31+1);
				}
				listp = &listbuf[listcnt];
			}
		}
		fclose(vfstab);
		*listp = (char *)0;
		listp = listbuf;
	} else {
		listp = &argv[optind];
		listcnt = argc - optind;
	}

	/*
	 * Open real mnttab
	 */
	mtab = fopen(MNTTAB, "r");
	if (mtab == NULL) {
		fprintf(stderr, "Can't open %s\n", MNTTAB);
		perror(whoami);
		exit(31+1);
	}
	/* check every entry for validity before we change mnttab */
	while ((status = getmntent(mtab, &mntp)) == 0)
		;
	if (status > 0)
		mnterror(status);
	rewind(mtab);

	signal(SIGHUP,  SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT,  SIG_IGN);

	/*
	 * Loop through mnttab, if a file system gets turned on or off
	 * do the quota call.
	 */
	while ((status = getmntent(mtab, &mntp)) == NULL) {
		if (strcmp(mntp.mnt_fstype, MNTTYPE_UFS) == 0 &&
		    !hasmntopt(&mntp, MNTOPT_RO) &&
		    (oneof(mntp.mnt_special, listp, listcnt) ||
		    oneof(mntp.mnt_mountp, listp, listcnt))) {
			errs += quotaonoff(&mntp, offmode);
		}
	}
	fclose(mtab);

	while (listcnt--) {
		if (*listp) {
			fprintf(stderr, "Cannot do %s\n", *listp);
			errs++;
		}
		listp++;
	}
	if (errs > 0)
		errs += 31;
	exit(errs);
}

int
quotaonoff(mntp, offmode)
	register struct mnttab *mntp;
	int offmode;
{

	if (offmode) {
		if (quotactl(Q_QUOTAOFF, mntp->mnt_mountp, (uid_t)0, NULL) < 0)
			goto bad;
		if (vflag)
			printf("%s: quotas turned off\n", mntp->mnt_mountp);
	} else {
		(void) sprintf(quotafile, "%s/%s", mntp->mnt_mountp, QFNAME);
		if (quotactl(Q_QUOTAON, mntp->mnt_mountp, (uid_t)0, quotafile) <
		    0)
			goto bad;
		if (vflag)
			printf("%s: quotas turned on\n", mntp->mnt_mountp);
	}
	return (0);
bad:
	fprintf(stderr, "quotactl: ");
	perror(mntp->mnt_special);
	return (1);
}

int
oneof(target, olistp, on)
	char *target;
	register char **olistp;
	register int on;
{
	int n = on;
	char **listp = olistp;

	while (n--) {
		if (*listp && strcmp(target, *listp) == 0) {
			*listp = (char *)0;
			return (1);
		}
		listp++;
	}
	return (0);
}

void
usage(whoami)
	char	*whoami;
{

	fprintf(stderr, "ufs usage:\n");
	fprintf(stderr, "\t%s [-v] -a\n", whoami);
	fprintf(stderr, "\t%s [-v] filesys ...\n", whoami);
		exit(31+1);
}


int
quotactl(cmd, mountpt, uid, addr)
	int		cmd;
	char		*mountpt;
	uid_t		uid;
	caddr_t		addr;
{
	int		fd;
	int		status;
	struct quotctl	quota;
	char		mountpoint[256];

	if (mountpt == NULL || mountpt[0] == '\0') {
		errno = ENOENT;
		return (-1);
	}
	strcpy(mountpoint, mountpt);
	strcat(mountpoint, "/quotas");
	if ((fd = open64(mountpoint, O_RDWR)) < 0) {
		fprintf(stderr, "quotactl: %s ", mountpoint);
		perror("open");
		exit(31+1);
	}

	quota.op = cmd;
	quota.uid = uid;
	quota.addr = addr;
	status = ioctl(fd, Q_QUOTACTL, &quota);
	close(fd);
	return (status);
}

char *
hasvfsopt(vfs, opt)
	register struct vfstab *vfs;
	register char *opt;
{
	char *f, *opts;
	static char *tmpopts;

	if (tmpopts == 0) {
		tmpopts = (char *)calloc(256, sizeof (char));
		if (tmpopts == 0)
			return (0);
	}
	strcpy(tmpopts, vfs->vfs_mntopts);
	opts = tmpopts;
	f = mntopt(&opts);
	for (; *f; f = mntopt(&opts)) {
		if (strncmp(opt, f, strlen(opt)) == 0)
			return (f - tmpopts + vfs->vfs_mntopts);
	}
	return (NULL);
}

void
mnterror(flag)
	int	flag;
{
	switch (flag) {
	case MNT_TOOLONG:
		fprintf(stderr, "%s: line in mnttab exceeds %d characters\n",
			whoami, MNT_LINE_MAX-2);
		break;
	case MNT_TOOFEW:
		fprintf(stderr, "%s: line in mnttab has too few entries\n",
			whoami);
		break;
	case MNT_TOOMANY:
		fprintf(stderr, "%s: line in mnttab has too many entries\n",
			whoami);
		break;
	}
	exit(1);
}

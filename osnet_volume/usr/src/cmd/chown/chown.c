/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)chown.c	1.17	95/01/12 SMI"	/* SVr4.0 1.12	*/

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

/*
 * chown [-hR] uid file ...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <locale.h>


static struct	passwd	*pwd;
static struct	group	*grp;
static struct	stat	stbuf;
static uid_t	uid = -1;
static gid_t	gid = -1;
static int	status;
static int hflag, rflag, fflag = 0;

static int Perror(char *);
static int isnumber(char *);
static int chownr(char *, uid_t, gid_t);

int
main(int argc, char *argv[])
{
	register c;
	int ch;
	char *grpp;			/* pointer to group name arg */
	extern int optind;
	int errflg = 0;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)		/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((ch = getopt(argc, argv, "hRf")) != EOF) {
		switch (ch) {
		case 'h':
			hflag++;
			break;

		case 'R':
			rflag++;
			break;

		case 'f':
			fflag++;
			break;

		default:
			errflg++;
			break;
		}
	}

	/*
	 * Check for sufficient arguments
	 * or a usage error.
	 */

	argc -= optind;
	argv = &argv[optind];

	if (errflg || argc < 2) {
		(void) fprintf(stderr, gettext(
			"usage: chown [-fhR] owner[:group] file...\n"));
		exit(2);
	}

	/*
	 * POSIX.2
	 * Check for owner[:group]
	 */
	if ((grpp = strchr(argv[0], ':')) != NULL) {
		*grpp++ = 0;

		if (isnumber(grpp)) {
			gid = (gid_t)atol(grpp);
		} else if ((grp = getgrnam(grpp)) == NULL) {
			(void) fprintf(stderr, gettext(
				"chown: unknown group id %s\n"), grpp);
			exit(2);
		} else
			gid = grp->gr_gid;
	}

	if (isnumber(argv[0])) {
		uid = (uid_t)atol(argv[0]);
	} else if ((pwd = getpwnam(argv[0])) == NULL) {
		(void) fprintf(stderr, gettext(
			"chown: unknown user id %s\n"), argv[0]);
		exit(2);
	} else
		uid = pwd->pw_uid;

	for (c = 1; c < argc; c++) {
		if (lstat(argv[c], &stbuf) < 0) {
			status += Perror(argv[c]);
			continue;
		}
		if (rflag & ((stbuf.st_mode & S_IFMT) == S_IFLNK)) {
			if (!hflag) {
				if (chown(argv[c], uid, gid) < 0)
					status = Perror(argv[c]);
			} else {
				if (lchown(argv[c], uid, gid) < 0)
					status = Perror(argv[c]);
			}
		} else if (rflag && ((stbuf.st_mode&S_IFMT) == S_IFDIR)) {
			status += chownr(argv[c], uid, gid);
		} else if (hflag) {
			if (lchown(argv[c], uid, gid) < 0) {
				status = Perror(argv[c]);
			}
		} else {
			if (chown(argv[c], uid, gid) < 0) {
				status = Perror(argv[c]);
			}
		}
	}
	return (status);
}

static int
chownr(char *dir, uid_t uid, gid_t gid)
{
	register DIR *dirp;
	register struct dirent *dp;
	struct stat st;
	char savedir[1024];
	int ecode = 0;

	if (getcwd(savedir, 1024) == (char *)0) {
		(void) Perror("getcwd");
		exit(255);
	}

	if (chdir(dir) < 0)
		return (Perror(dir));
	if ((dirp = opendir(".")) == NULL)
		return (Perror(dir));
	dp = readdir(dirp);
	dp = readdir(dirp); /* read "." and ".." */
	for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
		if (lstat(dp->d_name, &st) < 0) {
			status += Perror(dp->d_name);
			continue;
		}
		if (rflag & ((st.st_mode & S_IFMT) == S_IFLNK)) {
			if (!hflag) {
				if (chown(dp->d_name, uid, gid) < 0)
					status = Perror(dp->d_name);
			} else {
				if (lchown(dp->d_name, uid, gid) < 0)
					status = Perror(dp->d_name);
			}
		} else if (rflag && ((st.st_mode&S_IFMT) == S_IFDIR)) {
			status += chownr(dp->d_name, uid, gid);
		} else if (hflag) {
			if (lchown(dp->d_name, uid, gid) < 0) {
				status = Perror(dp->d_name);
			}
		} else {
			if (chown(dp->d_name, uid, gid) < 0) {
				status = Perror(dp->d_name);
			}
		}
	}

	(void) closedir(dirp);
	if (chdir(savedir) < 0) {
		(void) fprintf(stderr, gettext(
			"chown: can't change back to %s"), savedir);
		exit(255);
	}

	/*
	 * Change what we are given after doing it's contents.
	 */
	if (chown(dir, uid, gid) < 0)
		return (Perror(dir));

	return (ecode);
}

static int
isnumber(char *s)
{
	int c;

	while ((c = *s++) != '\0')
		if (!isdigit(c))
			return (0);
	return (1);
}

static int
Perror(char *s)
{
	if (!fflag) {
		(void) fprintf(stderr, "chown: ");
		perror(s);
	}
	return (!fflag);
}

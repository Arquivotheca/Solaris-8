/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Portions Copyright (c) 1988, Sun Microsystems, Inc.	*/
/*	All Rights Reserved.					*/

#ident	"@(#)chgrp.c	1.17	94/10/13 SMI"	/* SVr4.0 1.9	*/

/*
 * chgrp  [-h] [-R] gid file ...
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <grp.h>
#include <dirent.h>
#include <unistd.h>
#include <locale.h>

struct	group	*gr;
struct	stat	stbuf;
struct	stat	stbuf2;
gid_t	gid;
int	hflag = 0;
int	fflag = 0;
int	rflag = 0;
int	status;
int 	acode = 0;
extern  int optind;

main(argc, argv)
int  argc;
char *argv[];
{
	register c;

	/* set the locale for only the messages system (all else is clean) */

	setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)		/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "hRf")) != EOF)
		switch (c) {
			case 'R':
				rflag++;
				break;
			case 'h':
				hflag++;
				break;
			case 'f':
				fflag++;
				break;
			default:
				usage();
		}

	/*
	 * Check for sufficient arguments
	 * or a usage error.
	 */
	argc -= optind;
	argv = &argv[optind];
	if (argc < 2)
		usage();

	if (isnumber(argv[0])) {
		gid = (gid_t)atoi(argv[0]); /* gid is an int */
	} else {
		if ((gr = getgrnam(argv[0])) == NULL) {
			fprintf(stderr, "chgrp: ");
			fprintf(stderr, gettext("unknown group: %s\n"),
				argv[0]);
			exit(2);
		}
		gid = gr->gr_gid;
	}

	for (c = 1; c < argc; c++) {
		if (lstat(argv[c], &stbuf) < 0) {
			status += Perror(argv[c]);
			continue;
		}
		if (rflag & ((stbuf.st_mode & S_IFMT) == S_IFLNK)) {
			if (!hflag) {
				if (stat(argv[c], &stbuf2) < 0) {
					status += Perror(argv[c]);
					continue;
				}
				if (chown(argv[c], -1, gid) < 0)
					status = Perror(argv[c]);

				/*
				 * If the object is a directory reset the
				 * SUID bits.
				 */
				if ((stbuf2.st_mode & S_IFMT) == S_IFDIR) {
					if (chmod(argv[c],
						stbuf2.st_mode & ~S_IFMT) < 0) {
						status += Perror(argv[c]);
					}
				}
			} else {
				if (lchown(argv[c], -1, gid) < 0)
					status = Perror(argv[c]);
			}
		} else if (rflag && ((stbuf.st_mode&S_IFMT) == S_IFDIR)) {
			status += chownr(argv[c], -1, gid);
		} else if (hflag) {
			if (lchown(argv[c], -1, gid) < 0) {
				status = Perror(argv[c]);
			}
		} else {
			if (chown(argv[c], -1, gid) < 0) {
				status = Perror(argv[c]);
			}
		}
		/*
		 * If the object is a directory reset the SUID bits
		 */
		if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
			if (chmod(argv[c], stbuf.st_mode & ~S_IFMT) < 0) {
				status = Perror(argv[c]);
			}
		}
	}
	exit(status += acode);
}


chownr(dir, uid, gid)
	char *dir;
	uid_t uid;
	gid_t gid;
{
	register struct dirent *dp;
	register DIR *dirp;
	struct stat st, st2;
	char savedir[1024];
	int ecode = 0;

	if (getcwd(savedir, 1024) == 0) {
		fprintf(stderr, "chgrp: ");
		fprintf(stderr, gettext("%s\n"), savedir);
		exit(255);
	}

	/*
	 * Change what we are given before doing its contents.
	 */
	if (hflag) {
		if (lchown(dir, -1, gid) < 0 && Perror(dir))
			return (1);
	} else {
		if (stat(dir, &st2) < 0) {
			status += Perror(dir);
		}
		if (chown(dir, -1, gid) < 0 && Perror(dir))
			return (1);
		/*
		 * If the object is a directory reset the
		 * SUID bits.
		 */
		if ((st2.st_mode & S_IFMT) == S_IFDIR) {
			if (chmod(dir, st2.st_mode & ~S_IFMT) < 0) {
				status += Perror(dir);
			}
		}
	}

	if (chdir(dir) < 0)
		return (Perror(dir));
	if ((dirp = opendir(".")) == NULL)
		return (Perror(dir));
	dp = readdir(dirp);
	dp = readdir(dirp); /* read "." and ".." */
	for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
		if (lstat(dp->d_name, &st) < 0) {
			ecode += Perror(dp->d_name);
			continue;
		}
		if (rflag & ((st.st_mode & S_IFMT) == S_IFLNK)) {
			if (!hflag) {
				if (stat(dp->d_name, &st2) < 0) {
					status += Perror(dp->d_name);
					continue;
				}

				if (chown(dp->d_name, -1, gid) < 0)
					acode = Perror(dp->d_name);
				/*
				 * If the object is a directory reset the
				 * SUID bits.
				 */
				if ((st2.st_mode & S_IFMT) == S_IFDIR) {
					if (chmod(dp->d_name,
						st2.st_mode & ~S_IFMT) < 0) {
						status += Perror(dp->d_name);
					}
				}
			} else {
				if (lchown(dp->d_name, -1, gid) < 0)
					acode = Perror(dp->d_name);
			}
		} else if (rflag && ((st.st_mode&S_IFMT) == S_IFDIR)) {
			acode += chownr(dp->d_name, -1, gid);
		} else if (hflag) {
			if (lchown(dp->d_name, -1, gid) < 0) {
				acode = Perror(dp->d_name);
			}
		} else {
			if (chown(dp->d_name, -1, gid) < 0) {
				acode = Perror(dp->d_name);
			}
		}
		/*
		 * If the object is a directory reset the SUID bits
		 */
		if ((st.st_mode & S_IFMT) == S_IFDIR) {
			if (chmod(dp->d_name, st.st_mode & ~S_IFMT) < 0) {
				status = Perror(dp->d_name);
			}
		}
	}
	closedir(dirp);
	if (chdir(savedir) < 0) {
		fprintf(stderr, "chgrp: ");
		fprintf(stderr, gettext("can't change back to %s\n"), savedir);
		exit(255);
	}
	return (ecode);
}


isnumber(s)
char *s;
{
	register c;

	while (c = *s++)
		if (!isdigit(c))
			return (0);
	return (1);
}	


Perror(s)
char *s;
{
	if (!fflag) {
		fprintf(stderr, "chgrp: ");
		perror(s);
	}
	return (!fflag);
}


usage()
{
	fprintf(stderr, gettext(
		"usage: chgrp [-fhR] group file ...\n"));
	exit(2);
}

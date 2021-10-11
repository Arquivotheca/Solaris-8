/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T			*/
/*	  All Rights Reserved						*/
/*									*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T		*/
/*	The copyright notice above does not evidence any		*/
/*	actual or intended publication of such source code.		*/
/*									*/

#ident	"@(#)chmod.c	1.24	95/02/08 SMI"	/* SVr4.0 1.9	*/

/*			PROPRIETARY NOTICE (Combined)			*/
/*									*/
/*	This source code is unpublished proprietary information		*/
/*	constituting, or derived under license from AT&T's UNIX(r)	*/
/*	System V.							*/
/*	In addition, portions of such source code were derived from	*/
/*	Berkeley							*/
/*	4.3 BSD under license from the Regents of the University of	*/
/*	California.							*/
/*									*/
/*				Copyright Notice			*/
/*									*/
/*	Notice of copyright on this source code product does not	*/
/*	indicate publication.						*/
/*									*/
/*		(c) 1986,1987,1988,1989  Sun Microsystems, Inc		*/
/*		(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.		*/
/*			All rights reserved.				*/

/*
 * chmod option mode files
 * where
 *	mode is [ugoa][+-=][rwxXlstugo] or an octal number
 *	option is -R and -f
 */

/*
 *  Note that many convolutions are necessary
 *  due to the re-use of bits between locking
 *  and setgid
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <locale.h>
#include <string.h>	/* strerror() */
#include <stdarg.h>
#include <limits.h>

#define	USER	05700	/* user's bits */
#define	GROUP	02070	/* group's bits */
#define	OTHER	00007	/* other's bits */
#define	ALL	07777	/* all */

#define	READ	00444	/* read permit */
#define	WRITE	00222	/* write permit */
#define	EXEC	00111	/* exec permit */
#define	SETID	06000	/* set[ug]id */
#define	LOCK	02000	/* lock permit */
#define	STICKY	01000	/* sticky bit */

static int	rflag;
static int	fflag;

extern int	optind;
extern int	errno;

static int	mac;		/* Alternate to argc (for parseargs) */
static char	**mav;		/* Alternate to argv (for parseargs) */

static char	*ms;		/* Points to the mode argument */

extern mode_t
newmode(char *ms, mode_t new_mode, mode_t umsk, char *file);

static int
dochmod(char *name, mode_t umsk),
chmodr(char *dir, mode_t mode, mode_t umsk);

static void
usage(void);

void
errmsg(int severity, int code, char *format, ...);

static void
parseargs(int ac, char *av[]);

void
main(int argc, char *argv[])
{
	register int i, c;
	int status = 0;
	mode_t umsk;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	parseargs(argc, argv);

	while ((c = getopt(mac, mav, "Rf")) != EOF) {
		switch (c) {
		case 'R':
			rflag++;
			break;
		case 'f':
			fflag++;
			break;
		case '?':
			usage();
			exit(2);
		}
	}

	/*
	 * Check for sufficient arguments
	 * or a usage error.
	 */

	mac -= optind;
	mav += optind;

	if (mac < 2) {
		usage();
		exit(2);
	}

	ms = mav[0];

	umsk = umask(0);
	(void) umask(umsk);

	for (i = 1; i < mac; i++)
		status += dochmod(mav[i], umsk);

	exit(fflag ? 0 : status);
}

static int
dochmod(char *name, mode_t umsk)
{
	static struct stat st;
	int linkflg = 0;

	if (lstat(name, &st) < 0) {
		errmsg(2, 0, gettext("can't access %s\n"), name);
		return (1);
	}

	if ((st.st_mode & S_IFMT) == S_IFLNK) {
		linkflg = 1;
		if (stat(name, &st) < 0) {
			errmsg(2, 0, gettext("can't access %s\n"), name);
			return (1);
		}
	}

	/* Do not recurse if directory is object of symbolic link */
	if (rflag && ((st.st_mode & S_IFMT) == S_IFDIR) && !linkflg)
		return (chmodr(name, st.st_mode, umsk));

	if (chmod(name, newmode(ms, st.st_mode, umsk, name)) == -1) {
		errmsg(2, 0, gettext("can't change %s\n"), name);
		return (1);
	}

	return (0);
}

static int
chmodr(char *dir, mode_t mode, mode_t umsk)
{
	register DIR *dirp;
	register struct dirent *dp;
	char savedir[PATH_MAX];
	int ecode;

	if (getcwd(savedir, PATH_MAX) == 0)
		errmsg(2, 255, gettext("chmod: could not getcwd %s\n"),
		    savedir);

	/*
	 * Change what we are given before doing it's contents
	 */
	if (chmod(dir, newmode(ms, mode, umsk, dir)) < 0) {
		errmsg(2, 0, gettext("can't change %s\n"), dir);
		return (1);
	}
	if (chdir(dir) < 0) {
		errmsg(2, 0, "%s/%s: %s\n", savedir, dir, strerror(errno));
		return (1);
	}
	if ((dirp = opendir(".")) == NULL) {
		errmsg(2, 0, "%s\n", strerror(errno));
		return (1);
	}
	dp = readdir(dirp);
	dp = readdir(dirp); /* read "." and ".." */
	ecode = 0;
	for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp))
		ecode += dochmod(dp->d_name, umsk);
	(void) closedir(dirp);
	if (chdir(savedir) < 0) {
		errmsg(2, 255, gettext("can't change back to %s\n"), savedir);
	}
	return (ecode ? 1 : 0);
}

void
errmsg(int severity, int code, char *format, ...)
{
	va_list ap;
	static char *msg[] = {
	"",
	"ERROR",
	"WARNING",
	""
	};

	va_start(ap, format);

	/*
	 * Always print error message if this is a fatal error (code == 0);
	 * otherwise, print message if fflag == 0 (no -f option specified)
	 */
	if (!fflag || (code != 0)) {
		(void) fprintf(stderr,
			"chmod: %s: ", gettext(msg[severity]));
		(void) vfprintf(stderr, format, ap);
	}

	va_end(ap);

	if (code != 0)
		exit(fflag ? 0 : code);
}

static void
usage(void)
{
	(void) fprintf(stderr, gettext(
	    "usage:\tchmod [-fR] <absolute-mode> file ...\n"));

	(void) fprintf(stderr, gettext(
	    "\tchmod [-fR] <symbolic-mode-list> file ...\n"));

	(void) fprintf(stderr, gettext(
	    "where \t<symbolic-mode-list> is a comma-separated list of\n"));

	(void) fprintf(stderr, gettext(
	    "\t[ugoa]{+|-|=}[rwxXlstugo]\n"));
}

/*
 *  parseargs - generate getopt-friendly argument list for backwards
 *		compatibility with earlier Solaris usage (eg, chmod -w
 *		foo).
 *
 *  assumes the existence of a static set of alternates to argc and argv,
 *  (namely, mac, and mav[]).
 *
 */

static void
parseargs(int ac, char *av[])
{
	int i;			/* current argument			*/
	int fflag;		/* arg list contains "--"		*/
	size_t mav_num;		/* number of entries in mav[]		*/

	/*
	 * We add an extra argument slot, in case we need to jam a "--"
	 * argument into the list.
	 */

	mav_num = (size_t) ac+2;

	if ((mav = calloc(mav_num, sizeof (char *))) == NULL) {
		perror("chmod");
		exit(2);
	}

	/* scan for the use of "--" in the argument list */

	for (fflag = i = 0; i < ac; i ++) {
		if (strcmp(av[i], "--") == 0)
		    fflag = 1;
	}

	/* process the arguments */

	for (i = mac = 0;
	    (av[i] != (char *) NULL) && (av[i][0] != (char) NULL);
	    i++) {
		if (!fflag && av[i][0] == '-') {
			/*
			 *  If there is not already a "--" argument specified,
			 *  and the argument starts with '-' but does not
			 *  contain any of the official option letters, then it
			 *  is probably a mode argument beginning with '-'.
			 *  Force a "--" into the argument stream in front of
			 *  it.
			 */

			if ((strchr(av[i], 'R') == NULL &&
			    strchr(av[i], 'f') == NULL)) {
				mav[mac++] = strdup("--");
			}
		}

		mav[mac++] = strdup(av[i]);
	}

	mav[mac] = (char *) NULL;
}

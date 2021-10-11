/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)du.c 1.25	99/04/13 SMI"	/* SVr4.0 1.22  */

/*
 * du -- summarize disk usage
 *	/bin/du [-a][-d][-k][-r][-o|-s] [file ...]
 *	/usr/xpg4/bin/du [-a][-k][-r][-s][-x] [file ...]
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>

static int	aflg = 0, rflg = 0, sflg = 0, kflg = 0, oflg = 0, dflg = 0;
static int	Lflg = 0;
static char	*dot = ".";
static int	level = 0;	/* Level of recursion */

static char	*base;
static char	*name;
static size_t	base_len = PATH_MAX + 1;	/* # of chars for base */
static size_t	name_len = PATH_MAX + 1;	/* # of chars for name */

struct links {
	dev_t	dev;
	ino_t	ino;
};
#define	ML	1000	/* Number of struct links's to allocate at a time */

/*
 * convert DEV_BSIZE blocks to K blocks
 */
#define	DEV_KSHIFT	1
#define	kb(n)		(((u_longlong_t) (n)) >> DEV_KSHIFT)

long	wait();
static long descend(char *curname, int *retcode, dev_t device);
static void printsize(blkcnt_t blocks, char *path);
static void exitdu(int exitcode);

void
main(argc, argv)
	int argc;
	char **argv;
{
	blkcnt_t	blocks = 0;
	register	c;
	extern int	optind;
	register char	*np;
	register pid_t	pid, wpid;
	int		status, retcode = 0;
	setbuf(stderr, NULL);
	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

#ifdef XPG4
	rflg++;		/* "-r" is not an option but ON always */
#endif

#ifdef XPG4
	while ((c = getopt(argc, argv, "arskx")) != EOF)
#else
	while ((c = getopt(argc, argv, "arskodL")) != EOF)
#endif
		switch (c) {

		case 'a':
			aflg++;
			continue;

		case 'r':
			rflg++;
			continue;

		case 's':
			sflg++;
			continue;

		case 'k':
			kflg++;
			continue;

		case 'o':
			oflg++;
			continue;

		case 'd':
			dflg++;
			continue;

		case 'x':
			dflg++;
			continue;

		case 'L':
			Lflg++;
			continue;
#ifdef XPG4
		case '?':
			(void) fprintf(stderr,
		gettext("usage: du [-a][-k][-r][-s][-x] [file ...]\n"));
			exit(2);
#else
		case '?':
			(void) fprintf(stderr, gettext(
		"usage: du [-a][-d][-k][-r][-o|-s][-L] [file ...]\n"));
			exit(2);
#endif
		}
	if (optind == argc) {
		argv = &dot;
		argc = 1;
		optind = 0;
	}

	/* "-o" and "-s" don't make any sense together. */
	if (oflg && sflg)
		oflg = 0;

	if ((base = (char *)calloc(base_len, sizeof (char))) == NULL) {
		perror("du");
		exit(1);
	}
	if ((name = (char *)calloc(name_len, sizeof (char))) == NULL) {
		perror("du");
		free(base);
		exit(1);
	}
	do {
		if (optind < argc - 1) {
			pid = fork();
			if (pid == (pid_t)-1) {
				perror(gettext("du: No more processes"));
				exitdu(1);
			}
			if (pid != 0) {
				while ((wpid = wait(&status)) != pid &&
				    wpid != (pid_t)-1)
					;
				if (pid != (pid_t)-1 && status != 0)
					retcode = 1;
			}
		}
		if (optind == argc - 1 || pid == 0) {
			while (base_len < (strlen(argv[optind]) + 1)) {
				base_len = base_len * 2;
				if ((base = (char *)realloc(base, base_len *
				    sizeof (char))) == NULL) {
					if (rflg) {
						fprintf(stderr, gettext("du: "
						    "can't process %s"),
						    argv[optind]);
						perror("");
					}
					exitdu(1);
				}
			}
			if (base_len > name_len) {
				name_len = base_len;
				if ((name = (char *)realloc(name, name_len *
				    sizeof (char))) == NULL) {
					if (rflg) {
						fprintf(stderr, gettext("du: "
						    "can't process %s"),
						    argv[optind]);
						perror("");
					}
					exitdu(1);
				}
			}
			(void) strcpy(base, argv[optind]);
			(void) strcpy(name, argv[optind]);
			if (np = strrchr(name, '/')) {
				*np++ = '\0';
				if (chdir(*name ? name : "/") < 0) {
					if (rflg) {
						(void) fprintf(stderr, "du: ");
						perror(*name ? name : "/");
						exitdu(1);
					}
					exitdu(0);
				}
			} else
				np = base;
			blocks = descend(*np ? np : ".", &retcode,
				(dev_t) 0);
			if (sflg)
				printsize(blocks, base);
			if (optind < argc - 1)
				exitdu(retcode);
		}
		optind++;
	} while (optind < argc);
	exitdu(retcode);
}

static long
descend(curname, retcode, device)
	char *curname;
	int  *retcode;
	dev_t device;
{
	static DIR		*dirp = NULL;
	char			*ebase0, *ebase;
	struct stat		stb, stb1;
	int			i, j, ret;
	blkcnt_t		blocks = 0;
	off_t			curoff = 0;
	ptrdiff_t		offset;
	ptrdiff_t		offset0;
	register struct dirent	*dp;
	struct links		*temp_ml;
	static struct links	*ml = NULL;
	static int		mlx = 0, mlsize = 0;
	char			dirbuf[PATH_MAX+1];

	ebase0 = ebase = strchr(base, 0);
	if (ebase > base && ebase[-1] == '/')
		ebase--;
	offset = ebase - base;
	offset0 = ebase0 - base;

	if (Lflg == 0)
		i = lstat(curname, &stb);
	else {
		i = stat(curname, &stb);
		j = lstat(curname, &stb1);
	}

	if ((i < 0) || (j < 0)) {
		if (rflg) {
			(void) fprintf(stderr, "du: ");
			perror(base);
		}

		/*
		 * POSIX states that non-zero status codes are only set
		 * when an error message is printed out on stderr
		 */
		*retcode = (rflg ? 1 : 0);
		*ebase0 = 0;
		return (0);
	}
	if (device) {
		if (dflg && stb.st_dev != device) {
			*ebase0 = 0;
			return (0);
		}
	}
	else
		device = stb.st_dev;

	if (stb.st_nlink > 1 && (stb.st_mode & S_IFMT) != S_IFDIR) {
		for (i = 0; i < mlx; i++)
			if (ml[i].ino == stb.st_ino && ml[i].dev == stb.st_dev)
				return (0);
		if (mlx == mlsize)	/* Need to allocate more nodes! */
		{
			temp_ml = (struct links *) realloc(ml, (mlx + ML) *
				sizeof (struct links));
			if (temp_ml == NULL)
				perror(gettext(
					"du: can't keep track of more links"));
			else
			{
				ml = temp_ml;
				mlsize += ML;
			}
		}
		if (mlx < mlsize) {
			ml[mlx].dev = stb.st_dev;
			ml[mlx].ino = stb.st_ino;
			mlx++;
		}
	}
	blocks = stb.st_blocks;
	if ((stb.st_mode & S_IFMT) != S_IFDIR) {
		/*
		 * Don't print twice: if sflg, file will get printed in main().
		 * Otherwise, level == 0 means this file is listed on the
		 * command line, so print here; aflg means print all files.
		 */
		if (sflg == 0 && (aflg || level == 0))
			printsize(blocks, base);
		return (blocks);
	}
	if (dirp != NULL)
		(void) closedir(dirp);
	if ((dirp = opendir(curname)) == NULL) {
		if (rflg) {
			(void) fprintf(stderr, "du: ");
			perror(base);
		}
		*retcode = 1;
		*ebase0 = 0;
		return (0);
	}
	level++;
	if (Lflg && S_ISLNK(stb1.st_mode)) {
		if (getcwd(dirbuf, PATH_MAX) == NULL) {
			if (rflg) {
				(void) fprintf(stderr, "du: ");
				perror(base);
			}
			exitdu(1);
		}
	}
	if (chdir(curname) < 0) {
		if (rflg) {
			(void) fprintf(stderr, "du: ");
			perror(base);
		}
		*retcode = 1;
		*ebase0 = 0;
		(void) closedir(dirp);
		dirp = NULL;
		level--;
		return (0);
	}
	while (dp = readdir(dirp)) {
		if ((strcmp(dp->d_name, ".") == 0) ||
			(strcmp(dp->d_name, "..") == 0))
			continue;
		/*
		 * we're about to append "/" + dp->d_name
		 * onto end of base; make sure there's enough
		 * space
		 */
		while ((offset + strlen(dp->d_name) + 2) > base_len) {
			base_len = base_len * 2;
			if ((base = (char *)realloc(base, base_len *
			    sizeof (char))) == NULL) {
				if (rflg) {
					perror("du");
				}
				exitdu(1);
			}
			ebase = base + offset;
			ebase0 = base + offset0;
		}
		(void) sprintf(ebase, "/%s", dp->d_name);
		curoff = telldir(dirp);
		blocks += descend(ebase+1, retcode, device);
			/* base may have been moved via realloc in descend() */
		ebase = base + offset;
		ebase0 = base + offset0;
		*ebase = 0;
		if (dirp == NULL) {
			if ((dirp = opendir(".")) == NULL) {
				if (rflg) {
					(void) fprintf(stderr,
					    gettext("du: Can't reopen in "));
					perror(base);
				}
				*retcode = 1;
				level--;
				return (0);
			}
			seekdir(dirp, curoff);
		}
	}
	(void) closedir(dirp);
	level--;
	dirp = NULL;
	if (sflg == 0)
		printsize(blocks, base);
	if (Lflg && S_ISLNK(stb1.st_mode))
		ret = chdir(dirbuf);
	else
		ret = chdir("..");
	if (ret < 0) {
		if (rflg) {
			(void) sprintf(strchr(base, '\0'), "/..");
			(void) fprintf(stderr,
			    gettext("du: Can't change dir to '..' in "));
			perror(base);
		}
		exitdu(1);
	}
	*ebase0 = 0;
	if (oflg)
		return (0);
	else
		return (blocks);
}

static void
printsize(blkcnt_t blocks, char *path)
{
	if (!kflg) {
#ifdef XPG4
		printf("%lld %s\n", blocks, path);
#else
		printf("%lld\t%s\n", blocks, path);
#endif
	} else {
#ifdef XPG4
		printf("%lld %s\n", (long long) kb(blocks), path);
#else
		printf("%lld\t%s\n", (long long) kb(blocks), path);
#endif
	}
}

static void
exitdu(int exitcode) {
	free(base);
	free(name);
	exit(exitcode);
}

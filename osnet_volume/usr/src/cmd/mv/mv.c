/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/

/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *
 *		PROPRIETARY NOTICE (Combined)
 *
 *	This source code is unpublished proprietary information
 *	constituting, or derived under license from AT&T's UNIX(r) System V.
 *	In addition, portions of such source code were derived from Berkeley
 *	4.3 BSD under license from the Regents of the University of
 *	California.
 *
 *
 *
 *		Copyright Notice
 *
 *	Notice of copyright on this source code product does not indicate
 *	publication.
 *
 *	(c) 1986, 1987, 1988, 1989, 1997  Sun Microsystems, Inc
 *	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */

/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mv.c	1.57	99/07/09 SMI"

/*
 * Combined mv/cp/ln command:
 *	mv file1 file2
 *	mv dir1 dir2
 *	mv file1 ... filen dir1
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <utime.h>
#include <signal.h>
#include <errno.h>
#include <sys/param.h>
#include <dirent.h>
#include <stdlib.h>
#include <locale.h>
#include <langinfo.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/acl.h>

#define	FTYPE(A)	(A.st_mode)
#define	FMODE(A)	(A.st_mode)
#define	UID(A)		(A.st_uid)
#define	GID(A)		(A.st_gid)
#define	IDENTICAL(A, B)	(A.st_dev == B.st_dev && A.st_ino == B.st_ino)
#define	ISBLK(A)	((A.st_mode & S_IFMT) == S_IFBLK)
#define	ISCHR(A)	((A.st_mode & S_IFMT) == S_IFCHR)
#define	ISDIR(A)	((A.st_mode & S_IFMT) == S_IFDIR)
#define	ISFIFO(A)	((A.st_mode & S_IFMT) == S_IFIFO)
#define	ISLNK(A)	((A.st_mode & S_IFMT) == S_IFLNK)
#define	ISREG(A)	((A.st_mode & S_IFMT) == S_IFREG)
#define	ISDEV(A)	((A.st_mode & S_IFMT) == S_IFCHR || \
			(A.st_mode & S_IFMT) == S_IFBLK || \
			(A.st_mode & S_IFMT) == S_IFIFO)

#define	BLKSIZE	4096
#define	PATHSIZE 1024
#define	DOT	"."
#define	DOTDOT	".."
#define	DELIM	'/'
#define	EQ(x, y)	(strcmp(x, y) == 0)
#define	FALSE	0
#define	MODEBITS (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)
#define	TRUE 1
#define	MAXMAPSIZE	(256*1024)	/* map at most 256K */

static char	*dname(char *);
static int	move(char *, char *);
static int	getresp(void);
static int	lnkfil(char *, char *);
static int	cpymve(char *, char *);
static int	chkfiles(char *, char **);
static int	rcopy(char *, char *);
static int	accs_parent(char *, struct stat *);
static int	chg_time(char *, struct stat);
static int	chg_mode(char *, uid_t, gid_t, mode_t);
static int	copydir(char *, char *);
static int	copyspecial(char *);
static void	usage(void);
static void	Perror(char *);
static void	Perror2(char *, char *);
static int	writefile(int, int, char *, char *);
static int	use_stdin(void);

extern	int errno;
extern  char *optarg;
extern	int optind, opterr;
static struct stat s1, s2;
static int cpy = FALSE;
static int mve = FALSE;
static int lnk = FALSE;
static char	*cmd;
static int	silent = 0;
static int	fflg = 0;
static int	iflg = 0;
static int	nflg = 0;
static int	pflg = 0;
static int	Rflg = 0;
static int	rflg = 0;
static int	sflg = 0;
static mode_t	fixmode = (mode_t)0;		/* cleanup mode after copy */
static int	targetexists = 0;
static char	yeschr[SCHAR_MAX + 2];
static char	nochr[SCHAR_MAX + 2];
static int	s1aclcnt;
static aclent_t	*s1aclp = NULL;

void
main(int argc, char *argv[])
{
	register int c, i, r, errflg = 0;
	char target[MAXPATHLEN+1];

	/*
	 * Determine command invoked (mv, cp, or ln)
	 */

	if (cmd = strrchr(argv[0], '/'))
		++cmd;
	else
		cmd = argv[0];

	/*
	 * Set flags based on command.
	 */

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	strncpy(yeschr, nl_langinfo(YESSTR), SCHAR_MAX + 2);
	strncpy(nochr, nl_langinfo(NOSTR), SCHAR_MAX + 2);

	if (EQ(cmd, "mv"))
		mve = TRUE;
	else if (EQ(cmd, "ln"))
		lnk = TRUE;
	else if (EQ(cmd, "cp"))
		cpy = TRUE;
	else {
		fprintf(stderr, gettext("Invalid command name (%s); expecting "
		    "mv, cp, or ln.\n"), cmd);
		exit(1);
	}

	/*
	 * Check for options:
	 * 	cp [-fiprR] file1 [file2 ...] target
	 *	ln [-f] [-n] [-s] file1 [file2 ...] target
	 *	ln [-f] [-n] [-s] file1 [file2 ...]
	 *	mv [-f|i] file1 [file2 ...] target
	 *	mv [-f|i] dir1 target
	 */

	if (cpy) {
		while ((c = getopt(argc, argv, "fiprR")) != EOF)
			switch (c) {
			case 'f':
				fflg++;
				break;
			case 'i':
				iflg++;
				break;
			case 'p':
				pflg++;
				break;
			case 'R':
				Rflg++;
				/*FALLTHROUGH*/
			case 'r':
				rflg++;
				break;
			default:
				errflg++;
			}
	} else if (mve) {
		while ((c = getopt(argc, argv, "fis")) != EOF)
			switch (c) {
			case 'f':
				silent++;
#ifdef XPG4
				iflg = 0;
#endif
				break;
			case 'i':
				iflg++;
#ifdef XPG4
				silent = 0;
#endif
				break;
			default:
				errflg++;
			}
	} else { /* ln */
#ifdef XPG4
		nflg++;
#endif
		while ((c = getopt(argc, argv, "fns")) != EOF)
			switch (c) {
			case 'f':
				silent++;
				break;
			case 'n':
				nflg++;
				break;
			case 's':
				sflg++;
				break;
			default:
				errflg++;
			}
	}

	/*
	 * For BSD compatibility allow - to delimit the end of
	 * options for mv.
	 */
	if (mve && optind < argc && (strcmp(argv[optind], "-") == 0))
		optind++;

	/*
	 * Check for sufficient arguments
	 * or a usage error.
	 */

	argc -= optind;
	argv  = &argv[optind];

	if ((argc < 2 && lnk != TRUE) || (argc < 1 && lnk == TRUE)) {
		(void) fprintf(stderr,
		    gettext("%s: Insufficient arguments (%d)\n"),
		    cmd, argc);
		usage();
	}

	if (errflg != 0)
		usage();

	/*
	 * If there is more than a source and target,
	 * the last argument (the target) must be a directory
	 * which really exists.
	 */

	if (argc > 2) {
		if (stat(argv[argc-1], &s2) < 0) {
			(void) fprintf(stderr,
			    gettext("%s: %s not found\n"),
			    cmd, argv[argc-1]);
			exit(2);
		}

		if (!ISDIR(s2)) {
			(void) fprintf(stderr,
			    gettext("%s: Target %s must be a directory\n"),
			    cmd, argv[argc-1]);
			usage();
		}
	}

	if (strlen(argv[argc-1]) > sizeof (target)) {
		(void) fprintf(stderr,
		gettext("%s: Target %s file name length exceeds MAXPATHLEN"
		    " %d\n"), cmd, argv[argc-1], MAXPATHLEN);
		exit(78);
	}

	if (argc == 1) {
		if (strchr(argv[argc-1], '/') == NULL) {
			(void) strcpy(target, "./");
			(void) strcat(target, argv[argc-1]);
		} else
			(void) strcpy(target, strrchr(argv[argc-1], '/')+1);
	} else {
		(void) strcpy(target, argv[--argc]);
	}

	/*
	 * While target has trailing
	 * DELIM (/), remove them (unless only "/")
	 */
	while (((c = strlen(target)) > 1) && (target[c-1] == DELIM))
		target[c-1] = NULL;

	/*
	 * Perform a multiple argument mv|cp|ln by
	 * multiple invocations of move().
	 */

	r = 0;
	for (i = 0; i < argc; i++)
		r += move(argv[i], target);

	/*
	 * Show errors by nonzero exit code.
	 */

	exit(r?2:0);
}

static int
move(char *source, char *target)
{
	register int last;

	/*
	 * While source has trailing
	 * DELIM (/), remove them (unless only "/")
	 */

	while (((last = strlen(source)) > 1) && (source[last-1] == DELIM))
		source[last-1] = NULL;

	if (lnk)
		return (lnkfil(source, target));
	else
		return (cpymve(source, target));
}


static int
lnkfil(char *source, char *target)
{
	char	*buf = NULL;

	if (sflg) {

		/*
		 * If target is a directory make complete
		 * name of the new symbolic link within that
		 * directory.
		 */

		if ((stat(target, &s2) >= 0) && ISDIR(s2)) {
			if ((buf = (char *)malloc(strlen(target)
			    + strlen(dname(source)) + 4)) == NULL) {
				(void) fprintf(stderr,
				    gettext("%s: Insufficient memory "
					"to %s %s\n"),
				    cmd, cmd, source);
				exit(3);
			}
			(void) sprintf(buf, "%s/%s", target, dname(source));
			target = buf;
		}

		/*
		 * Create a symbolic link to the source.
		 */

		if (symlink(source, target) < 0) {
			(void) fprintf(stderr,
			    gettext("%s: cannot create %s: "),
			    cmd, target);
			perror("");
			if (buf != NULL)
				free(buf);
			return (1);
		}
		if (buf != NULL)
			free(buf);
		return (0);
	}

	switch (chkfiles(source, &target)) {
		case 1: return (1);
		case 2: return (0);
			/* default - fall through */
	}

	/*
	 * Make sure source file is not a directory,
	 * we can't link directories...
	 */

	if (ISDIR(s1)) {
		(void) fprintf(stderr,
		    gettext("%s: %s is a directory\n"), cmd, source);
		return (1);
	}

	/*
	 * hard link, call link() and return.
	 */

	if (link(source, target) < 0) {
		if (errno == EXDEV)
			(void) fprintf(stderr,
			    gettext("%s: %s is on a different file system\n"),
			    cmd, target);
		else {
			(void) fprintf(stderr,
			    gettext("%s: cannot create link %s: "),
			    cmd, target);
			perror("");
		}
		if (buf != NULL)
			free(buf);
		return (1);
	} else {
		if (buf != NULL)
			free(buf);
		return (0);
	}
}


static int
cpymve(char *source, char *target)
{
	int	n;
	int fi, fo;
	int ret = 0;

	switch (chkfiles(source, &target)) {
		case 1: return (1);
		case 2: return (0);
			/* default - fall through */
	}

	/*
	 * If it's a recursive copy and source
	 * is a directory, then call rcopy.
	 */
	if (cpy) {
		if (ISDIR(s1))
			return (copydir(source, target));
		else if (ISDEV(s1) && Rflg)
			return (copyspecial(target));
		else
			goto copy;
	}

	if (mve) {
		if (rename(source, target) >= 0)
			return (0);
		if (errno != EXDEV) {
			if (errno != EACCES && ISDIR(s1)) {
				(void) fprintf(stderr,
				    gettext("%s: %s is a directory\n"),
				    cmd, source);
				return (1);
			}
			(void) fprintf(stderr,
			    gettext("%s: cannot rename %s: "),
			    cmd, source);
			perror("");
			return (1);
		}
		if (ISDIR(s1)) {
			if ((n =  copydir(source, target)) == 0)
				(void) rmdir(source);
			return (n);
		}

		/*
		 * File can't be renamed, try to recreate the symbolic
		 * link or special device, or copy the file wholesale
		 * between file systems.
		 */
		if (ISLNK(s1)) {
			register int	m;
			register mode_t md;
			char symln[MAXPATHLEN + 1];

			if (targetexists && unlink(target) < 0) {
				(void) fprintf(stderr,
				    gettext("%s: cannot unlink %s: "),
				    cmd, target);
				perror("");
				return (1);
			}

			m = readlink(source, symln, sizeof (symln) - 1);
			if (m < 0) {
				Perror(source);
				return (1);
			}
			symln[m] = '\0';

			md = umask(~(s1.st_mode & MODEBITS));
			if (symlink(symln, target) < 0) {
				Perror(target);
				return (1);
			}
			(void) umask(md);
			m = lchown(target, UID(s1), GID(s1));
#ifdef XPG4
			if (m < 0) {
				(void) fprintf(stderr, gettext("%s: cannot"
				    " change owner and group of %s: "),
				    cmd, target);
				perror("");
			}
#endif
			goto cleanup;
		}
		if (ISDEV(s1)) {

			if (targetexists && unlink(target) < 0) {
				(void) fprintf(stderr,
				    gettext("%s: cannot unlink %s: "),
				    cmd, target);
				perror("");
				return (1);
			}

			if (mknod(target, s1.st_mode, s1.st_rdev) < 0) {
				Perror(target);
				return (1);
			}

			(void) chg_mode(target, UID(s1), GID(s1), FMODE(s1));
			(void) chg_time(target, s1);
			goto cleanup;
		}

		if (ISREG(s1)) {
			struct stat	spd;	/* stat of source's parent */
			int	uid;		/* real uid */

			if (accs_parent(source, &spd) == -1)
				goto unlink_fail;

			/*
			* If sticky bit set on source's parent, then move
			* only when : superuser, source's owner, parent's
			* owner, or source is writable. Otherwise, we
			* won't be able to unlink source later and will be
			* left with two links and an error exit from mv.
			*/
			if (spd.st_mode & S_ISVTX && (uid = getuid()) != 0 &&
			    s1.st_uid != uid && spd.st_uid != uid &&
			    access(source, 2) < 0) {
				(void) fprintf(stderr,
				    gettext("%s: cannot unlink %s: "),
				    cmd, source);
				perror("");
				return (1);
			}

			if (ISDIR(s2)) {
				if (targetexists && rmdir(target) < 0) {
					(void) fprintf(stderr,
					    gettext("%s: cannot rmdir %s: "),
					    cmd, target);
					perror("");
					return (1);
				}
			} else {
				if (targetexists && unlink(target) < 0) {
					(void) fprintf(stderr,
					    gettext("%s: cannot unlink %s: "),
					    cmd, target);
					perror("");
					return (1);
				}
			}


copy:
			fi = open(source, O_RDONLY);
			if (fi < 0) {
				(void) fprintf(stderr,
				    gettext("%s: cannot open %s: "),
				    cmd, source);
				perror("");
				return (1);
			}

			fo = creat(target, s1.st_mode & MODEBITS);
			if (fo < 0) {
				/*
				 * If -f and creat() failed, unlink
				 * and try again.
				 */
				if (fflg) {
					(void) unlink(target);
					fo = creat(target,
					    s1.st_mode & MODEBITS);
				}
			}
			if (fo < 0) {
				(void) fprintf(stderr,
				    gettext("%s: cannot create %s: "),
				    cmd, target);
				perror("");
				(void) close(fi);
				return (1);
			} else {
				/* stat the new file, its used below */
				(void) stat(target, &s2);
			}

			/*
			 * If we created a target, set its permissions
			 * to the source before any copying so that any
			 * partially copied file will have the source's
			 * permissions (at most) or umask permissions
			 * whichever is the most restrictive.
			 *
			 * ACL for regular files
			 */

			if (!targetexists || pflg || mve) {
				if (pflg || mve)
					(void) chmod(target, FMODE(s1));
				if (s1aclp != NULL) {
					if ((acl(target, SETACL, s1aclcnt,
					    s1aclp)) < 0) {
						if (pflg || mve) {
							(void) fprintf(stderr,
				"%s: failed to set acl entries on %s\n",
							    cmd, target);
							if (pflg)
								return (1);
						}
						/* else: silent and continue */
					}
				}
			}

			if (writefile(fi, fo, source, target) != 0)
				return (1);

			(void) close(fi);
			if (close(fo) < 0) {
				Perror2(target, "write");
				return (1);
			}
			/*
			 * XPG4: the write system call will clear setgid
			 * and setuid bits, so set them again.
			 */
			if (pflg || mve) {
				if ((ret = chg_mode(target, UID(s1), GID(s1),
				    FMODE(s1))) > 0)
					return (1);
				if ((ret = chg_time(target, s1)) > 0)
					return (1);
			}
			if (cpy)
				return (0);
			goto cleanup;
		}
		(void) fprintf(stderr,
		    gettext("%s: unknown file type %o"), source, s1.st_mode);
		return (1);

cleanup:
		if (unlink(source) < 0) {
			(void) unlink(target);
unlink_fail:
			(void) fprintf(stderr,
			    gettext("%s: cannot unlink %s: "),
			    cmd, source);
			perror("");
			return (1);
		}
		return (ret);
	}
	/*NOTREACHED*/
}

static int
writefile(int fi, int fo, char *source, char *target)
{
	int mapsize, munmapsize;
	caddr_t cp;
	off_t filesize = s1.st_size;
	off_t offset;
	int nbytes;
	int remains;
	int n;
	char buf[8192];

	if (ISREG(s1)) {
		/*
		 * Determine size of initial mapping.  This will determine the
		 * size of the address space chunk we work with.  This initial
		 * mapping size will be used to perform munmap() in the future.
		 */
		mapsize = MAXMAPSIZE;
		if (s1.st_size < mapsize) mapsize = s1.st_size;
		munmapsize = mapsize;

		/*
		 * Mmap time!
		 */
		cp = mmap((caddr_t)NULL, mapsize, PROT_READ,
			MAP_SHARED, fi, (off_t)0);
		if (cp == (caddr_t)-1) mapsize = 0;   /* can't mmap today */
	} else
		mapsize = 0;

	if (mapsize != 0) {
		offset = 0;

		(void) madvise(cp, mapsize, MADV_SEQUENTIAL);
		for (;;) {
			nbytes = write(fo, cp, mapsize);
			/*
			 * if we write less than the mmaped size it's due to a
			 * media error on the input file or out of space on
			 * the output file.  So, try again, and look for errno.
			 */
			if ((nbytes >= 0) && (nbytes != (int)mapsize)) {
				remains = mapsize - nbytes;
				while (remains > 0) {
					nbytes = write(fo,
					    cp + mapsize - remains, remains);
					if (nbytes < 0) {
						if (errno == ENOSPC)
							Perror(target);
						else
							Perror(source);
						(void) close(fi);
						(void) close(fo);
						(void) munmap(cp, munmapsize);
						if (ISREG(s2))
							(void) unlink(target);
						return (1);
					}
					remains -= nbytes;
					if (remains == 0)
						nbytes = mapsize;
				}
			}
			/*
			 * although the write manual page doesn't specify this
			 * as a possible errno, it is set when the nfs read
			 * via the mmap'ed file is accessed, so report the
			 * problem as a source access problem, not a target file
			 * problem
			 */
			if (nbytes < 0) {
				if (errno == EACCES)
					Perror(source);
				else
					Perror(target);
				(void) close(fi);
				(void) close(fo);
				(void) munmap(cp, munmapsize);
				if (ISREG(s2))
					(void) unlink(target);
				return (1);
			}
			filesize -= nbytes;
			if (filesize == 0)
				break;
			offset += nbytes;
			if (filesize < mapsize)
				mapsize = filesize;
			if (mmap(cp, mapsize, PROT_READ, MAP_SHARED | MAP_FIXED,
			    fi, offset) == (caddr_t)-1) {
				Perror(source);
				(void) close(fi);
				(void) close(fo);
				(void) munmap(cp, munmapsize);
				if (ISREG(s2))
					(void) unlink(target);
				return (1);
			}
			(void) madvise(cp, mapsize, MADV_SEQUENTIAL);
		}
		(void) munmap(cp, munmapsize);
	} else {
		for (;;) {
			n = read(fi, buf, sizeof (buf));
			if (n == 0) {
				return (0);
			} else if (n < 0) {
				Perror2(source, "read");
				(void) close(fi);
				(void) close(fo);
				if (ISREG(s2))
					(void) unlink(target);
				return (1);
			} else if (write(fo, buf, n) != n) {
				Perror2(target, "write");
				(void) close(fi);
				(void) close(fo);
				if (ISREG(s2))
					(void) unlink(target);
				return (1);
			}
		}
	}
	return (0);
}


static int
chkfiles(char *source, char **to)
{
	char	*buf = (char *)NULL;
	int	(*statf)() = cpy ? stat : lstat;
	char    *target = *to;

	/*
	 * Make sure source file exists.
	 */

	if ((*statf)(source, &s1) < 0) {
		(void) fprintf(stderr,
		    gettext("%s: cannot access %s\n"), cmd, source);
		return (1);
	}

	/*
	 * Get ACL info: don't bother with ln or mv'ing symlinks
	 */
	if ((!lnk) && !(mve && ISLNK(s1))) {
		if (s1aclp != NULL) {
			free(s1aclp);
			s1aclp = NULL;
		}
		if ((s1aclcnt = acl(source, GETACLCNT, 0, NULL)) < 0) {
			(void) fprintf(stderr,
			    "%s: failed to get acl entries\n", source);
			return (1);
		}
		if (s1aclcnt > MIN_ACL_ENTRIES) {
			if ((s1aclp = (aclent_t *)malloc(
				sizeof (aclent_t) * s1aclcnt)) == NULL) {
				(void) fprintf(stderr, "Insufficient memory\n");
				return (1);
			}
			if ((acl(source, GETACL, s1aclcnt, s1aclp)) < 0) {
				(void) fprintf(stderr,
				    "%s: failed to get acl entries\n", source);
				return (1);
			}
		}
		/* else: just permission bits */
	}

	/*
	 * If stat fails, then the target doesn't exist,
	 * we will create a new target with default file type of regular.
	 */

	FTYPE(s2) = S_IFREG;
	targetexists = 0;
	if ((*statf)(target, &s2) >= 0) {
		if (ISLNK(s2))
			(void) stat(target, &s2);
		/*
		 * If target is a directory,
		 * make complete name of new file
		 * within that directory.
		 */
		if (ISDIR(s2)) {
			if ((buf = (char *)malloc(strlen(target) +
			    strlen(dname(source)) + 4)) == NULL) {
				(void) fprintf(stderr,
				    gettext("%s: Insufficient memory to "
					"%s %s\n "),
				    cmd, cmd, source);
				exit(3);
			}
			(void) sprintf(buf, "%s/%s", target, dname(source));
			*to = target = buf;
		}

		/*
		 * If filenames for the source and target are
		 * the same and the inodes are the same, it is
		 * an error.
		 */

		if ((*statf)(target, &s2) >= 0) {
			int overwrite	= FALSE;
			int override	= FALSE;

			targetexists++;
			if (IDENTICAL(s1, s2)) {
				(void) fprintf(stderr,
				    gettext("%s: %s and %s are identical\n"),
				    cmd, source, target);
				if (buf != NULL)
					free(buf);
				return (1);
			}
			if (lnk && nflg && !silent) {
				(void) fprintf(stderr,
				    gettext("%s: %s: File exists\n"),
				    cmd, target);
				return (1);
			}

			/*
			 * overwrite:
			 * If the user does not have access to
			 * the target, ask ----if it is not
			 * silent and user invoked command
			 * interactively.
			 *
			 * override:
			 * If not silent, and stdin is a terminal, and
			 * there's no write access, and the file isn't a
			 * symbolic link, ask for permission.
			 *
			 * XPG4: both overwrite and override:
			 * ask only one question.
			 *
			 * TRANSLATION_NOTE - The following messages will
			 * contain the first character of the strings for
			 * "yes" and "no" defined in the file
			 * "nl_langinfo.po".  After substitution, the
			 * message will appear as follows:
			 *	<cmd>: overwrite <filename> (y/n)?
			 * where <cmd> is the name of the command
			 * (cp, mv) and <filename> is the destination file
			 */


			overwrite = iflg && !silent && use_stdin();
			override = !cpy && (access(target, 2) < 0) &&
			    !silent && use_stdin() && !ISLNK(s2);

			if (overwrite && override)
				(void) fprintf(stderr,
				    gettext("%s: overwrite %s and override "
				    "protection %o (%s/%s)? "),
				    cmd, target, FMODE(s2) & MODEBITS,
				    yeschr, nochr);
			else if (overwrite && ISREG(s2))
				(void) fprintf(stderr,
				    gettext("%s: overwrite %s (%s/%s)? "),
				    cmd, target, yeschr, nochr);
			else if (override)
				(void) fprintf(stderr,
				    gettext("%s: %s: override protection "
					"%o (%s/%s)? "),
				    cmd, target, FMODE(s2) & MODEBITS,
				    yeschr, nochr);
			if (overwrite || override) {
				if (ISREG(s2)) {
					if (getresp()) {
						if (buf != NULL)
							free(buf);
						return (2);
					}
				}
			}
			if (lnk && unlink(target) < 0) {
				(void) fprintf(stderr,
				    gettext("%s: cannot unlink %s: "),
				    cmd, target);
				perror("");
				return (1);
			}
		}
	}
	return (0);
}

static int
rcopy(char *from, char *to)
{
	DIR *fold = opendir(from);
	struct dirent *dp;
	struct stat statb, s1save;
	int errs = 0;
	char fromname[MAXPATHLEN + 1];

	if (fold == 0 || ((pflg || mve) && fstat(fold->dd_fd, &statb) < 0)) {
		Perror(from);
		return (1);
	}
	if (pflg || mve) {
		/*
		 * Save s1 (stat information for source dir) so that
		 * mod and access times can be reserved during "cp -p"
		 * or mv, since s1 gets overwritten.
		 */
		s1save = s1;
	}
	for (;;) {
		dp = readdir(fold);
		if (dp == 0) {
			(void) closedir(fold);
			if (pflg || mve)
				return (chg_time(to, s1save) + errs);
			return (errs);
		}
		if (dp->d_ino == 0)
			continue;
		if ((strcmp(dp->d_name, ".") == 0) ||
		    (strcmp(dp->d_name, "..") == 0))
			continue;
		if (strlen(from)+1+strlen(dp->d_name) >=
		    sizeof (fromname) - 1) {
			(void) fprintf(stderr,
			    gettext("%s : %s/%s: Name too long\n"),
			    cmd, from, dp->d_name);
			errs++;
			continue;
		}
		(void) sprintf(fromname, "%s/%s", from, dp->d_name);
		errs += move(fromname, to);
	}
}

/* In addition to checking write access on parent dir, do a stat on it */
static int
accs_parent(char *name, struct stat *sdirp)
{
	register int	c;
	register char	*p, *q;
	char *buf;

	/*
	 * Allocate a buffer for parent.
	 */

	if ((buf = malloc(strlen(name) + 2)) == NULL) {
		(void) fprintf(stderr,
		    gettext("%s: Insufficient memory space.\n"), cmd);
		exit(3);
	}
	p = q = buf;

	/*
	 * Copy name into buf and set 'q' to point to the last
	 * delimiter within name.
	 */

	/*LINTED*/
	while (c = *p++ = *name++)
		if (c == DELIM)
			q = p-1;

	/*
	 * If the name had no '\' or was "\" then leave it alone,
	 * otherwise null the name at the last delimiter.
	 */

	if (q == buf && *q == DELIM)
		q++;
	*q = NULL;

	/*
	* Find the access of the parent.
	* If no parent specified, use dot.
	*/

	if ((geteuid() != 0) && (c = access(buf[0] ? buf : DOT, W_OK)) == -1) {
		free(buf);
		return (c);
	}

	/*
	 * Stat the parent : needed for sticky bit check.
	 * If stat fails, move() should fail the access,
	 * since we cannot proceed anyway.
	 */
	c = stat(buf[0] ? buf : DOT, sdirp);
	free(buf);
	return (c);
}

static char *
dname(char *name)
{
	register char *p;

	/*
	 * Return just the file name given the complete path.
	 * Like basename(1).
	 */

	p = name;

	/*
	 * While there are characters left,
	 * set name to start after last
	 * delimiter.
	 */

	while (*p)
		if (*p++ == DELIM && *p)
			name = p;
	return (name);
}

static int
getresp(void)
{
	register int	c, i;
	char	ans_buf[SCHAR_MAX + 1];

	/*
	 * Get response from user. Based on
	 * first character, make decision.
	 * Discard rest of line.
	 */
	for (i = 0; ; i++) {
		c = getchar();
		if (c == '\n' || c == 0 || c == EOF) {
			ans_buf[i] = 0;
			break;
		}
		if (i < SCHAR_MAX)
			ans_buf[i] = c;
	}
	if (i >= SCHAR_MAX) {
		i = SCHAR_MAX;
		ans_buf[SCHAR_MAX] = 0;
	}
	if ((i == 0) | (strncmp(yeschr, ans_buf, i)))
		return (1);
	return (0);
}

static void
usage(void)
{
	/*
	 * Display usage message.
	 */

	if (mve) {
		(void) fprintf(stderr, gettext(
		    "Usage: mv [-f] [-i] f1 f2\n"
		    "       mv [-f] [-i] f1 ... fn d1\n"
		    "       mv [-f] [-i] d1 d2\n"));
	} else if (lnk) {
		(void) fprintf(stderr, gettext(
#ifdef XPG4
		    "Usage: ln [-f] [-s] f1 [f2]\n"
		    "       ln [-f] [-s] f1 ... fn d1\n"
		    "       ln [-f] -s d1 d2\n"));
#else
		    "Usage: ln [-f] [-n] [-s] f1 [f2]\n"
		    "       ln [-f] [-n] [-s] f1 ... fn d1\n"
		    "       ln [-f] [-n] -s d1 d2\n"));
#endif
	} else if (cpy) {
		(void) fprintf(stderr, gettext(
		    "Usage: cp [-f] [-i] [-p] f1 f2\n"
		    "       cp [-f] [-i] [-p] f1 ... fn d1\n"
		    "       cp -r|R [-f] [-i] [-p] d1 ... dn-1 dn\n"));
	}
	exit(2);
}

/*
 * chg_time()
 *
 * Try to preserve modification and access time.
 * If 1) pflg is not set, or 2) pflg is set and this is the Solaris version,
 * don't report a utime() failure.
 * If this is the XPG4 version and utime fails, if 1) pflg is set (cp -p)
 * or 2) we are doing a mv, print a diagnostic message; arrange for a non-zero
 * exit status only if pflg is set.
 */
static int
chg_time(char *to, struct stat ss)
{
	struct utimbuf times;
	int rc;

	times.actime = ss.st_atime;
	times.modtime = ss.st_mtime;

	rc = utime(to, &times);
#ifdef XPG4
	if ((pflg || mve) && rc != 0) {
		(void) fprintf(stderr,
		gettext("%s: cannot set times for %s: "), cmd, to);
		perror("");
		if (pflg)
			return (1);
	}
#endif

	return (0);

}

/*
 * chg_mode()
 *
 * This function is called upon "cp -p" or mv across filesystems.
 *
 * Try to preserve the owner and group id.  If chown() fails,
 * only print a diagnostic message if doing a mv in the XPG4 version;
 * try to clear S_ISUID and S_ISGID bits in the target.  If unable to clear
 * S_ISUID and S_ISGID bits, print a diagnostic message and arrange for a
 * non-zero exit status because this is a security violation.
 * Try to preserve permissions.
 * If this is the XPG4 version and chmod() fails, print a diagnostic message
 * and arrange for a non-zero exit status.
 * If this is the Solaris version and chmod() fails, do not print a
 * diagnostic message or exit with a non-zero value.
 */
static int
chg_mode(char *target, uid_t uid, gid_t gid, mode_t mode)
{
	int clearflg = 0; /* controls message printed upon chown() error */

	if (chown(target, uid, gid) != 0) {
#ifdef XPG4
		if (mve) {
			(void) fprintf(stderr, gettext("%s: cannot change"
			    " owner and group of %s: "), cmd, target);
			perror("");
		}
#endif
		if (mode & (S_ISUID | S_ISGID)) {
			/* try to clear S_ISUID and S_ISGID */
			mode &= ~S_ISUID & ~S_ISGID;
			++clearflg;
		}
	}
	if (chmod(target, mode) != 0) {
		if (clearflg) {
			(void) fprintf(stderr, gettext(
			"%s: cannot clear S_ISUID and S_ISGID bits in %s: "),
			cmd, target);
			perror("");
			/* cp -p should get non-zero exit; mv should not */
			if (pflg)
				return (1);
		}
#ifdef XPG4
		else {
			(void) fprintf(stderr, gettext(
			"%s: cannot set permissions for %s: "), cmd, target);
			perror("");
			/* cp -p should get non-zero exit; mv should not */
			if (pflg)
				return (1);
		}
#endif
	}
	return (0);

}

static void
Perror(char *s)
{
	char buf[MAXPATHLEN + 10];

	(void) sprintf(buf, "%s: %s", cmd, s);
	perror(buf);
}

static void
Perror2(char *s1, char *s2)
{
	char buf[MAXPATHLEN + 20];

	(void) sprintf(buf, "%s: %s: %s", cmd, gettext(s1), gettext(s2));
	perror(buf);
}

/*
 * used for cp -R and for mv across file systems
 */
static int
copydir(char *source, char *target)
{
	int ret;
	int pret = 0;		/* need separate flag if -p is specified */
	struct stat s1save;
	int s1aclcnt_save;
	aclent_t *s1aclp_save = NULL;

	if (cpy && !rflg) {
		(void) fprintf(stderr,
		    gettext("%s: %s: is a directory\n"), cmd, source);
		return (1);
	}

	if (stat(target, &s2) < 0) {
		if (mkdir(target, (s1.st_mode & MODEBITS)) < 0) {
			(void) fprintf(stderr, "%s: ", cmd);
			perror(target);
			return (1);
		}
		if (stat(target, &s2) == 0)
			fixmode = s2.st_mode;
		else
			fixmode = s1.st_mode;
		(void) chmod(target, ((fixmode & MODEBITS) | S_IRWXU));
	} else if (!(ISDIR(s2))) {
		(void) fprintf(stderr,
		    gettext("%s: %s: not a directory.\n"), cmd, target);
		return (1);
	}
	if (pflg || mve) {
		/*
		 * Save s1 (stat information for source dir) and acl info,
		 * if any, so that ownership, modes, times, and acl's can
		 * be reserved during "cp -p" or mv.
		 * s1 gets overwritten when doing the recursive copy.
		 */
		s1save = s1;
		if (s1aclp != NULL) {
			if ((s1aclp_save = (aclent_t *)malloc(sizeof (aclent_t)
			    * s1aclcnt)) != NULL) {
				memcpy(s1aclp_save, s1aclp, sizeof (aclent_t)
				    * s1aclcnt);
				s1aclcnt_save = s1aclcnt;
			}
#ifdef XPG4
			else {
				(void) fprintf(stderr, gettext("%s: "
				    "Insufficient memory to save acl entry\n"),
				    cmd);
				if (pflg)
					return (1);
			}
#endif
		}
	}

	ret = rcopy(source, target);

	/*
	 * Once we created a directory, go ahead and set
	 * its attributes, e.g. acls and time. The info
	 * may get overwritten if we continue traversing
	 * down the tree.
	 *
	 * ACL for directory
	 */
	if (pflg || mve) {
		if (s1aclp_save != NULL) {
			if ((acl(target, SETACL, s1aclcnt_save, s1aclp_save))
			    < 0) {
#ifdef XPG4
				if (pflg || mve) {
#else
				if (pflg) {
#endif
					(void) fprintf(stderr, gettext(
					    "%s: failed to set acl entries "
					    "on %s\n"), cmd, target);
					if (pflg) {
						free(s1aclp_save);
						ret++;
					}
				}
				/* else: silent and continue */
			}
			free(s1aclp_save);
		}
		if ((pret = chg_mode(target, UID(s1save), GID(s1save),
		    FMODE(s1save))) == 0)
			pret = chg_time(target, s1save);
		ret += pret;
	} else if (fixmode != (mode_t)0)
		(void) chmod(target, fixmode & MODEBITS);

	return (ret);
}

static int
copyspecial(char *target)
{
	int ret = 0;

	if (mknod(target, s1.st_mode, s1.st_rdev) != 0) {
		(void) fprintf(stderr, gettext(
		    "cp: cannot create special file %s: "), target);
		perror("");
		return (1);
	}

	if (pflg) {
		if ((ret = chg_mode(target, UID(s1), GID(s1), FMODE(s1))) == 0)
			ret = chg_time(target, s1);
	}

	return (ret);
}

static int
use_stdin(void)
{
#ifdef XPG4
	return (1);
#else
	return (isatty(fileno(stdin)));
#endif
}

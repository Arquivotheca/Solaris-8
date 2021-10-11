/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)proc_dirname.c	1.2	99/12/02 SMI"

#define	_POSIX_PTHREAD_SEMANTICS	/* for readdir_r() declaration */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <alloca.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/param.h>

/* The /a/a/a etc. case requires /../../.. etc. so we allocate 3/2 MAXPATHLEN */
#define	BUF_SIZE	(3 * MAXPATHLEN / 2)

/*
 * Return the full pathname of a named directory without using chdir().
 * This enables us to get the working directory of another process from
 * its /proc/<pid>/cwd alias.  Also, dirname = "." is the getcwd() case.
 */
char *
proc_dirname(const char *dirname, char *buf, size_t size)
{
	struct stat	cdir;	/* current directory status */
	struct stat	tdir;
	struct stat	pdir;	/* parent directory status */
	char		*dotdots;
	char		*dotend;
	DIR		*pdfd;
	struct dirent	*dirbuf;
	struct dirent	*dir;
	size_t		pos;	/* current position of the string in buf */
	size_t		dirlen;
	size_t		len;
	int		err;

	if (size < 2) {
		errno = (size == 0) ? EINVAL : ENAMETOOLONG;
		return (NULL);
	}

	if ((dirlen = strlen(dirname)) == 0) {	/* "" and "." are the same */
		dirname = ".";
		dirlen = 1;
	}

	/*
	 * XXX: Bug in /proc directory handling.
	 * If the 'cwd' directory refers to another directory that is
	 * a mount point, then VOP_LOOKUP() of that directory/..
	 * is defined to return itself so the higher-level code
	 * can traverse the mount point.
	 * That makes /proc/nnn/cwd look like the root directory.
	 *
	 * So, instead of this:
	 * if (stat(dirname, &pdir) != 0)	/ * errno already set * /
	 *	return (NULL);
	 * we do the following:
	 */
	{
		int dfd;
		if ((dfd = open(dirname, O_RDONLY)) < 0 ||
		    fstat(dfd, &pdir) != 0) {
			if (dfd >= 0)
				(void) close(dfd);
			return (NULL);
		}
		(void) close(dfd);
	}

	if (!S_ISDIR(pdir.st_mode)) {
		errno = ENOTDIR;
		return (NULL);
	}

	/* Do one alloca() for both buffers */
	dirbuf = alloca(sizeof (struct dirent) + MAXNAMELEN	/* dirbuf */
		+ dirlen + 1 + BUF_SIZE + MAXNAMELEN);		/* dotdots */
	dotdots = (char *)dirbuf + sizeof (struct dirent) + MAXNAMELEN;

	/* dotdots will become, successively, dirname/.. dirname/../.. etc */
	(void) strcpy(dotdots, dirname);
	dotend = dotdots + dirlen;

	/* XXX: more of the kludge workaround for the bug described above. */
	*dotend++ = '/';
	*dotend++ = '.';

	*dotend++ = '/';
	*dotend++ = '.';
	*dotend++ = '.';
	*dotend = '\0';

	/* We build up the string from the end, prepending each new component */
	pos = size - 1;		/* starting position in the output buffer */
	buf[pos] = '\0';	/* starting value is the null string */

	for (;;) {
		/* update current directory */
		cdir = pdir;

		/* open parent directory */
		if ((pdfd = opendir(dotdots)) == NULL ||
		    fstat(pdfd->dd_fd, &pdir) != 0) {
			if (pdfd != NULL)
				(void) closedir(pdfd);
			/* errno set by opendir() or fstat() */
			break;
		}

		/*
		 * Find subdirectory of parent that matches current directory.
		 */
		if (cdir.st_dev == pdir.st_dev) {
			/*
			 * Curent and parent directories are on the same
			 * filesystem.  We just have to compare i-numbers.
			 */
			if (cdir.st_ino == pdir.st_ino) {
				/* at root */
				(void) closedir(pdfd);
				if (pos == (size - 1))	/* cwd is "/" */
					buf[--pos] = '/';
				(void) memmove(buf, &buf[pos], size - pos);
				return (buf);
			}
			for (;;) {
				err = readdir_r(pdfd, dirbuf, &dir);
				if (err != 0) {
					(void) closedir(pdfd);
					if (err != -1)
						errno = err;
					goto out;
				}
				if (dir == NULL) {
					(void) closedir(pdfd);
					errno = ENOENT;
					goto out;
				}
				if (dir->d_ino == cdir.st_ino)
					break;
			}
		} else {
			/*
			 * Curent and parent directories are on different
			 * filesystems.  We have to do stat()s and compare
			 * both device numbers and i-numbers.  Expensive.
			 */
			*dotend = '/';
			for (;;) {
				err = readdir_r(pdfd, dirbuf, &dir);
				if (err != 0) {
					(void) closedir(pdfd);
					if (err != -1)
						errno = err;
					goto out;
				}
				if (dir == NULL) {
					(void) closedir(pdfd);
					errno = ENOENT;
					goto out;
				}
				/* Skip "." and ".." */
				if (dir->d_name[0] == '.' &&
				    (dir->d_name[1] == '\0' ||
				    (dir->d_name[1] == '.' &&
				    dir->d_name[2] == '\0')))
					continue;
				(void) strcpy(dotend + 1, dir->d_name);
				/*
				 * Ignore non-stat'able entries.
				 */
				if (stat(dotdots, &tdir) == 0 &&
				    tdir.st_dev == cdir.st_dev &&
				    tdir.st_ino == cdir.st_ino)
					break;
			}
		}
		(void) closedir(pdfd);

		len = strlen(dir->d_name);

		if (len + 1 > pos) {
			errno = ENAMETOOLONG;
			break;
		}

		/* copy name of current directory into buf */
		pos -= len;
		(void) strncpy(&buf[pos], dir->d_name, len);
		buf[--pos] = '/';

		if (dotend + 3 > dotdots + dirlen + 1 + BUF_SIZE) {
			errno = ENAMETOOLONG;
			break;
		}

		/* update dotdots to parent directory */
		*dotend++ = '/';
		*dotend++ = '.';
		*dotend++ = '.';
		*dotend = '\0';
	}
out:
	return (NULL);
}

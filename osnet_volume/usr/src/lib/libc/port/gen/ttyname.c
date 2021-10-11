/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ttyname.c	1.24	98/03/24 SMI"	/* SVr4.0 1.23 */

/*LINTLIBRARY*/
/*
 * ttyname(f): return "/dev/X" (where X is a relative pathname
 * under /dev/), which is the name of the tty character special
 * file associated with the file descriptor f, or NULL if the
 * pathname cannot be found.
 *
 * Ttyname tries to find the tty file by matching major/minor
 * device, file system ID, and inode numbers of the file
 * descriptor parameter to those of files in the /dev/ directory.
 *
 * It attempts to find a match on major/minor device numbers,
 * file system ID, and inode numbers, but failing to match on
 * all three, settles for just a match on device numbers and
 * file system ID.
 *
 * To achieve higher performance and more flexible functionality,
 * ttyname first looks for the tty file in the directories specified
 * in the configuration file /etc/ttysrch. Entries in /etc/ttysrch
 * may be qualified to specify that a partial match may be acceptable.
 * This further improves performance by allowing an entry which
 * matches major/minor and file system ID, but not inode number
 * without searching the entire /dev tree. If /etc/ttysrch does not
 * exist, ttyname looks in a default list of directories.  If after
 * looking in the most likely places, ttyname still cannot find the
 * tty file, it recursively searches thru the rest of the /dev/
 * directory.
 */

#pragma weak ttyname = _ttyname
#ifdef _REENTRANT
#pragma weak ttyname_r = _ttyname_r
#endif /* _REENTRANT */

#include "synonyms.h"
#include "shlib.h"
#include <mtlib.h>
#include "_libc_gettext.h"
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <thread.h>
#include <synch.h>
#include <errno.h>
#include <limits.h>

typedef struct entry {
	char *name;
	int flags;
} entry_t;

static int srch_dir(entry_t path, int depth, entry_t skip_dirs[],
    struct stat64 *fsb);
static entry_t *get_pri_dirs(void);
static char *ispts(struct stat64 *fsb);
static void itoa(int i, char *ptr);

#define	MAX_DEV_PATH	TTYNAME_MAX
#define	MAX_SRCH_DEPTH	4

#define	MATCH_MM	1
#define	MATCH_FS	2
#define	MATCH_INO	4
#define	MATCH_ALL	7

#define	DEV		"/dev"
#define	TTYSRCH		"/etc/ttysrch"
#define	PTS		"/dev/pts"

static  entry_t dev_dir =
	{ "/dev", MATCH_ALL };

static  entry_t def_srch_dirs[] = {	/* default search list */
	{ "/dev/term", MATCH_ALL },
	{ "/dev/pts", MATCH_ALL },
	{ "/dev/xt", MATCH_ALL },
	{ NULL, 0 }
};

static char *dir_buf;		/* directory buffer for ttysrch body */
static entry_t *dir_vec;	/* directory vector for ttysrch ptrs */
static char rbuf[MAX_DEV_PATH]; /* perfect match file name */
static char dev_rbuf[MAX_DEV_PATH]; /* partial match file name */
static int dev_flag;		/* if set, dev + rdev match was found */
static char *special_case[] = {
	"/dev/tty",
	"/dev/console",
	"/dev/conslog",
	"/dev/syscon",
	"/dev/systty",
	"/dev/wscons",
};
#define	NUMSPECIAL	(sizeof (special_case) / sizeof (char *))

#ifdef _REENTRANT
static mutex_t tty_lock = DEFAULTMUTEX;
#endif _REENTRANT

/*
 * POSIX.1c Draft-6 version of the function ttyname_r.
 * It was implemented by Solaris 2.3.
 */
char *
_ttyname_r(int f, char *buffer, int buflen)
{
	struct stat64 fsb;	/* what we are searching for */
	struct stat64 tfsb;
	entry_t *srch_dirs;	/* priority directories */
	int i;
	int found = 0;
	int dirno = 0;
	int is_pts = 0;
	char *retval = 0;
	char *pt = 0;

	/*
	 * do we need to search anything at all? (is fildes a char special tty
	 * file?)
	 */
	if (fstat64(f, &fsb) < 0) {
		errno = EBADF;
		return (0);
	}
	if ((isatty(f) == 0) ||
	    ((fsb.st_mode & S_IFMT) != S_IFCHR)) {
		errno = ENOTTY;
		return (0);
	}

	if (buflen < MAX_DEV_PATH) {
		errno = ERANGE;
		return (0);
	}

	(void) _mutex_lock(&tty_lock);


	/*
	 * match special cases first before searching other directories
	 */

	for (i = 0; i < NUMSPECIAL; i++) {
		if (stat64(special_case[i], &tfsb) == 0) {
			if (tfsb.st_dev == fsb.st_dev &&
				tfsb.st_rdev == fsb.st_rdev &&
				tfsb.st_ino == fsb.st_ino) {
					retval = special_case[i];
					goto out;
			}
		}
	}

	/*
	* search the priority directories
	*/


	srch_dirs = get_pri_dirs();
	dev_flag = 0;

	while ((!found) && (srch_dirs[dirno].name != NULL)) {

		/*
		* if /dev is one of the priority directories, only
		* search its top level(set depth = MAX_SEARCH_DEPTH)
		*/

		/*
		* Is /dev/pts then just do a quick check. We don't have
		* to stat the entire /dev/pts dir.
		*/
		if (strcmp(PTS, srch_dirs[dirno].name) == NULL) {
			if ((pt = ispts(&fsb)) != NULL) {
				is_pts = 1;
				found = 1;
			}
		} else {
			found = srch_dir(srch_dirs[dirno],
				((strcmp(srch_dirs[dirno].name,
				dev_dir.name) == 0) ?
				MAX_SRCH_DEPTH : 1), 0, &fsb);
		}
		dirno++;
	}

	/*
	* search the /dev/ directory, skipping priority directories
	*/
	if (!found)
		found = srch_dir(dev_dir, 0, srch_dirs, &fsb);

	if (dir_buf != NULL) {
		free(dir_buf);
		dir_buf = NULL;
	}
	if (dir_vec != NULL) {
		free(dir_vec);
		dir_vec = NULL;
	}

	/*
	 * return
	 */

	if (found) {
		if (is_pts)
			retval = pt;
		else
			retval = rbuf;
	} else if (dev_flag)
		retval = dev_rbuf;
	else
		retval = NULL;
out:	retval = (retval ? strcpy(buffer, retval) : NULL);
	(void) _mutex_unlock(&tty_lock);
	return (retval);
}

/*
 * POSIX.1c standard version of the function ttyname_r.
 * User gets it via static ttyname_r from the header file.
 */
int
__posix_ttyname_r(int fildes, char *name, size_t namesize)
{
	int nerrno = 0;
	int oerrno = errno;
	int namelen;

	errno = 0;

	if (namesize > INT_MAX)
		namelen = INT_MAX;
	else
		namelen = (int)namesize;

	if (_ttyname_r(fildes, name, namelen) == NULL) {
		if (errno == 0)
			nerrno = EINVAL;
		else
			nerrno = errno;
	}
	errno = oerrno;
	return (nerrno);
}

/*
 * Checks if the name is under /dev/pts directory
 */
static char *
ispts(struct stat64 *fsb)
{
	static  char	buf[MAX_DEV_PATH];
	struct	stat64	stb;

	(void) strcpy(buf, "/dev/pts/");
	itoa(geteminor(fsb->st_rdev), buf+strlen(buf));

	if (stat64(buf, &stb) != 0)
		return (NULL);

	if (stb.st_rdev == fsb->st_rdev &&
		stb.st_dev == fsb->st_dev &&
		stb.st_ino == fsb->st_ino)
		return (buf);
	else
		return (NULL);
}


/*
 * Converts a number to a string (null terminated).
 */
static void
itoa(int i, char *ptr)
{
	int dig = 0;
	int tempi;

	tempi = i;
	do {
		dig++;
		tempi /= 10;
	} while (tempi);

	ptr += dig;
	*ptr = '\0';
	while (--dig >= 0) {
		*(--ptr) = i % 10 + '0';
		i /= 10;
	}
}

/*
 * srch_dir() searches directory path and all directories under it up
 * to depth directories deep for a file described by a stat structure
 * fsb.  It puts the answer into rbuf.  If a match is found on device
 * number only, the file name is put into dev_rbuf and dev_flag is set.
 *
 * srch_dir() returns 1 if a match (on device and inode) is found,
 * or 0 otherwise.
 *
 */

static int
srch_dir(entry_t path,		/* current path */
	int depth,		/* current depth (/dev = 0) */
	entry_t skip_dirs[],	/* directories not needing searching */
	struct stat64 *fsb)	/* the file being searched for */
{
	DIR *dirp;
	struct dirent64 *direntp;
	struct stat64 tsb;
	char file_name[MAX_DEV_PATH];
	entry_t file;
	char *last_comp;
	int found = 0;
	int dirno = 0;
	size_t path_len;

	file.name = file_name;
	file.flags = path.flags;

	/*
	 * do we need to search this directory? (always search /dev at depth 0)
	 */
	if ((skip_dirs != NULL) && (depth != 0))
		while (skip_dirs[dirno].name != NULL)
			if (strcmp(skip_dirs[dirno++].name, path.name) == 0)
				return (0);

	/*
	 * open directory
	 */
	if ((dirp = opendir(path.name)) == NULL) {
		return (0);
	}

	/*
	 * skip two first entries ('.' and '..')
	 */
	if (((direntp = readdir64(dirp)) == NULL) ||
	    ((direntp = readdir64(dirp)) == NULL)) {
		(void) closedir(dirp);
		return (0);
	}

	path_len = strlen(path.name);
	(void) strcpy(file_name, path.name);
	last_comp = file_name + path_len;
	*last_comp++ = '/';

	/*
	 * read thru the directory
	 */
	while ((!found) && ((direntp = readdir64(dirp)) != NULL)) {

		/*
		 * if the file name (path + "/" + d_name + NULL) would be too
		 * long, skip it
		 */
		if ((path_len + strlen(direntp->d_name) + 2) > MAX_DEV_PATH)
			continue;
		else
			(void) strcpy(last_comp, direntp->d_name);

		if (stat64(file_name, &tsb) < 0)
			continue;

		/*
		 * if a file is a directory and we are not too deep, recurse
		 */
		if ((tsb.st_mode & S_IFMT) == S_IFDIR)
			if (depth < MAX_SRCH_DEPTH)
				found = srch_dir(file, depth+1, skip_dirs, fsb);
			else
				continue;

		/*
		 * else if it is not a directory, is it a character special
		 * file?
		 */
		else if ((tsb.st_mode & S_IFMT) == S_IFCHR) {
			int flag = 0;
			if (tsb.st_dev == fsb->st_dev)
				flag |= MATCH_FS;
			if (tsb.st_rdev == fsb->st_rdev)
				flag |= MATCH_MM;
			if (tsb.st_ino == fsb->st_ino)
				flag |= MATCH_INO;

			if ((flag & file.flags) == file.flags) {
				(void) strcpy(rbuf, file.name);
				found = 1;
			} else if ((flag & (MATCH_MM | MATCH_FS)) ==
					(MATCH_MM | MATCH_FS)) {

				/*
				 * no (inodes do not match), but save the name
				 * for later
				 */
				(void) strcpy(dev_rbuf, file.name);
				dev_flag = 1;
			}
		}
	}
	(void) closedir(dirp);
	return (found);
}


/*
 * get_pri_dirs() - returns a pointer to an array of strings, where each string
 * is a priority directory name.  The end of the array is marked by a NULL
 * pointer.  The priority directories' names are obtained from the file
 * /etc/ttysrch if it exists and is readable, or if not, a default hard-coded
 * list of directories.
 *
 * /etc/ttysrch, if used, is read in as a string of characters into memory and
 * then parsed into strings of priority directory names, omitting comments and
 * blank lines.
 *
 */

#define	START_STATE	1
#define	COMMENT_STATE	2
#define	DIRNAME_STATE	3
#define	FLAG_STATE	4
#define	CHECK_STATE	5

#define	COMMENT_CHAR	'#'
#define	EOLN_CHAR	'\n'

static entry_t *
get_pri_dirs(void)
{
	int fd, state;
	size_t sz;
	ssize_t size;
	struct stat64 sb;
	char *buf, *ebuf;
	entry_t *vec;

	/*
	 * if no /etc/ttysrch, use defaults
	 */
	if ((fd = open(TTYSRCH, 0)) < 0 || stat64(TTYSRCH, &sb) < 0)
		return (def_srch_dirs);
	sz = (size_t)sb.st_size;
	if ((dir_buf = malloc(sz + 1)) == NULL ||
	    (size = read(fd, dir_buf, sz)) < 0) {
		(void) close(fd);
		return (def_srch_dirs);
	}
	(void) close(fd);

	/*
	 * ensure newline termination for buffer.  Add an extra
	 * entry to dir_vec for null terminator
	 */
	ebuf = &dir_buf[size];
	*ebuf++ = '\n';
	for (sz = 1, buf = dir_buf; buf < ebuf; ++buf)
		if (*buf == '\n')
			++sz;
	if ((dir_vec = (entry_t *)malloc(sz * sizeof (*dir_vec))) == NULL)
		return (def_srch_dirs);

	state = START_STATE;
	for (buf = dir_buf, vec = dir_vec; buf < ebuf; ++buf) {
		switch (state) {

		case START_STATE:
			if (*buf == COMMENT_CHAR) {
				state = COMMENT_STATE;
				break;
			}
			if (!isspace(*buf))	/* skip leading white space */
				state = DIRNAME_STATE;
				vec->name = buf;
				vec->flags = 0;
			break;

		case COMMENT_STATE:
			if (*buf == EOLN_CHAR)
				state = START_STATE;
			break;

		case DIRNAME_STATE:
			if (*buf == EOLN_CHAR) {
				state = CHECK_STATE;
				*buf = '\0';
			} else if (isspace(*buf)) {
				/* skip trailing white space */
				state = FLAG_STATE;
				*buf = '\0';
			}
			break;

		case FLAG_STATE:
			switch (*buf) {
				case 'M':
					vec->flags |= MATCH_MM;
					break;
				case 'F':
					vec->flags |= MATCH_FS;
					break;
				case 'I':
					vec->flags |= MATCH_INO;
					break;
				case EOLN_CHAR:
					state = CHECK_STATE;
					break;
			}
			break;

		case CHECK_STATE:
			if (strncmp(vec->name, DEV, strlen(DEV)) != 0) {
				int tfd = open("/dev/console", O_WRONLY);
				if (tfd >= 0) {
					char buf[256];
					(void) sprintf(buf, _libc_gettext(
"ERROR: Entry '%s' in /etc/ttysrch ignored.\n"),
						vec->name);
					(void) write(tfd, buf, strlen(buf));
					(void) close(tfd);
				}
			} else {
				char *slash;
				slash = vec->name + strlen(vec->name) - 1;
				while (*slash == '/')
					*slash-- = '\0';
				if (vec->flags == 0)
					vec->flags = MATCH_ALL;
				vec++;
			}
			state = START_STATE;
			/*
			 * This state does not consume a character, so
			 * reposition the pointer.
			 */
			buf--;
			break;

		}
	}
	vec->name = NULL;
	return ((entry_t *)dir_vec);
}


char *
ttyname(int f)
{
	static char ans[MAX_DEV_PATH];

	return (_ttyname_r(f, ans, MAX_DEV_PATH));
}

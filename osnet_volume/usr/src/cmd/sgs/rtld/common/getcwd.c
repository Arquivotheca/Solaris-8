/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)getcwd.c	1.6	98/08/28 SMI"

#include	"_synonyms.h"
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<dirent.h>
#include	<limits.h>
#include	<stdlib.h>
#include	<strings.h>
#include	"rtld.h"
#include	"msg.h"

static struct stat	status;

/*
 * Included here (instead of using libc's) to reduce the number of stat calls.
 */
static DIR *
_opendir(const char * file)
{
	DIR * 	dirp;
	int	fd;

	if ((fd = open(file, (O_RDONLY | O_NDELAY), 0)) < 0)
		return (0);

	if ((fstat(fd, &status) < 0) ||
	    ((status.st_mode & S_IFMT) != S_IFDIR) ||
	    ((dirp = (DIR *)malloc(sizeof (DIR) + DIRBUF)) == NULL)) {
		(void) close(fd);
		return (0);
	}

	dirp->dd_buf = (char *)dirp + sizeof (DIR);
	dirp->dd_fd = fd;
	dirp->dd_loc = dirp->dd_size = 0;

	return (dirp);
}

static struct dirent *
_readdir(DIR * dirp)
{
	struct dirent * denp;
	int		saveloc = 0;

	if (dirp->dd_size != 0) {
		/* LINTED */
		denp = (struct dirent *)&dirp->dd_buf[dirp->dd_loc];
		saveloc = dirp->dd_loc;
		dirp->dd_loc += denp->d_reclen;
	}
	if (dirp->dd_loc >= dirp->dd_size)
		dirp->dd_loc = dirp->dd_size = 0;

	if ((dirp->dd_size == 0) &&
	    ((dirp->dd_size = getdents(dirp->dd_fd,
	    /* LINTED */
	    (struct dirent *)dirp->dd_buf, DIRBUF)) <= 0)) {
		if (dirp->dd_size == 0)
			dirp->dd_loc = saveloc;
		return (0);
	}

	/* LINTED */
	denp = (struct dirent *)&dirp->dd_buf[dirp->dd_loc];

	return (denp);
}

static int
_closedir(DIR * dirp)
{
	int 	fd = dirp->dd_fd;

	free((char *)dirp);
	return (close(fd));
}

/*
 * Simplified getcwd(3C), stolen from raf's proc(1) pwdx.
 */
static char *
_getcwd(char * path, size_t pathsz)
{
	char		_path[PATH_MAX], cwd[PATH_MAX];
	size_t		cwdsz;
	ino_t		cino;
	dev_t		cdev;

	_path[--pathsz] = '\0';

	/*
	 * Stat the present working directory to establish the initial device
	 * and inode pair.
	 */
	(void) strcpy(cwd, MSG_ORIG(MSG_FMT_CWD));
	cwdsz = MSG_FMT_CWD_SIZE;

	if (stat(cwd, &status) == -1)
		return (NULL);

	/* LINTED */
	while (1) {
		DIR *		dirp;
		struct dirent *	denp;
		size_t		len;

		cino = status.st_ino;
		cdev = status.st_dev;

		/*
		 * Open parent directory
		 */
		(void) strcpy(&cwd[cwdsz], MSG_ORIG(MSG_FMT_PARENT));
		cwdsz += MSG_FMT_PARENT_SIZE;

		if ((dirp = _opendir(cwd)) == 0)
			return (NULL);

		/*
		 * Find subdirectory of parent that matches current directory.
		 */
		if (cdev == status.st_dev) {
			if (cino == status.st_ino) {
				/*
				 * At root, return the pathname we've
				 * established.
				 */
				(void) _closedir(dirp);
				(void) strcpy(path, &_path[pathsz]);
				return (path);
			}

			do {
				if ((denp = _readdir(dirp)) == NULL) {
					(void) _closedir(dirp);
					return (NULL);
				}
			} while (denp->d_ino != cino);

		} else {
			/*
			 * The parent director is a different filesystem, so
			 * determine filenames of subdirectories and stat.
			 */
			struct stat	_status;

			cwd[cwdsz] = '/';

			/* LINTED */
			while (1) {
				if ((denp = _readdir(dirp)) == NULL) {
					(void) _closedir(dirp);
					return (NULL);
				}
				if (denp->d_name[0] == '.') {
					if (denp->d_name[1] == '\0')
						continue;
					if (denp->d_name[1] == '.' &&
					    denp->d_name[2] == '\0')
						continue;
				}
				(void) strcpy(&cwd[cwdsz + 1], denp->d_name);

				/*
				 * Silently ignore non-stat'able entries.
				 */
				if (stat(cwd, &_status) == -1)
					continue;

				if ((_status.st_ino == cino) &&
				    (_status.st_dev == cdev))
					break;
			}
		}

		/*
		 * Copy name of current directory into pathname.
		 */
		if ((len = strlen(denp->d_name)) < pathsz) {
			pathsz -= len;
			(void) strncpy(&_path[pathsz], denp->d_name, len);
			_path[--pathsz] = '/';
		}
		(void) _closedir(dirp);
	}

	return (NULL);
}

/*
 * Take the given link-map file/pathname and prepend the current working
 * directory.
 */
size_t
fullpath(Rt_map * lmp)
{
	char *	name = (char *)PATHNAME(lmp), * str, path[PATH_MAX];
	size_t	size = strlen(name);

	if (name[0] != '/') {
		if ((_getcwd(path, PATH_MAX) != NULL) &&
		    ((size = (strlen(path) + 2 + size)) < PATH_MAX)) {
			(void) strcat(path, MSG_ORIG(MSG_STR_SLASH));
			(void) strcat(path, name);
		} else {
			size += MSG_FMT_CWD_SIZE + 1;
			(void) strcpy(path, MSG_ORIG(MSG_FMT_CWD));
			(void) strcat(path, name);
		}

		/*
		 * Assign the new string to storage.
		 */
		if ((str = (char *)malloc(size + 1)) != 0) {
			(void) strcpy(str, path);
			PATHNAME(lmp) = name = str;
		}
	}

	/*
	 * Establish the directory name size - this also acts as a flag that the
	 * directory name has been computed.
	 */
	str = strrchr(name, '/');
	DIRSZ(lmp) = size = str - name;

	return (size);
}

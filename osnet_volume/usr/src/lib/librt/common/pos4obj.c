/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)pos4obj.c	1.15	98/12/22 SMI"

/*LINTLIBRARY*/

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <pthread.h>
#include <thread.h>
#include <string.h>
#include <dirent.h>
#include "pos4.h"
#include "pos4obj.h"

static	char	*__pos4obj_name(const char *, const char *);

static	char	objroot[] = "/var/tmp/";
static	long int	name_max = 0;

int
__open_nc(const char *path, int oflag, mode_t mode)
{
	int	canstate, val;

	if (_thr_main() != -1)
		(void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,
						&canstate);

	val = open(path, oflag, mode);

	if (_thr_main() != -1)
		(void) pthread_setcancelstate(canstate, &canstate);

	return (val);
}

int
__close_nc(int fildes)
{
	int	canstate, val;

	if (_thr_main() != -1)
		(void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,
						&canstate);

	val = close(fildes);

	if (_thr_main() != -1)
		(void) pthread_setcancelstate(canstate, &canstate);

	return (val);
}


static char	*
__pos4obj_name(const char *path, const char *type)
{
	size_t	len;
	struct stat	statbuf;
	char	*dfile;

	len = strlen(objroot) + strlen(type) + strlen(path) +1;

	if ((dfile = malloc(len)) == NULL)
		return (NULL);

	(void) memset(dfile, 0, len);
	(void) strcpy(dfile, objroot);
	(void) strcat(dfile, type);

	/*
	 * if the directory where the file will reside does not exists
	 * create it. make the directory with permission 777 so that
	 * everyone can use it. in the event of failure free dfile so
	 * the caller does not have to do it.
	 */
	if (stat(dfile, &statbuf) == -1) {
		if (errno == ENOENT) {
			if (mkdir(dfile, S_IRWXU|S_IRWXG|S_IRWXO) == -1) {
				free(dfile);
				return (NULL);
			}
			if (chmod(dfile, S_IRWXU|S_IRWXG|S_IRWXO) == -1) {
				free(dfile);
				return (NULL);
			}
		} else {
			free(dfile);
			return (NULL);
		}
	}

	(void) strcat(dfile, path);
	return (dfile);
}

/*
 * This open function assume that there is no simultaneous
 * open/unlink operation is going on. The caller is supposed
 * to ensure that both open in O_CREAT mode happen atomically.
 * It returns the crflag as 1 if file is created else 0.
 */
int
__pos4obj_open(const char *name, char *type, int oflag,
		mode_t mode, int *crflag)
{
	int fd;
	char *dfile;

	errno = 0;
	*crflag = 0;

	if ((dfile = __pos4obj_name(name, type)) == NULL) {
		return (-1);
	}

	if (!(oflag & O_CREAT)) {
		fd = __open_nc(dfile, oflag, mode);
		free(dfile);
		return (fd);
	}

	/*
	 * We need to make sure that crflag is set iff we actually create
	 * the file.  We do this by or'ing in O_EXCL, and attempting an
	 * open.  If that fails with an EEXIST, and O_EXCL wasn't specified
	 * by the caller, then the file seems to exist;  we'll try an
	 * open with O_CREAT cleared.  If that succeeds, then the file
	 * did indeed exist.  If that fails with an ENOENT, however, the
	 * file was removed between the opens;  we need to take another
	 * lap.
	 */
	for (;;) {
		if ((fd = __open_nc(dfile, (oflag | O_EXCL), mode)) == -1) {
			if (errno == EEXIST && !(oflag & O_EXCL)) {
				fd = __open_nc(dfile, oflag & ~O_CREAT, mode);

				if (fd == -1 && errno == ENOENT)
					continue;
				break;
			}
		} else {
			*crflag = 1;
		}
		break;
	}

	free(dfile);
	return (fd);
}


int
__pos4obj_unlink(const char *name, const char *type)
{
	int	err;
	char	*dfile;

	if ((dfile = __pos4obj_name(name, type)) == NULL) {
		return (-1);
	}

	err = unlink(dfile);

	free(dfile);

	return (err);
}

/*
 * This function opens the lock file for each named object
 * the presence of this file in the file system is the lock
 */
int
__pos4obj_lock(const char *name, const char *ltype)
{
	char	*dfile;
	int	fd;
	int	limit = 64;

	if ((dfile = __pos4obj_name(name, ltype)) == NULL) {
		return (-1);
	}

	while (limit-- > 0) {
		if ((fd = __open_nc(dfile, O_RDWR | O_CREAT | O_EXCL, 0666))
		    < 0) {
			sleep(1);
			continue;
		}

		(void) __close_nc(fd);
		free(dfile);
		return (1);
	}

	free(dfile);
	return (-1);
}

/*
 * Unlocks the file by unlinking it from the filesystem
 */
int
__pos4obj_unlock(const char *path, const char *type)
{
	return (__pos4obj_unlink(path, type));
}

/*
 * Check that path starts with a /, does not contain a / within it
 * and is not longer than PATH_MAX or NAME_MAX
 */
int
__pos4obj_check(const char *path)
{
	long int	i;

	/*
	 * This assumes that __pos4obj_check() is called before
	 * any of the other functions in this file
	 */
	if (name_max == 0 || name_max == -1) {
		name_max = pathconf(objroot, _PC_NAME_MAX);
		if (name_max == -1)
			return (-1);
	}

	if (*path++ != '/') {
		errno = EINVAL;
		return (-1);
	}

	for (i = 0; *path != '\0'; i++) {
		if (*path++ == '/') {
			errno = EINVAL;
			return (-1);
		}
	}

	if (i > PATH_MAX || i > name_max) {
		errno = ENAMETOOLONG;
		return (-1);
	}

	return (0);
}

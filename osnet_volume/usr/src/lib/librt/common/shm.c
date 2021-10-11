/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)shm.c	1.9	98/04/28	SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include "pos4obj.h"

int
shm_open(const char *path, int oflag, mode_t mode)
{
	int flag, fd, flags;

	if (__pos4obj_check(path) == -1) {
		return (-1);
	}

	/* acquire semaphore lock to have atomic operation */
	if ((__pos4obj_lock(path, SHM_LOCK_TYPE)) < 0) {
		return (-1);
	}

	fd = __pos4obj_open(path, SHM_DATA_TYPE, oflag, mode, &flag);

	if (fd < 0) {
		(void) __pos4obj_unlock(path, SHM_LOCK_TYPE);
		return (-1);
	}


	if ((flags = fcntl(fd, F_GETFD)) < 0) {
		(void) __pos4obj_unlock(path, SHM_LOCK_TYPE);
		return (-1);
	}

	if ((fcntl(fd, F_SETFD, flags|FD_CLOEXEC)) < 0) {
		(void) __pos4obj_unlock(path, SHM_LOCK_TYPE);
		return (-1);
	}

	/* relase semaphore lock operation */
	if (__pos4obj_unlock(path, SHM_LOCK_TYPE) < 0) {
		return (-1);
	}
	return (fd);

}

int
shm_unlink(const char *path)
{
	int	oerrno;
	int	err;

	if (__pos4obj_check(path) < 0)
		return (-1);

	if (__pos4obj_lock(path, SHM_LOCK_TYPE) < 0)
		return (-1);

	err = __pos4obj_unlink(path, SHM_DATA_TYPE);

	oerrno = errno;

	(void) __pos4obj_unlock(path, SHM_LOCK_TYPE);

	errno = oerrno;
	return (err);

}

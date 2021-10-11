/*	Copyright (c) 1993-1999 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)sem.c	1.17	99/12/06	SMI"

#pragma weak	sem_open = _sem_open
#pragma weak	sem_close = _sem_close
#pragma weak	sem_unlink = _sem_unlink
#pragma weak	sem_init = _sem_init
#pragma weak	sem_destroy = _sem_destroy
#pragma weak	sem_wait = _sem_wait
#pragma weak	sem_trywait = _sem_trywait
#pragma weak	sem_post = _sem_post
#pragma weak	sem_getvalue = _sem_getvalue

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <semaphore.h>
#include <synch.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <thread.h>
#include "pos4obj.h"
#include "pos4.h"

#pragma weak	_libthread_sema_wait
extern	int	_libthread_sema_wait(sema_t *);

typedef	struct	semaddr {
	struct	semaddr	*sad_next;	/* next in the link */
	char		sad_name[PATH_MAX + 1]; /* name of sem object */
	sem_t		*sad_addr;	/* mmapped address of semaphore */
	ino_t		sad_inode;	/* inode # of the mmapped file */
} semaddr_t;

static	semaddr_t	*semheadp = NULL;
static	mutex_t		semlock = DEFAULTMUTEX;

sem_t *
_sem_open(const char *path, int oflag, /* mode_t mode, int value */ ...)
{
	va_list	ap;
	mode_t	crmode = 0;
	sem_t	*sem = NULL;
	struct	stat statbuf;
	semaddr_t *next = NULL;
	int	fd = 0;
	int	err = 0;
	int	cr_flag = 0;
	uint_t	value = 0;

	if (__pos4obj_check(path) == -1)
		return (SEM_FAILED);

	/* acquire semaphore lock to have atomic operation */
	if (__pos4obj_lock(path, SEM_LOCK_TYPE) < 0)
		return (SEM_FAILED);

	/* modify oflag to have RDWR and filter CREATE mode only */
	oflag = (oflag & (O_CREAT|O_EXCL)) | (O_RDWR);
	if ((oflag & O_CREAT) != 0) {
		va_start(ap, oflag);
		crmode = va_arg(ap, mode_t);
		value = va_arg(ap, uint_t);
		va_end(ap);
		/* check value < the max for a named semaphore */
		if ((_lsemvaluemax == -1L) ||
		    ((unsigned long)value > (unsigned long)_lsemvaluemax)) {
			errno = EINVAL;
			goto out;
		}
	}

	errno = 0;

	if ((fd = __pos4obj_open(path, SEM_DATA_TYPE,
				oflag, crmode, &cr_flag)) < 0)
		goto out;

	if (cr_flag)
		cr_flag = DFILE_CREATE | DFILE_OPEN;
	else
		cr_flag = DFILE_OPEN;

	/* find out inode # for the opened file */
	if (fstat(fd, &statbuf) < 0)
		goto out;

	/* if created, acquire total_size in the file */
	if ((cr_flag & DFILE_CREATE) != 0) {
		if (ftruncate(fd, sizeof (sem_t)) < 0)
			goto out;

	} else {
		(void) mutex_lock(&semlock);

		/*
		 * if this semaphore has already been opened, inode
		 * will indicate then return the same semaphore address
		 */
		for (next = semheadp; next != NULL; next = next->sad_next) {
			if (statbuf.st_ino == next->sad_inode &&
				strcmp(path, next->sad_name) == 0) {

				(void) __close_nc(fd);
				(void) mutex_unlock(&semlock);
				(void) __pos4obj_unlock(path, SEM_LOCK_TYPE);
				return ((sem_t *)next->sad_addr);
			}
		}
		(void) mutex_unlock(&semlock);
	}


	/* new sem descriptor to be allocated and new address to be mapped */
	if ((next = (semaddr_t *)malloc(sizeof (semaddr_t))) == NULL) {
		errno = ENOMEM;
		goto out;
	}

	cr_flag |= ALLOC_MEM;

	/* LINTED */
	sem = (sem_t *)mmap(0, sizeof (sem_t), PROT_READ|PROT_WRITE,
							MAP_SHARED, fd, 0);
	(void) __close_nc(fd);
	cr_flag &= ~DFILE_OPEN;

	if (sem == MAP_FAILED)
		goto out;

	cr_flag |= DFILE_MMAP;

	/* add to the list pointed by semheadp */
	next->sad_next = semheadp;
	semheadp = next;
	next->sad_addr = sem;
	next->sad_inode = statbuf.st_ino;
	(void) strcpy(next->sad_name, path);

	/* initialize it by jumping through the jump table */
	if ((cr_flag & DFILE_CREATE) != 0) {
		if ((err = (*(pos4_jmptab->sema_init))(sem, value,
		    USYNC_PROCESS, 0)) != 0) {
			errno = err;
			goto out;
		}
	}

	if (__pos4obj_unlock(path, SEM_LOCK_TYPE) < 0)
		return (SEM_FAILED);

	return (sem);

out:
	err = errno;
	if ((cr_flag & DFILE_OPEN) != 0)
		(void) __close_nc(fd);
	if ((cr_flag & DFILE_CREATE) != 0)
		(void) __pos4obj_unlink(path, SEM_DATA_TYPE);
	if ((cr_flag & ALLOC_MEM) != 0)
		free((caddr_t)next);
	if ((cr_flag & DFILE_MMAP) != 0)
		(void) munmap((caddr_t)sem, sizeof (sem_t));

	errno = err;
	(void) __pos4obj_unlock(path, SEM_LOCK_TYPE);
	return (SEM_FAILED);
}


int
_sem_close(sem_t *sem)
{
	semaddr_t	**next;
	semaddr_t	*freeit;

	(void) mutex_lock(&semlock);

	for (next = &semheadp; (freeit = *next) != NULL;
	    next = &(freeit->sad_next)) {
		if (freeit->sad_addr == sem) {
			*next = freeit->sad_next;
			free((caddr_t)freeit);
			(void) mutex_unlock(&semlock);
			return (munmap((caddr_t)sem, sizeof (sem_t)));
		}
	}
	(void) mutex_unlock(&semlock);
	errno = EINVAL;
	return (-1);
}


int
_sem_unlink(const char *path)
{
	int	err;
	int	oerrno;

	if (__pos4obj_check(path) < 0)
		return (-1);

	if (__pos4obj_lock(path, SEM_LOCK_TYPE) < 0)
		return (-1);

	err =  __pos4obj_unlink(path, SEM_DATA_TYPE);

	oerrno = errno;

	(void) __pos4obj_unlock(path, SEM_LOCK_TYPE);

	errno = oerrno;

	return (err);
}

int
_sem_init(sem_t *sem, int pshared, uint_t value)
{
	int	err;

	if ((err = (*(pos4_jmptab->sema_init))(sem, value,
	    pshared ? USYNC_PROCESS : USYNC_THREAD, NULL)) != 0) {
		errno = err;
		return (-1);
	}
	return (0);
}

int
_sem_destroy(sem_t *sem)
{
	int	err;

	if ((err = (*(pos4_jmptab->sema_destroy))(sem)) > 0) {
		errno = err;
		return (-1);
	}
	return (err);
}

int
_sem_post(sem_t *sem)
{
	int	err;
	if ((err = (*(pos4_jmptab->sema_post))(sem)) > 0) {
		errno = err;
		return (-1);
	}
	return (err);
}

int
_sem_wait(sem_t *sem)
{
	int	err;
	if ((err = (*(pos4_jmptab->sema_wait))(sem)) > 0) {
		errno = err;
		return (-1);
	}
	return (err);
}

int
_sem_trywait(sem_t *sem)
{
	int	err;

	if ((err = (*(pos4_jmptab->sema_trywait))(sem)) > 0) {
		errno = (err == EBUSY ? EAGAIN : err);
		return (-1);
	}
	return (err);
}

int
_sem_getvalue(sem_t *sem, int *sval)
{
	*sval = (int)sem->sem_count;
	return (0);
}

int
_librt_sema_wait(sem_t *sem)
{

	/*
	 * If linked with /usr/lib[/sparcv9]/libthread.so, then we
	 * will call _libthread_sema_wait(), since it is only defined
	 * in that library. The routine _libthread_sema_wait() will
	 * ensure that unbound threads are bound to LWPs while waiting
	 * on the semaphore.
	 */
	if (_libthread_sema_wait != NULL)
		return (_libthread_sema_wait((sema_t *)sem));

	/*
	 * If linked with /usr/lib/lwp[/sparcv9]/libthread.so, then we
	 * can call sema_wait() directly because all threads are permanently
	 * bound to LWPs in that library.
	 */
	return (sema_wait((sema_t *)sem));
}

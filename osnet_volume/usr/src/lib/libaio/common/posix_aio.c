/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)posix_aio.c	1.24	99/12/06 SMI"

/*
 * posix_aio.c implements the POSIX async. I/O
 * functions for librt
 *
 *	aio_read
 *	aio_write
 *	aio_error
 *	aio_return
 *	aio_suspend
 *	lio_listio
 *
 * the following are not supported yet but are kept here
 * for completeness
 *
 *	aio_fsync
 *	aio_cancel
 *
 * (also, the 64-bit versions for _LARGEFILE64_SOURCE)
 */

#include	<aio.h>
#include	<stdio.h>
#include	<errno.h>
#include	<libaio.h>
#include	<sys/time.h>
#include	<sys/lwp.h>
#include	<signal.h>
#include	<string.h>
#include	<unistd.h>
#include	<sys/stat.h>
#include	<time.h>

extern aio_req_t *_aio_hash_find(aio_result_t *);
extern int _libc_nanosleep(const struct timespec *, struct timespec *);

/*
 * List I/O list head stuff
 */
static aio_lio_t *_lio_head_freelist = NULL;
static int _aio_lio_alloc(aio_lio_t **);
static void _aio_lio_free(aio_lio_t *);
static void _lio_list_decr(aio_lio_t *);

void _aio_remove(aio_req_t *);

int
__aio_read(aiocb_t *cb)
{
	aio_lio_t	*head = NULL;

	if ((cb == NULL) || cb->aio_reqprio < 0) {
		errno = EINVAL;
		return (-1);
	}

	cb->aio_lio_opcode = LIO_READ;
	return (_aio_rw(cb, head, &__nextworker_rd, AIOAREAD,
	    (AIO_KAIO | AIO_NO_DUPS)));
}

int
__aio_write(aiocb_t *cb)
{
	aio_lio_t	*head = NULL;

	if ((cb == NULL) || cb->aio_reqprio < 0) {
		errno = EINVAL;
		return (-1);
	}

	cb->aio_lio_opcode = LIO_WRITE;
	return (_aio_rw(cb, head, &__nextworker_wr, AIOAWRITE,
	    (AIO_KAIO | AIO_NO_DUPS)));
}


int
__lio_listio(int mode, aiocb_t * const list[],
    int nent, struct sigevent *sig)
{
	int 		i, err;
	int 		aio_ufs = 0;
	int 		oerrno = 0;
	aio_lio_t	*head = NULL;
	int		state = 0;
	static long	aio_list_max = 0;
	aio_worker_t 	**nextworker;
	int 		EIOflg = 0;
	int 		rw;
	int		do_kaio = 0;

	if (!_kaio_ok)
		_kaio_init();

	if (aio_list_max == 0)
		aio_list_max = sysconf(_SC_AIO_LISTIO_MAX);

	if (nent < 0 || (long)nent > aio_list_max) {
		errno = EINVAL;
		return (-1);
	}

	switch (mode) {
	case LIO_WAIT:
		state = NOCHECK;
		break;
	case LIO_NOWAIT:
		state = CHECK;
		break;
	default:
		errno = EINVAL;
		return (-1);
	}

	for (i = 0; i < nent; i++) {
		if (list[i]) {
			if (list[i]->aio_lio_opcode != LIO_NOP) {
				list[i]->aio_state = state;
				if (KAIO_SUPPORTED(list[i]->aio_fildes))
					do_kaio++;
				else
					list[i]->aio_resultp.aio_errno =
						ENOTSUP;
			} else
				list[i]->aio_state = NOCHECK;
		}
	}

	if (do_kaio) {
		if ((err = (int)_kaio(AIOLIO, mode, list, nent, sig)) == 0)
				return (0);
		oerrno = errno;
	} else {
		oerrno = errno = ENOTSUP;
		err = -1;
	}
	if ((err == -1) && (errno == ENOTSUP)) {
		err = errno = 0;
		/*
		 * If LIO_WAIT, or signal required, allocate a list head.
		 */
		if ((mode == LIO_WAIT) || ((sig) && (sig->sigev_signo > 0)))
			_aio_lio_alloc(&head);
		if (head) {
			_lwp_mutex_lock(&head->lio_mutex);
			head->lio_mode = (char)mode;
			if ((mode == LIO_NOWAIT) && (sig) &&
			    (sig->sigev_notify != SIGEV_NONE) &&
			    (sig->sigev_signo > 0)) {
				head->lio_signo = sig->sigev_signo;
				head->lio_sigval.sival_int =
					sig->sigev_value.sival_int;
			} else
				head->lio_signo = 0;
			head->lio_nent = head->lio_refcnt = nent;
			_lwp_mutex_unlock(&head->lio_mutex);
		}
		/*
		 * find UFS requests, errno == ENOTSUP,
		 */
		for (i = 0; i < nent; i++) {
			if (list[i] &&
				list[i]->aio_resultp.aio_errno == ENOTSUP) {
				if (list[i]->aio_lio_opcode == LIO_NOP) {
					if (head)
						_lio_list_decr(head);
					continue;
				}
				SET_KAIO_NOT_SUPPORTED(list[i]->aio_fildes);
				if (list[i]->aio_reqprio < 0) {
					list[i]->aio_resultp.aio_errno =
					    EINVAL;
					list[i]->aio_resultp.aio_return = -1;
					EIOflg = 1;
					if (head)
						_lio_list_decr(head);
					continue;
				}
				/*
				 * submit an AIO request with flags AIO_NO_KAIO
				 * to avoid the kaio() syscall in _aio_rw()
				 */
				switch (list[i]->aio_lio_opcode) {
					case LIO_READ:
						rw = AIOAREAD;
						nextworker = &__nextworker_rd;
						break;
					case LIO_WRITE:
						rw = AIOAWRITE;
						nextworker = &__nextworker_wr;
						break;
				}
				err = _aio_rw(list[i], head, nextworker, rw,
					    (AIO_NO_KAIO | AIO_NO_DUPS));
				if (err != 0) {
					if (head)
						_lio_list_decr(head);
					list[i]->aio_resultp.aio_errno = err;
					EIOflg = 1;
				} else
					aio_ufs++;

			} else {
				if (head)
					_lio_list_decr(head);
				continue;
			}
		}
	}
	if (EIOflg) {
		errno = EIO;
		return (-1);
	}
	if ((mode == LIO_WAIT) && (oerrno == ENOTSUP)) {
		/*
		 * call kaio(AIOLIOWAIT) to get all outstanding
		 * kernel AIO requests
		 */
		if ((nent - aio_ufs) > 0) {
			(void) _kaio(AIOLIOWAIT, mode, list, nent, sig);
		}
		if (head && head->lio_nent > 0) {
			_lwp_mutex_lock(&head->lio_mutex);
			while (head->lio_refcnt > 0) {
				errno = _lwp_cond_wait(&head->lio_cond_cv,
				    &head->lio_mutex);
				if (errno) {
					_lwp_mutex_unlock(&head->lio_mutex);
					return (-1);
				}
			}
			_lwp_mutex_unlock(&head->lio_mutex);
			for (i = 0; i < nent; i++) {
				if (list[i] &&
				    list[i]->aio_resultp.aio_errno) {
					errno = EIO;
					return (-1);
				}
			}
		}
		return (0);
	}
	return (err);
}

static void
_lio_list_decr(aio_lio_t *head)
{
	_lwp_mutex_lock(&head->lio_mutex);
	head->lio_nent--;
	head->lio_refcnt--;
	_lwp_mutex_unlock(&head->lio_mutex);
}

#pragma	weak	_cancelon
#pragma	weak	_canceloff

extern void _cancelon(void);
extern void _canceloff(void);

int
__aio_suspend(aiocb_t *list[], int nent, const timespec_t *timo)
{
	int		oerrno, err, i, aio_done, aio_outstanding;
	struct timeval	curtime, end, wait;
	int		timedwait = 0, polledwait = 0;
	struct timespec	one_sec = {1, 0};

	for (i = 0; i < nent; i++) {
		if (list[i] && list[i]->aio_state == CHECK)
			list[i]->aio_state = CHECKED;
	}

	if (_cancelon != NULL)
		_cancelon();
	/*
	 * Always do the kaio() call without using the KAIO_SUPPORTED()
	 * checks because it is not mandatory to have a valid fd
	 * set in the list entries, only the resultp must be set.
	 */

	err = (int)_kaio(AIOSUSPEND, list, nent, timo, -1);
	if (_cancelon != NULL)
		_canceloff();
	if (!err)
		return (0);

	oerrno = errno;

	if (timo) {
		if ((timo->tv_sec > 0) || (timo->tv_nsec > 0)) {
			gettimeofday(&curtime, NULL);
			end.tv_sec = timo->tv_sec + curtime.tv_sec;
			end.tv_usec = (timo->tv_nsec  / 1000000)
					+ curtime.tv_usec;
			timedwait++;
		} else
			polledwait++;
	}

	aio_done = aio_outstanding = 0;

	/*CONSTANTCONDITION*/
	while (1) {
		for (i = 0; i < nent; i++) {
			if (list[i] == NULL)
				continue;
			if (list[i]->aio_resultp.aio_errno == EINPROGRESS)
				aio_outstanding++;
			else if (list[i]->aio_resultp.aio_errno != ECANCELED)
				aio_done++;
		}
		/*
		 * got some I/O's
		 */
		if (aio_done) {
			errno = 0;
			return (0);
		}
		/*
		 * No UFS I/O outstanding, return
		 * kaio(AIOSUSPEND) error status
		 */
		if (aio_outstanding == 0) {
			errno = oerrno;
			return (err);
		}
		if (polledwait) {
			errno = EAGAIN;
			return (-1);
		} else if (timedwait) {
			gettimeofday(&curtime, NULL);
			wait.tv_sec = end.tv_sec - curtime.tv_sec;
			wait.tv_usec = end.tv_usec - curtime.tv_usec;
			if (wait.tv_sec < 0 || (wait.tv_sec == 0 &&
				wait.tv_usec <= 0)) {
				errno = EAGAIN;
				return (-1);
			}
		} else {
			if (_libc_nanosleep(&one_sec, NULL) == -1)
				return (-1);
		}
	}
	/* NOTREACHED */
}

int
__aio_error(aiocb_t *cb)
{
	aio_req_t *reqp;

	if (cb->aio_resultp.aio_errno == EINPROGRESS) {
		if (cb->aio_state == CHECK) {
			/*
			 * Always do the kaio() call without using
			 * the KAIO_SUPPORTED()
			 * checks because it is not mandatory to
			 * have a valid fd
			 * set in the aiocb, only the resultp must be set.
			 */
			if (((int)_kaio(AIOERROR, cb)) == EINVAL) {
				errno = EINVAL;
				return (-1);
			}
		} else if (cb->aio_state == CHECKED)
			cb->aio_state =  CHECK;
	} else if (cb->aio_state == USERAIO) {
		_aio_lock();
		if (reqp = _aio_hash_find(&cb->aio_resultp)) {
			cb->aio_state = NOCHECK;
			_lio_remove(reqp->lio_head);
			_aio_hash_del(reqp->req_resultp);
			_aio_req_free(reqp);
		}
		_aio_unlock();
	}
	return (cb->aio_resultp.aio_errno);
}

ssize_t
__aio_return(aiocb_t *cb)
{
	ssize_t ret;
	aio_req_t *reqp;

	/*
	 * graceful detection of an invalid cb is not possible. a
	 * SIGSEGV will be generated if it is invalid.
	 */
	if (cb == NULL) {
		errno = EINVAL;
		exit(-1);
	}

	/*
	 * we use this condition to indicate that
	 * aio_return has been called before
	 */
	if (cb->aio_resultp.aio_return == -1 &&
	    cb->aio_resultp.aio_errno == EINVAL) {
		errno = EINVAL;
		return (-1);
	}

	/*
	 * Before we return mark the result as being returned so that later
	 * calls to aio_return() will return the fact that the result has
	 * already been returned
	 */
	ret = cb->aio_resultp.aio_return;
	cb->aio_resultp.aio_return = -1;
	cb->aio_resultp.aio_errno = EINVAL;
	if (cb->aio_state == USERAIO) {
		_aio_lock();
		if (reqp = _aio_hash_find(&cb->aio_resultp)) {
			cb->aio_state = NOCHECK;
			_lio_remove(reqp->lio_head);
			_aio_hash_del(reqp->req_resultp);
			_aio_req_free(reqp);
		}
		_aio_unlock();
	}
	return (ret);

}

void
_lio_remove(aio_lio_t *head)
{
	int refcnt;

	if (head) {
		_lwp_mutex_lock(&head->lio_mutex);
		refcnt = (head->lio_nent--);
		_lwp_mutex_unlock(&head->lio_mutex);
		if (!refcnt)
			_aio_lio_free(head);
	}
}

void
_aio_remove(aio_req_t *reqp)
{
	_lio_remove(reqp->lio_head);
	_aio_lock();
	_aio_hash_del(reqp->req_resultp);
	_aio_req_free(reqp);
	_aio_unlock();
}

int
_aio_lio_alloc(aio_lio_t **head)
{
	aio_lio_t	*lio_head;

	_lwp_mutex_lock(&__lio_mutex);
	if (_lio_head_freelist == NULL) {
		lio_head = (aio_lio_t *)malloc(sizeof (aio_lio_t));
	} else {
		lio_head = _lio_head_freelist;
		_lio_head_freelist = lio_head->lio_next;
	}
	if (lio_head == NULL) {
		_lwp_mutex_unlock(&__lio_mutex);
		return (-1);
	}
	memset(lio_head, 0, sizeof (aio_lio_t));
	*head = lio_head;
	_lwp_mutex_unlock(&__lio_mutex);
	return (0);
}

void
_aio_lio_free(aio_lio_t *head)
{
	aio_lio_t	*lio_head = head;

	_lwp_mutex_lock(&__lio_mutex);
	if (_lio_head_freelist == NULL) {
		_lio_head_freelist = lio_head;
	} else {
		_lio_head_freelist->lio_next  = lio_head;
	}
	_lwp_mutex_unlock(&__lio_mutex);
}

static int
__aio_fsync_bar(aiocb_t *cb, aio_lio_t *head, aio_worker_t *aiowp,
    int workerscnt)
{
	int i;
	int err;
	aio_worker_t *next = aiowp;

	for (i = 0; i < workerscnt; i++) {
		err = _aio_rw(cb, head, &next, AIOFSYNC, AIO_NO_KAIO);
		if (err != 0) {
			_lwp_mutex_lock(&head->lio_mutex);
			head->lio_mode = LIO_DESTROY;
			head->lio_nent = head->lio_refcnt = i;
			_lwp_mutex_unlock(&head->lio_mutex);
			errno = EAGAIN;
			return (-1);
		}
		next = next->work_forw;
	}
	return (0);
}

int
__aio_fsync(int op, aiocb_t *cb, int waitflg)
{
	struct stat buf;
	aio_lio_t *head;

	if (cb == NULL) {
		return (0);
	}

	if ((op != O_DSYNC) && (op != O_SYNC)) {
		errno = EINVAL;
		return (-1);
	}

	if (fstat(cb->aio_fildes, &buf) < 0)
		return (-1);

	/*
	 * re-use aio_offset as the op field.
	 * 	O_DSYNC - fdatasync()
	 * 	O_SYNC - fsync()
	 */
	cb->aio_offset = op;
	cb->aio_lio_opcode = AIOFSYNC;

	/*
	 * create a list of fsync requests. the worker
	 * that gets the last request will do the fsync
	 * request.
	 */
	_aio_lio_alloc(&head);
	if (head == NULL) {
		errno = EAGAIN;
		return (-1);
	}
	head->lio_mode = LIO_FSYNC;
	head->lio_signo = 0;
	head->lio_nent = head->lio_refcnt = __wr_workerscnt + __rd_workerscnt;
	/* insert an fsync request on every read workers' queue. */
	if (__aio_fsync_bar(cb, head, __workers_rd, __rd_workerscnt) == -1)
		return (-1);
	/* insert an fsync request on every write workers' queue. */
	if (__aio_fsync_bar(cb, head, __workers_wr, __wr_workerscnt) == -1)
		return (-1);
	if (waitflg) {
		_lwp_mutex_lock(&head->lio_mutex);
		while (head->lio_refcnt > 0) {
			errno = _lwp_cond_wait(&head->lio_cond_cv,
			    &head->lio_mutex);
			if (errno) {
				_lwp_mutex_unlock(&head->lio_mutex);
				return (-1);
			}
		}
		_lwp_mutex_unlock(&head->lio_mutex);
	}
	return (0);
}

int
__aio_cancel(int fd, aiocb_t *cb)
{
	aio_req_t *rp;
	aio_worker_t *aiowp;
	int done = 0;
	int canceled = 0;
	struct stat buf;

	if (fstat(fd, &buf) < 0)
		return (-1);

	if (cb != NULL) {
		if (cb->aio_state == USERAIO) {
			_aio_lock();
			rp = _aio_hash_find(&cb->aio_resultp);
			if (rp == NULL) {
				_aio_unlock();
				return (AIO_ALLDONE);
			} else {
				aiowp = rp->req_worker;
				_lwp_mutex_lock(&aiowp->work_qlock1);
				_aio_cancel_req(aiowp, rp, &canceled,
				    &done);
				_lwp_mutex_unlock(&aiowp->work_qlock1);
				_aio_unlock();
				if (done)
					return (AIO_ALLDONE);
				else if (canceled)
					return (AIO_CANCELED);
				else
					return (AIO_NOTCANCELED);
			}
		}
		return ((int)_kaio(AIOCANCEL, fd, cb));
	}

	return (aiocancel_all(fd));
}

#if	defined(_LARGEFILE64_SOURCE) && !defined(_LP64)

int
__aio_read64(aiocb64_t *cb)
{
	aio_lio_t	*head = NULL;

	if (cb == NULL || cb->aio_offset < 0 || cb->aio_reqprio < 0) {
		errno = EINVAL;
		return (-1);
	}

	cb->aio_lio_opcode = LIO_READ;
	return (_aio_rw64(cb, head, &__nextworker_rd, AIOAREAD64,
	    (AIO_KAIO | AIO_NO_DUPS)));
}

int
__aio_write64(aiocb64_t *cb)
{
	aio_lio_t	*head = NULL;

	if (cb == NULL || cb->aio_offset < 0 || cb->aio_reqprio < 0) {
		errno = EINVAL;
		return (-1);
	}
	cb->aio_lio_opcode = LIO_WRITE;
	return (_aio_rw64(cb, head, &__nextworker_wr, AIOAWRITE64,
	    (AIO_KAIO | AIO_NO_DUPS)));
}

int
__lio_listio64(int mode, aiocb64_t * const list[],
    int nent, struct sigevent *sig)
{
	int 		i, err;
	int 		aio_ufs = 0;
	int 		oerrno = 0;
	aio_lio_t	*head = NULL;
	int		state = 0;
	static long	aio_list_max = 0;
	aio_worker_t 	**nextworker;
	int 		EIOflg = 0;
	int 		rw;
	int		do_kaio = 0;

	if (!_kaio_ok)
		_kaio_init();

	if (aio_list_max == 0)
		aio_list_max = sysconf(_SC_AIO_LISTIO_MAX);

	if (nent < 0 || nent > aio_list_max) {
		errno = EINVAL;
		return (-1);
	}

	switch (mode) {
	case LIO_WAIT:
		state = NOCHECK;
		break;
	case LIO_NOWAIT:
		state = CHECK;
		break;
	default:
		errno = EINVAL;
		return (-1);
	}

	for (i = 0; i < nent; i++) {
		if (list[i]) {
			if (list[i]->aio_lio_opcode != LIO_NOP) {
				list[i]->aio_state = state;
				if (KAIO_SUPPORTED(list[i]->aio_fildes))
					do_kaio++;
				else
					list[i]->aio_resultp.aio_errno =
						ENOTSUP;
			} else
				list[i]->aio_state = NOCHECK;
		}
	}

	if (do_kaio) {
		if ((err = (int)_kaio(AIOLIO64, mode, list, nent, sig)) == 0)
				return (0);
		oerrno = errno;
	} else {
		oerrno = errno = ENOTSUP;
		err = -1;
	}
	if ((err == -1) && (errno == ENOTSUP)) {
		err = errno = 0;
		/*
		 * If LIO_WAIT, or signal required, allocate a list head.
		 */
		if ((mode == LIO_WAIT) || ((sig) && (sig->sigev_signo > 0)))
			_aio_lio_alloc(&head);
		if (head) {
			_lwp_mutex_lock(&head->lio_mutex);
			head->lio_mode = mode;
			if ((mode == LIO_NOWAIT) && (sig) &&
			    (sig->sigev_notify != SIGEV_NONE) &&
			    (sig->sigev_signo > 0)) {
				head->lio_signo = sig->sigev_signo;
				head->lio_sigval.sival_int =
					sig->sigev_value.sival_int;
			} else
				head->lio_signo = 0;
			head->lio_nent = head->lio_refcnt = nent;
			_lwp_mutex_unlock(&head->lio_mutex);
		}
		/*
		 * find UFS requests, errno == ENOTSUP,
		 */
		for (i = 0; i < nent; i++) {
			if (list[i] &&
				list[i]->aio_resultp.aio_errno == ENOTSUP) {
				if (list[i]->aio_lio_opcode == LIO_NOP) {
					if (head)
						_lio_list_decr(head);
					continue;
				}
				SET_KAIO_NOT_SUPPORTED(list[i]->aio_fildes);
				if (list[i]->aio_reqprio < 0) {
					list[i]->aio_resultp.aio_errno =
					    EINVAL;
					list[i]->aio_resultp.aio_return = -1;
					EIOflg = 1;
					if (head)
						_lio_list_decr(head);
					continue;
				}
				/*
				 * submit an AIO request with flags AIO_NO_KAIO
				 * to avoid the kaio() syscall in _aio_rw()
				 */
				switch (list[i]->aio_lio_opcode) {
					case LIO_READ:
						rw = AIOAREAD64;
						nextworker = &__nextworker_rd;
						break;
					case LIO_WRITE:
						rw = AIOAWRITE64;
						nextworker = &__nextworker_wr;
						break;
				}
				err = _aio_rw64(list[i], head, nextworker, rw,
					    (AIO_NO_KAIO | AIO_NO_DUPS));
				if (err != 0) {
					if (head)
						_lio_list_decr(head);
					list[i]->aio_resultp.aio_errno = err;
					EIOflg = 1;
				} else
					aio_ufs++;

			} else {
				if (head)
					_lio_list_decr(head);
				continue;
			}
		}
	}
	if (EIOflg) {
		errno = EIO;
		return (-1);
	}
	if ((mode == LIO_WAIT) && (oerrno == ENOTSUP)) {
		/*
		 * call kaio(AIOLIOWAIT) to get all outstanding
		 * kernel AIO requests
		 */
		if ((nent - aio_ufs) > 0) {
			_kaio(AIOLIOWAIT, mode, list, nent, sig);
		}
		if (head && head->lio_nent > 0) {
			_lwp_mutex_lock(&head->lio_mutex);
			while (head->lio_refcnt > 0) {
				errno = _lwp_cond_wait(&head->lio_cond_cv,
				    &head->lio_mutex);
				if (errno) {
					_lwp_mutex_unlock(&head->lio_mutex);
					return (-1);
				}
			}
			_lwp_mutex_unlock(&head->lio_mutex);
			for (i = 0; i < nent; i++) {
				if (list[i] &&
				    list[i]->aio_resultp.aio_errno) {
					errno = EIO;
					return (-1);
				}
			}
		}
		return (0);
	}
	return (err);
}

int
__aio_suspend64(aiocb64_t *list[], int nent, const timespec_t *timo)
{
	int		oerrno, err, i, aio_done, aio_outstanding;
	struct timeval	curtime, end, wait;
	int		timedwait = 0, polledwait = 0;

	for (i = 0; i < nent; i++) {
		if (list[i] && list[i]->aio_state == CHECK)
			list[i]->aio_state = CHECKED;
	}

	if (_cancelon != NULL)
		_cancelon();
	/*
	 * Always do the kaio() call without using the KAIO_SUPPORTED()
	 * checks because it is not mandatory to have a valid fd
	 * set in the list entries, only the resultp must be set.
	 */
	err = (int)_kaio(AIOSUSPEND64, list, nent, timo, -1);
	if (_cancelon != NULL)
		_canceloff();
	if (!err)
		return (0);
	oerrno = errno;

	if (timo) {
		if ((timo->tv_sec > 0) || (timo->tv_nsec > 0)) {
			gettimeofday(&curtime, NULL);
			end.tv_sec = timo->tv_sec + curtime.tv_sec;
			end.tv_usec = (timo->tv_nsec  / 1000000)
					+ curtime.tv_usec;
			timedwait++;
		} else
			polledwait++;
	}

	aio_done = aio_outstanding = 0;

	/*CONSTANTCONDITION*/
	while (1) {
		for (i = 0; i < nent; i++) {
			if (list[i] == NULL)
				continue;
			if (list[i]->aio_resultp.aio_errno == EINPROGRESS)
				aio_outstanding++;
			else if (list[i]->aio_resultp.aio_errno != ECANCELED)
				aio_done++;
		}
		/*
		 * got some I/O's
		 */
		if (aio_done) {
			errno = 0;
			return (0);
		}
		/*
		 * No UFS I/O outstanding, return
		 * kaio(AIOSUSPEND) error status
		 */
		if (aio_outstanding == 0) {
			errno = oerrno;
			return (err);
		}
		if (polledwait) {
			errno = EAGAIN;
			return (-1);
		} else if (timedwait) {
			gettimeofday(&curtime, NULL);
			wait.tv_sec = end.tv_sec - curtime.tv_sec;
			wait.tv_usec = end.tv_usec - curtime.tv_usec;
			if (wait.tv_sec < 0 || (wait.tv_sec == 0 &&
				wait.tv_usec <= 0)) {
				errno = EAGAIN;
				return (-1);
			}
		}

	}

	/*NOTREACHED*/
	return (0);
}

int
__aio_error64(aiocb64_t *cb)
{
	aio_req_t *reqp;

	if (cb->aio_resultp.aio_errno == EINPROGRESS) {
		if (cb->aio_state == CHECK) {
			/*
			 * Always do the kaio() call without using
			 * the KAIO_SUPPORTED()
			 * checks because it is not mandatory to
			 * have a valid fd
			 * set in the aiocb, only the resultp must be set.
			 */
			if ((_kaio(AIOERROR64, cb)) == EINVAL) {
				errno = EINVAL;
				return (-1);
			}
		} else if (cb->aio_state == CHECKED)
			cb->aio_state =  CHECK;
		return (cb->aio_resultp.aio_errno);
	}
	if (cb->aio_state == USERAIO) {
		_aio_lock();
		if (reqp = _aio_hash_find(&cb->aio_resultp)) {
			cb->aio_state = NOCHECK;
			_lio_remove(reqp->lio_head);
			_aio_hash_del(reqp->req_resultp);
			_aio_req_free(reqp);
		}
		_aio_unlock();
	}
	return (cb->aio_resultp.aio_errno);
}

ssize_t
__aio_return64(aiocb64_t *cb)
{
	aio_req_t *reqp;
	int ret;

	/*
	 * graceful detection of an invalid cb is not possible. a
	 * SIGSEGV will be generated if it is invalid.
	 */
	if (cb == NULL) {
		errno = EINVAL;
		exit(-1);
	}
	/*
	 * we use this condition to indicate that
	 * aio_return has been called before
	 */
	if (cb->aio_resultp.aio_return == -1 &&
	    cb->aio_resultp.aio_errno == EINVAL) {
		errno = EINVAL;
		return (-1);
	}

	/*
	 * Before we return mark the result as being returned so that later
	 * calls to aio_return() will return the fact that the result has
	 * already been returned
	 */
	ret = cb->aio_resultp.aio_return;
	cb->aio_resultp.aio_return = -1;
	cb->aio_resultp.aio_errno = EINVAL;
	if (cb->aio_state == USERAIO) {
		_aio_lock();
		if (reqp = _aio_hash_find(&cb->aio_resultp)) {
			cb->aio_state = NOCHECK;
			_lio_remove(reqp->lio_head);
			_aio_hash_del(reqp->req_resultp);
			_aio_req_free(reqp);
		}
		_aio_unlock();
	}
	return (ret);
}

int
__aio_fsync64(int op, aiocb64_t *cb, int waitflg)
{
	struct stat buf;
	aio_lio_t *head;
	aio_worker_t *nextworker;
	int workerscnt;
	int i, err;

	if (cb == NULL) {
		return (0);
	}

	if ((op != O_DSYNC) && (op != O_SYNC)) {
		errno = EINVAL;
		return (-1);
	}

	if (fstat(cb->aio_fildes, &buf) < 0)
		return (-1);

	if ((buf.st_mode & S_IWRITE) == 0) {
		errno = EBADF;
		return (-1);
	}

	/*
	 * re-use aio_offset as the op field.
	 * 	O_DSYNC - fdatasync()
	 * 	O_SYNC - fsync()
	 */
	cb->aio_offset = op;
	cb->aio_lio_opcode = AIOFSYNC;

	/*
	 * create a list of fsync requests. the worker
	 * that gets the last request will do the fsync
	 * request.
	 */
	_aio_lio_alloc(&head);
	if (head == NULL) {
		errno = EAGAIN;
		return (-1);
	}
	head->lio_signo = 0;
	workerscnt = __wr_workerscnt;
	head->lio_nent = head->lio_refcnt = workerscnt;
	cb->aio_offset = op;
	/*
	 * insert an fsync request on every write workers' queue.
	 */
	nextworker = __workers_wr;
	for (i = 0; i < workerscnt; i++) {
		err = _aio_rw64(cb, head, &nextworker, AIOFSYNC,
		    AIO_NO_KAIO);
		if (err != 0) {
			_lwp_mutex_lock(&head->lio_mutex);
			head->lio_nent = head->lio_refcnt = i;
			_lwp_mutex_unlock(&head->lio_mutex);
			errno = EAGAIN;
			return (-1);
		}
		nextworker = nextworker->work_forw;
	}
	if (waitflg) {
		_lwp_mutex_lock(&head->lio_mutex);
		while (head->lio_refcnt > 0) {
			errno = _lwp_cond_wait(&head->lio_cond_cv,
			    &head->lio_mutex);
			if (errno) {
				_lwp_mutex_unlock(&head->lio_mutex);
				return (-1);
			}
		}
		_lwp_mutex_unlock(&head->lio_mutex);
	}
	return (0);
}

int
__aio_cancel64(int fd, aiocb64_t *cb)
{
	aio_req_t	*rp;
	aio_worker_t *aiowp;
	int done = 0;
	int canceled = 0;
	struct stat	buf;

	if (fstat(fd, &buf) < 0)
		return (-1);

	if (cb != NULL) {
		if (cb->aio_state == USERAIO) {
			_aio_lock();
			rp = _aio_hash_find(&cb->aio_resultp);
			if (rp == NULL) {
				_aio_unlock();
				return (AIO_ALLDONE);
			} else {
				aiowp = rp->req_worker;
				_lwp_mutex_lock(&aiowp->work_qlock1);
				_aio_cancel_req(aiowp, rp, &canceled,
				    &done);
				_lwp_mutex_unlock(&aiowp->work_qlock1);
				_aio_unlock();
				if (done)
					return (AIO_ALLDONE);
				else if (canceled)
					return (AIO_CANCELED);
				else
					return (AIO_NOTCANCELED);
			}
		}
		return ((int)_kaio(AIOCANCEL, fd, cb));
	}

	return (aiocancel_all(fd));
}

#endif /* (_LARGEFILE64_SOURCE) && !defined(_LP64) */

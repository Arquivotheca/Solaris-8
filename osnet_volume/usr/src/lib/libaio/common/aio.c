/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)aio.c	1.55	99/11/22 SMI"

#include	<sys/asm_linkage.h>
#include	<sys/types.h>
#include	<sys/param.h>
#include	<sys/errno.h>
#include	<sys/procset.h>
#include	<sys/signal.h>
#include	<sys/siginfo.h>
#include	<sys/stat.h>
#include	<sys/ucontext.h>
#include	<unistd.h>
#include	<signal.h>
#include	<fcntl.h>
#include	<errno.h>
#include	<limits.h>
#include	<stdio.h>
#include	<string.h>
#include	"libaio.h"

static int _aio_hash_insert(aio_result_t *, aio_req_t *);
static aio_req_t *_aio_req_alloc(void);
static aio_req_t *_aio_req_get(aio_worker_t *);
static void _aio_req_add(aio_req_t *, aio_worker_t **, int);
static void _aio_req_del(aio_worker_t *, aio_req_t *, int);
static aio_result_t *_aio_req_done(void);
static void _aio_work_done(aio_worker_t *);

aio_req_t *_aio_hash_find(aio_result_t *);
void _aio_req_free(aio_req_t *);
void _aio_lock(void);
void _aio_unlock(void);

extern ssize_t __libaio_pread(int, void *, size_t, offset_t, int *);
extern ssize_t __libaio_pwrite(int, void *, size_t, offset_t, int *);
extern ssize_t __libaio_read(int, void *, size_t, int *);
extern ssize_t __libaio_write(int, void *, size_t, int *);
extern int __libaio_fdsync(int, int, int *);
extern int _libc_sigprocmask(int, const sigset_t *, sigset_t *);

static int _aio_fsync_del(aio_req_t *, aio_lio_t *);
static int _aiodone(aio_req_t *, aio_lio_t *, int, ssize_t, int);
static void _aio_cancel_work(aio_worker_t *, int, int *, int *);

#ifdef DEBUG
void _aio_stats(void);
#endif

int _pagesize;

#define	AIOREQSZ		(sizeof (struct aio_req))
#define	AIOCLICKS		((_pagesize)/AIOREQSZ)
#define	HASHSZ			8192L	/* power of 2 */
#define	AIOHASH(resultp)	((((uintptr_t)(resultp) >> 13) ^ \
				    ((uintptr_t)(resultp))) & (HASHSZ-1))
#define	POSIX_AIO(x)		((x)->req_type == AIO_POSIX_REQ)

/*
 * switch for kernel async I/O
 */
int _kaio_ok = 0;			/* 0 = disabled, 1 = on, -1 = error */

/*
 * Array for determining whether or not a file supports kaio
 */
uint32_t _kaio_supported[MAX_KAIO_FDARRAY_SIZE];

int _aioreqsize = AIOREQSZ;

#ifdef DEBUG
int *_donecnt;				/* per worker AIO done count */
int *_idlecnt;				/* per worker idle count */
int *_qfullcnt;				/* per worker full q count */
int *_firstqcnt;			/* num times queue one is used */
int *_newworker;			/* num times new worker is created */
int _clogged = 0;			/* num times all queues are locked */
int _qlocked = 0;			/* num times submitter finds q locked */
int _aio_submitcnt = 0;
int _aio_submitcnt2 = 0;
int _submitcnt = 0;
int _avesubmitcnt = 0;
int _aiowaitcnt = 0;
int _startaiowaitcnt = 1;
int _avedone = 0;
int _new_workers = 0;
#endif

/*
 *  workers for read requests.
 * (__aio_mutex lock protects circular linked list of workers.)
 */
aio_worker_t *__workers_rd;	/* circular list of AIO workers */
aio_worker_t *__nextworker_rd;	/* next worker in list of workers */
int __rd_workerscnt;		/* number of read workers */

/*
 * workers for write requests.
 * (__aio_mutex lock protects circular linked list of workers.)
 */
aio_worker_t *__workers_wr;	/* circular list of AIO workers */
aio_worker_t *__nextworker_wr;	/* next worker in list of workers */
int __wr_workerscnt;		/* number of write workers */

/*
 * worker for sigevent requests.
 */
aio_worker_t *__workers_si;	/* circular list of AIO workers */
aio_worker_t *__nextworker_si;	/* next worker in list of workers */
int __si_workerscnt;		/* number of write workers */

struct aio_req *_aio_done_tail;		/* list of done requests */
struct aio_req *_aio_done_head;

lwp_mutex_t __aio_initlock;		/* makes aio initialization  atomic */
lwp_mutex_t __aio_mutex;		/* protects counts, and linked lists */
lwp_mutex_t __aio_cachefillock;		/* single-thread aio cache filling */
lwp_cond_t __aio_cachefillcv;		/* sleep cv for cache filling */

mutex_t __lio_mutex;			/* protects lio lists */

int __aiostksz;				/* aio worker's stack size */
int __aio_cachefilling = 0;		/* set when aio cache is filling */
int __sigio_masked = 0;			/* bit mask for SIGIO signal */
int __sigio_maskedcnt = 0;		/* mask count for SIGIO signal */
pid_t __pid;
static struct aio_req **_aio_hash;
static struct aio_req *_aio_freelist;
static int _aio_freelist_cnt;

static struct sigaction act;

cond_t _aio_done_cv;

/*
 * Input queue of requests which is serviced by the aux. lwps.
 */
cond_t _aio_idle_cv;

int _aio_cnt = 0;
int _aio_donecnt = 0;

int _max_workers = 256;			/* max number of workers permitted */
int _min_workers = 8;			/* min number of workers */
int _maxworkload = 32;			/* max length of worker's request q */
int _minworkload = 2;			/* min number of request in q */
int _aio_outstand_cnt = 0;		/* # of queued requests */
int _aio_worker_cnt = 0;		/* number of workers to do requests */
int _idle_workers = 0;			/* number of idle workers */
int __uaio_ok = 0;			/* AIO has been enabled */
sigset_t _worker_set;			/* worker's signal mask */

int _aiowait_flag;			/* when set, aiowait() is inprogress */

struct aio_worker *_kaiowp;		/* points to kaio cleanup thread */
/*
 * called by the child when the main thread forks. the child is
 * cleaned up so that it can use libaio.
 */
void
_aio_forkinit(void)
{
	__uaio_ok = 0;
	__workers_rd = NULL;
	__nextworker_rd = NULL;
	__workers_wr = NULL;
	__nextworker_wr = NULL;
	_aio_done_tail = NULL;
	_aio_done_head = NULL;
	_aio_hash = NULL;
	_aio_freelist = NULL;
	_aio_freelist_cnt = 0;
	_aio_outstand_cnt = 0;
	_aio_worker_cnt = 0;
	_idle_workers = 0;
	_kaio_ok = 0;
#ifdef	DEBUG
	_clogged = 0;
	_qlocked = 0;
#endif
}

#ifdef DEBUG
/*
 * print out a bunch of interesting statistics when the process
 * exits.
 */
void
_aio_stats()
{
	int i;
	char *fmt;
	int cnt;
	FILE *fp;

	fp = fopen("/tmp/libaio.log", "w+a");
	if (fp == NULL)
		return;
	fprintf(fp, "size of AIO request struct = %d bytes\n", _aioreqsize);
	fprintf(fp, "number of AIO workers = %d\n", _aio_worker_cnt);
	cnt = _aio_worker_cnt + 1;
	for (i = 2; i <= cnt; i++) {
		fmt = "%d done %d, idle = %d, qfull = %d, newworker = %d\n";
		fprintf(fp, fmt, i, _donecnt[i], _idlecnt[i], _qfullcnt[i],
		    _newworker[i]);
	}
	fprintf(fp, "num times submitter found next work queue locked = %d\n",
	    _qlocked);
	fprintf(fp, "num times submitter found all work queues locked = %d\n",
	    _clogged);
	fprintf(fp, "average submit request = %d\n", _avesubmitcnt);
	fprintf(fp, "average number of submit requests per new worker = %d\n",
	    _avedone);
}
#endif

/*
 * libaio is initialized when an AIO request is made. important
 * constants are initialized like the max number of workers that
 * libaio can create, and the minimum number of workers permitted before
 * imposing some restrictions. also, some workers are created.
 */
int
__uaio_init(void)
{
	int i;
	size_t size;
	extern sigset_t __sigiomask;

	_lwp_mutex_lock(&__aio_initlock);
	if (!__uaio_ok) {
		__pid = getpid();
		act.sa_handler = aiosigcancelhndlr;
		act.sa_flags = SA_SIGINFO;
		if (_sigaction(SIGAIOCANCEL, &act, &sigcanact) == -1) {
			_lwp_mutex_unlock(&__aio_initlock);
			return (-1);
		}
		/*
		 * Constant sigiomask, used by _aiosendsig()
		 */
		sigaddset(&__sigiomask, SIGIO);
#ifdef DEBUG
		size = _max_workers * (sizeof (int) * 5 +
		    sizeof (int));
		_donecnt = malloc(size);
		memset((caddr_t)_donecnt, 0, size);
		_idlecnt = _donecnt + _max_workers;
		_qfullcnt = _idlecnt + _max_workers;
		_firstqcnt = _qfullcnt + _max_workers;
		_newworker = _firstqcnt + _max_workers;
		atexit(_aio_stats);
#endif
		size = HASHSZ * sizeof (struct aio_req *);
		_aio_hash = malloc(size);
		if (_aio_hash == NULL) {
			_lwp_mutex_unlock(&__aio_initlock);
			return (-1);
		}
		memset((caddr_t)_aio_hash, 0, size);

		/* initialize worker's signal mask to only catch SIGAIOCANCEL */
		sigfillset(&_worker_set);
		sigdelset(&_worker_set, SIGAIOCANCEL);

		/*
		 * Create equal number of READ and WRITE workers.
		 */
		i = 0;
		while (i++ < (_min_workers/2))
			_aio_create_worker(NULL, AIOREAD);
		i = 0;
		while (i++ < (_min_workers/2))
			_aio_create_worker(NULL, AIOWRITE);

		/* create one worker to send completion signals. */
		_aio_create_worker(NULL, AIOSIGEV);
		_lwp_mutex_unlock(&__aio_initlock);
		__uaio_ok = 1;
		return (0);
	}

	_lwp_mutex_unlock(&__aio_initlock);
	return (0);
}

/*
 * special kaio cleanup thread sits in a loop in the
 * kernel waiting for pending kaio requests to complete.
 */
void
/*ARGSUSED0*/
_kaio_cleanup_thread(void *arg)
{
	(void) _kaio(AIOSTART);
}

/*
 * initialize kaio.
 */
void
_kaio_init(void)
{
	caddr_t stk;
	int stksize;
	int error = 0;
	sigset_t set;
	ucontext_t uc;

	_lwp_mutex_lock(&__aio_initlock);
	if (!_kaio_ok) {
		_pagesize = (int)PAGESIZE;
		__aiostksz = 8 * _pagesize;
		__init_stacks(__aiostksz, _max_workers);
		if (_aio_alloc_stack(__aiostksz, &stk) == 0)
			error =  ENOMEM;
		else {
			/* LINTED */
			_kaiowp = (struct aio_worker *)(stk + __aiostksz -
			    sizeof (struct aio_worker));
			_kaiowp->work_stk = stk;
			stksize = __aiostksz - (int)sizeof (struct aio_worker);
			_lwp_makecontext(&uc, _kaio_cleanup_thread, NULL,
			    _kaiowp, stk, stksize);
			sigfillset(&set);
			memcpy(&uc.uc_sigmask, &set, sizeof (sigset_t));
			error = (int)_kaio(AIOINIT);
			if (!error)
				error = _lwp_create(&uc, NULL,
				    &_kaiowp->work_lid);
			if (error)
				_aio_free_stack_unlocked(__aiostksz, stk);
		}
		if (error)
			_kaio_ok = -1;
		else
			_kaio_ok = 1;
	}
	_lwp_mutex_unlock(&__aio_initlock);
}

int
aioread(int fd, caddr_t buf, int bufsz, off_t offset, int whence,
    aio_result_t *resultp)
{
	return (_aiorw(fd, buf, bufsz, offset, whence, resultp, AIOREAD));
}

int
aiowrite(int fd, caddr_t buf, int bufsz, off_t offset, int whence,
    aio_result_t *resultp)
{
	return (_aiorw(fd, buf, bufsz, offset, whence, resultp, AIOWRITE));
}

#if	defined(_LARGEFILE64_SOURCE) && !defined(_LP64)
int
aioread64(int fd, caddr_t buf, int bufsz, off64_t offset, int whence,
    aio_result_t *resultp)
{
	return (_aiorw(fd, buf, bufsz, offset, whence, resultp, AIOREAD));
}

int
aiowrite64(int fd, caddr_t buf, int bufsz, off64_t offset, int whence,
    aio_result_t *resultp)
{
	return (_aiorw(fd, buf, bufsz, offset, whence, resultp, AIOWRITE));
}
#endif	/* (_LARGEFILE64_SOURCE) && !defined(_LP64) */

int
_aiorw(int fd, caddr_t buf, int bufsz, offset_t offset, int whence,
    aio_result_t *resultp, int mode)
{
	aio_worker_t **nextworker;
	aio_req_t *aiorp = NULL;
	aio_args_t *ap = NULL;
	offset_t loffset = 0;
	struct stat stat;
	int err = 0;
	int kerr;

	switch (whence) {

	case SEEK_SET:
		loffset = offset;
		break;
	case SEEK_CUR:
		if ((loffset = llseek(fd, 0, SEEK_CUR)) == -1)
			err = -1;
		else
			loffset += offset;
		break;
	case SEEK_END:
		if (fstat(fd, &stat) == -1)
			err = -1;
		else
			loffset = offset + stat.st_size;
		break;
	default:
		errno = EINVAL;
		err = -1;
	}

	if (err)
		return (err);

	/* initialize kaio */
	if (!_kaio_ok)
		_kaio_init();

	/*
	 * Try kernel aio first.
	 * If errno is ENOTSUP, fall back to the lwp implementation.
	 */
	if ((_kaio_ok > 0) && (KAIO_SUPPORTED(fd))) {
		resultp->aio_errno = 0;
		kerr = (int)_kaio(((resultp->aio_return == AIO_INPROGRESS) ?
		    (mode | AIO_POLL_BIT) : mode),
		    fd, buf, bufsz, loffset, resultp);
		if (kerr == 0)
			return (0);
		else if (errno != ENOTSUP)
			return (-1);
		SET_KAIO_NOT_SUPPORTED(fd);
	}
	if (!__uaio_ok) {
		if (__uaio_init() == -1)
			return (-1);
	}

	aiorp = _aio_req_alloc();
	if (aiorp == (aio_req_t *)-1) {
		errno = EAGAIN;
		return (-1);
	}

	aiorp->req_op = mode;
	aiorp->req_resultp = resultp;
	ap = &(aiorp->req_args);
	ap->fd = fd;
	ap->buf = buf;
	ap->bufsz = bufsz;
	ap->offset = loffset;

	nextworker = ((mode == AIOWRITE) ? &__nextworker_wr : &__nextworker_rd);
	_aio_lock();
	if (_aio_hash_insert(resultp, aiorp)) {
		_aio_req_free(aiorp);
		_aio_unlock();
		errno = EINVAL;
		return (-1);
	} else {
		_aio_unlock();
		_aio_req_add(aiorp, nextworker, mode);
		return (0);
	}
}

int
aiocancel(aio_result_t *resultp)
{
	aio_req_t *aiorp;
	struct aio_worker *aiowp;
	int done = 0, canceled = 0;

	if (!__uaio_ok) {
		errno = EINVAL;
		return (-1);
	}

	_aio_lock();
	aiorp = _aio_hash_find(resultp);
	if (aiorp == NULL) {
		_aio_unlock();
		if (!_aio_outstand_cnt) {
			errno = EINVAL;
			return (-1);
		}
		errno = EACCES;
		return (-1);
	} else {
		aiowp = aiorp->req_worker;
		_lwp_mutex_lock(&aiowp->work_qlock1);
		(void) _aio_cancel_req(aiowp, aiorp, &canceled, &done);
		_lwp_mutex_unlock(&aiowp->work_qlock1);
		_aio_unlock();
		if (canceled)
			return (0);
		if (_aio_outstand_cnt)
			errno = EACCES;
		else
			errno = EINVAL;
		return (-1);
	}
}

/*
 * This must be asynch safe
 */
aio_result_t *
aiowait(struct timeval *uwait)
{
	aio_result_t *uresultp, *kresultp, *resultp;
	int dontblock;
	int timedwait = 0;
	int kaio_errno = 0;
	struct timeval curtime, end, *wait = NULL, twait;

	if (uwait) {
		if ((uwait->tv_sec > 0) || (uwait->tv_usec > 0)) {
			gettimeofday(&curtime, NULL);
			end.tv_sec = uwait->tv_sec + curtime.tv_sec;
			end.tv_usec = uwait->tv_usec + curtime.tv_usec;
			*(struct timeval *)&twait = *uwait;
			wait = &twait;
			timedwait++;
		} else {
			/* polling */
			kresultp = (aio_result_t *)_kaio(AIOWAIT, -1, 1);
			if (kresultp != (aio_result_t *)-1 &&
			    kresultp != NULL && kresultp != (aio_result_t *)1)
				return (kresultp);
			_aio_lock();
			uresultp = _aio_req_done();
			if (uresultp != NULL && uresultp !=
			    (aio_result_t *)-1) {
				_aio_unlock();
				return (uresultp);
			}
			_aio_unlock();
			if (uresultp == (aio_result_t *)-1 &&
				kresultp == (aio_result_t *)-1) {
				errno = EINVAL;
				return ((aio_result_t *)-1);
			} else
				return (NULL);
		}
	}
	/*CONSTCOND*/
	while (1) {
		_aio_lock();
		uresultp = _aio_req_done();
		if (uresultp != NULL && uresultp != (aio_result_t *)-1) {
			_aio_unlock();
			resultp = uresultp;
			break;
		}
		_aiowait_flag++;
		_aio_unlock();
		dontblock = (uresultp == (aio_result_t *)-1);
		kresultp = (aio_result_t *)_kaio(AIOWAIT, wait, dontblock);
		kaio_errno = errno;
		_aio_lock();
		_aiowait_flag--;
		_aio_unlock();
		if (kresultp == (aio_result_t *)1) {
			/* aiowait() awakened by an aionotify() */
			continue;
		} else if (kresultp != NULL && kresultp != (aio_result_t *)-1) {
			resultp = kresultp;
			break;
		} else if (kresultp == (aio_result_t *)-1 && kaio_errno ==
		    EINVAL && uresultp == (aio_result_t *)-1) {
			errno = kaio_errno;
			resultp = (aio_result_t *)-1;
			break;
		} else if (kresultp == (aio_result_t *)-1 &&
		    kaio_errno == EINTR) {
			errno = kaio_errno;
			resultp = (aio_result_t *)-1;
			break;
		} else if (timedwait) {
			gettimeofday(&curtime, NULL);
			wait->tv_sec = end.tv_sec - curtime.tv_sec;
			wait->tv_usec = end.tv_usec - curtime.tv_usec;
			if (wait->tv_sec < 0 || (wait->tv_sec == 0 &&
			    wait->tv_usec <= 0)) {
				resultp = NULL;
				break;
			}
		} else {
			ASSERT((kresultp == NULL && uresultp == NULL));
			resultp = NULL;
			continue;
		}
	}
	return (resultp);
}
/*
 * If closing by file descriptor: we will simply cancel all the outstanding
 * aio`s and return. Those aio's in question will have either noticed the
 * cancellation notice before, during, or after initiating io.
 */
int
aiocancel_all(int fd)
{
	aio_req_t *aiorp;
	aio_req_t **aiorpp;
	struct aio_worker *first, *next;
	int canceled = 0;
	int done = 0;
	int cancelall = 0;

	if (_aio_outstand_cnt == 0)
		return (AIO_ALLDONE);

	_aio_lock();
	/*
	 * cancel read requests from the read worker's queue.
	 */
	first = __nextworker_rd;
	next = first;
	do {
		_aio_cancel_work(next, fd, &canceled, &done);
	} while ((next = next->work_forw) != first);

	/*
	 * cancel write requests from the write workers queue.
	 */

	first = __nextworker_wr;
	next = first;
	do {
		_aio_cancel_work(next, fd, &canceled, &done);
	} while ((next = next->work_forw) != first);

	/*
	 * finally, check if there are requests on the done queue that
	 * should be canceled.
	 */
	if (fd < 0)
		cancelall = 1;
	aiorpp = &_aio_done_tail;
	while ((aiorp = *aiorpp) != NULL) {
		if (cancelall || aiorp->req_args.fd == fd) {
			*aiorpp = aiorp->req_next;
			_aio_donecnt--;
			_aio_hash_del(aiorp->req_resultp);
			_aio_req_free(aiorp);
		} else
			aiorpp = &aiorp->req_next;
	}
	if (cancelall) {
		ASSERT(_aio_donecnt == 0);
		_aio_done_head = NULL;
	}
	_aio_unlock();

	if (canceled && done == 0)
		return (AIO_CANCELED);
	else if (done && canceled == 0)
		return (AIO_ALLDONE);
	else if ((canceled + done == 0) && KAIO_SUPPORTED(fd))
		return ((int)_kaio(AIOCANCEL, fd, NULL));
	return (AIO_NOTCANCELED);
}

/*
 * cancel requests from a given work queue. if the file descriptor
 * parameter, fd, is non NULL, then only cancel those requests in
 * this queue that are to this file descriptor. if the "fd"
 * parameter is -1, then cancel all requests.
 */
static void
_aio_cancel_work(aio_worker_t *aiowp, int fd, int *canceled, int *done)
{
	aio_req_t *aiorp;

	_lwp_mutex_lock(&aiowp->work_qlock1);
	/*
	 * cancel queued requests first.
	 */
	aiorp = aiowp->work_tail1;
	while (aiorp != NULL) {
		if (fd < 0 || aiorp->req_args.fd == fd) {
			if (_aio_cancel_req(aiowp, aiorp, canceled, done)) {
				/*
				 * callers locks were dropped. aiorp is
				 * invalid, start traversing the list from
				 * the beginning.
				 */
				aiorp = aiowp->work_tail1;
				continue;
			}
		}
		aiorp = aiorp->req_next;
	}
	/*
	 * since the queued requests have been canceled, there can
	 * only be one inprogress request that shoule be canceled.
	 */
	if ((aiorp = aiowp->work_req) != NULL) {
		if (fd < 0 || aiorp->req_args.fd == fd) {
			(void) _aio_cancel_req(aiowp, aiorp, canceled, done);
			aiowp->work_req = NULL;
		}
	}
	_lwp_mutex_unlock(&aiowp->work_qlock1);
}

/*
 * cancel a request. return 1 if the callers locks were temporarily
 * dropped, otherwise return 0.
 */
int
_aio_cancel_req(aio_worker_t *aiowp, aio_req_t *aiorp, int *canceled, int *done)
{
	int ostate;
	int rwflg = 1;
	int siqueued;
	int canned;

	ASSERT(MUTEX_HELD(&__aio_mutex));
	ASSERT(MUTEX_HELD(&aiowp->work_qlock1));
	ostate = aiorp->req_state;
	if (ostate == AIO_REQ_CANCELED) {
		return (0);
	}
	if (ostate == AIO_REQ_DONE) {
		(*done)++;
		return (0);
	}
	if (ostate == AIO_REQ_FREE)
		return (0);
	if (aiorp->req_op == AIOFSYNC) {
		canned = aiorp->lio_head->lio_canned;
		aiorp->lio_head->lio_canned = 1;
		rwflg = 0;
		if (canned)
			return (0);
	}
	aiorp->req_state = AIO_REQ_CANCELED;
	_aio_req_del(aiowp, aiorp, ostate);
	if (ostate == AIO_REQ_INPROGRESS)
		_lwp_kill(aiowp->work_lid, SIGAIOCANCEL);
	_lwp_mutex_unlock(&aiowp->work_qlock1);
	_aio_hash_del(aiorp->req_resultp);
	_lwp_mutex_unlock(&__aio_mutex);
	siqueued = _aiodone(aiorp, aiorp->lio_head, rwflg, -1, ECANCELED);
	_lwp_mutex_lock(&__aio_mutex);
	_lwp_mutex_lock(&aiowp->work_qlock1);
	_lio_remove(aiorp->lio_head);
	if (!siqueued)
		_aio_req_free(aiorp);
	(*canceled)++;
	return (1);
}

/*
 * this is the worker's main routine. it keeps executing queued
 * requests until terminated. it blocks when its queue is empty.
 * All workers take work from the same queue.
 */
void
_aio_do_request(void *arglist)
{
	aio_worker_t *aiowp = (aio_worker_t *)arglist;
	struct aio_args *arg;
	aio_req_t *aiorp;		/* current AIO request */
	int ostate;
	ssize_t retval;
	int rwflg;

	aiowp->work_lid = _lwp_self();

cancelit:
	if (setjmp(aiowp->work_jmp_buf)) {
		_libc_sigprocmask(SIG_SETMASK, &_worker_set, NULL);
		goto cancelit;
	}

	/*CONSTCOND*/
	while (1) {
		int err = 0;

		while ((aiorp = _aio_req_get(aiowp)) == NULL) {
			_aio_idle(aiowp);
		}
#ifdef DEBUG
		_donecnt[aiowp->work_lid]++;
#endif
		arg = &aiorp->req_args;

		switch (aiorp->req_op) {
			case AIOREAD:
				retval = __libaio_pread(arg->fd, arg->buf,
				    arg->bufsz, arg->offset, &err);
				if (retval == -1 && err == ESPIPE) {
					err = 0;
					retval = __libaio_read(arg->fd,
					    arg->buf, arg->bufsz, &err);
				}
				rwflg = 1;
				break;
			case AIOWRITE:
				retval = __libaio_pwrite(arg->fd, arg->buf,
				    arg->bufsz, arg->offset, &err);
				if (retval == -1 && err == ESPIPE) {
					err = 0;
					retval = __libaio_write(arg->fd,
					    arg->buf, arg->bufsz, &err);
				}
				rwflg = 1;
				break;
			case AIOFSYNC:
				if (_aio_fsync_del(aiorp, aiorp->lio_head))
					continue;
				_lwp_mutex_lock(&aiowp->work_qlock1);
				ostate = aiorp->req_state;
				_lwp_mutex_unlock(&aiowp->work_qlock1);
				if (ostate == AIO_REQ_CANCELED) {
					_lwp_mutex_lock(&aiorp->req_lock);
					aiorp->req_canned = 1;
					_lwp_cond_broadcast(&aiorp->req_cancv);
					_lwp_mutex_unlock(&aiorp->req_lock);
					continue;
				}
				rwflg = 0;
				/*
				 * all writes for this fsync request are
				 * now acknowledged. now, make these writes
				 * visible.
				 */
				if (arg->offset == O_SYNC)
					retval = __libaio_fdsync(arg->fd,
					    O_SYNC, &err);
				else
					retval = __libaio_fdsync(arg->fd,
					    O_DSYNC, &err);
				break;
			default:
				_aiopanic("_aio_do_request, bad op\n");
		}
		_lwp_mutex_lock(&aiowp->work_qlock1);
		aiorp->req_state = AIO_REQ_DONE;
		_aio_cancel_off(aiowp);
		_lwp_mutex_unlock(&aiowp->work_qlock1);
		_aiodone(aiorp, aiorp->lio_head, rwflg, retval, err);
	}
}

/*
 * posix supports signal notification for completed aio requests.
 * when aio_do_requests() notices that an aio requests should send
 * a signal, the aio request is moved to the signal notification
 * queue. this routine drains this queue, and guarentees that the
 * signal notification is sent.
 */
void
_aio_send_sigev(void *arg)
{
	aio_req_t *rp;
	aio_worker_t *aiowp = (aio_worker_t *)arg;

	aiowp->work_lid = _lwp_self();

	/*CONSTCOND*/
	while (1) {

		while ((rp = _aio_req_get(aiowp)) == NULL) {
			_aio_idle(aiowp);
		}
		if (rp->aio_sigevent.sigev_notify == SIGEV_SIGNAL) {
			while (__sigqueue(__pid, rp->aio_sigevent.sigev_signo,
			    rp->aio_sigevent.sigev_value,
			    SI_ASYNCIO) == -1)
				yield();
		}
		if (rp->lio_signo) {
			while (__sigqueue(__pid, rp->lio_signo, rp->lio_sigval,
			    SI_ASYNCIO) == -1)
				yield();
		}
		_aio_lock();
		_aio_req_free(rp);
		_aio_unlock();
	}
}

/*
 * do the completion semantic for a request that was either canceled
 * by _aio_cancel_req(), or was completed by _aio_do_request(). return
 * the value 1 when a sigevent was queued, otherwise return 0.
 */
static int
_aiodone(aio_req_t *rp, aio_lio_t *head, int rwflg, ssize_t retval, int err)
{
	aio_result_t *resultp;
	int sigev;

	_aio_lock();

	if (POSIX_AIO(rp)) {
		sigev = (rp->aio_sigevent.sigev_notify == SIGEV_SIGNAL ||
		    (head && head->lio_signo));
		if (sigev)
			_aio_hash_del(rp->req_resultp);
	}

	if ((rwflg) && (POSIX_AIO(rp) || (err == ECANCELED)))
		_aio_outstand_cnt--;

	_aio_unlock();

	resultp = rp->req_resultp;
	resultp->aio_errno = err;
	resultp->aio_return = retval;
	if (POSIX_AIO(rp)) {
		rp->lio_signo = 0;
		rp->lio_sigval.sival_int = 0;
		if (head) {
			/*
			 * If all the lio requests have completed,
			 * signal the waiting process
			 */
			_lwp_mutex_lock(&head->lio_mutex);
			if (--head->lio_refcnt == 0) {
				if (head->lio_mode == LIO_WAIT)
					_lwp_cond_signal(&head->lio_cond_cv);
				else {
					rp->lio_signo = head->lio_signo;
					rp->lio_sigval = head->lio_sigval;
				}
			}
			_lwp_mutex_unlock(&head->lio_mutex);
		}
		if (sigev) {
			_aio_req_add(rp, &__workers_si, AIOSIGEV);
			return (1);
		}
	}
	return (0);
}

/*
 * delete fsync requests from list head until there is
 * only one left. return 0 when there is only one, otherwise
 * return a non-zero value.
 */
static int
_aio_fsync_del(aio_req_t *rp, aio_lio_t *head)
{
	int refcnt;

	_lwp_mutex_lock(&head->lio_mutex);
	if (head->lio_refcnt > 1 || head->lio_mode == LIO_DESTROY ||
	    head->lio_canned) {
		head->lio_nent--;
		refcnt = head->lio_refcnt--;
		_lwp_mutex_unlock(&head->lio_mutex);
		if (refcnt || head->lio_canned) {
			_lwp_mutex_lock(&__aio_mutex);
			_aio_req_free(rp);
			_lwp_mutex_unlock(&__aio_mutex);
			if (head->lio_canned) {
				ASSERT(refcnt >= 0);
				return (0);
			}
			return (1);
		}
		ASSERT(head->lio_mode == LIO_DESTROY);
		ASSERT(head->lio_nent == 0 && head->lio_refcnt == 0);
		_aio_remove(rp);
		return (0);
	}
	ASSERT(head->lio_refcnt == head->lio_nent);
	_lwp_mutex_unlock(&head->lio_mutex);
	return (0);
}

/*
 * worker is set idle when its work queue is empty. if the worker has
 * done some work, these completed requests are placed on a common
 * done list. the worker checks again that it has no more work and then
 * goes to sleep waiting for more work.
 */
void
_aio_idle(aio_worker_t *aiowp)
{
	/* put completed requests on aio_done_list */
	if (aiowp->work_done1)
		_aio_work_done(aiowp);

	_lwp_mutex_lock(&aiowp->work_lock);
	if (aiowp->work_cnt1 == 0) {
#ifdef DEBUG
		_idlecnt[aiowp->work_lid]++;
#endif
		aiowp->work_idleflg = 1;
		___lwp_cond_wait(&aiowp->work_idle_cv, &aiowp->work_lock, NULL);
		/*
		 * idle flag is cleared before worker is awakened
		 * by aio_req_add().
		 */
		return;
	}
	_lwp_mutex_unlock(&aiowp->work_lock);
}

/*
 * A worker's completed AIO requests are placed onto a global
 * done queue. The application is only sent a SIGIO signal if
 * the process has a handler enabled and it is not waiting via
 * aiowait().
 */
static void
_aio_work_done(struct aio_worker *aiowp)
{
	int done_cnt = 0;
	struct aio_req *head = NULL, *tail;

	_lwp_mutex_lock(&aiowp->work_qlock1);
	head = aiowp->work_prev1;
	head->req_next = NULL;
	tail = aiowp->work_tail1;
	done_cnt = aiowp->work_done1;
	aiowp->work_done1 = 0;
	aiowp->work_tail1 = aiowp->work_next1;
	if (aiowp->work_tail1 == NULL)
		aiowp->work_head1 = NULL;
	aiowp->work_prev1 = NULL;
	_lwp_mutex_unlock(&aiowp->work_qlock1);
	_lwp_mutex_lock(&__aio_mutex);
	_aio_donecnt += done_cnt;
	_aio_outstand_cnt -= done_cnt;
	ASSERT(_aio_donecnt > 0 && _aio_outstand_cnt >= 0);
	ASSERT(head != NULL && tail != NULL);

	if (_aio_done_tail == NULL) {
		_aio_done_head = head;
		_aio_done_tail = tail;
	} else {
		_aio_done_head->req_next = tail;
		_aio_done_head = head;
	}

	if (_aiowait_flag) {
		_lwp_mutex_unlock(&__aio_mutex);
		(void) _kaio(AIONOTIFY);
	} else {
		_lwp_mutex_unlock(&__aio_mutex);
		if (_sigio_enabled) {
			kill(__pid, SIGIO);
		}
	}
}

/*
 * the done queue consists of AIO requests that are in either the
 * AIO_REQ_DONE or AIO_REQ_CANCELED state. requests that were cancelled
 * are discarded. if the done queue is empty then NULL is returned.
 * otherwise the address of a done aio_result_t is returned.
 */
struct aio_result_t *
_aio_req_done(void)
{
	struct aio_req *next;
	aio_result_t *resultp;

	ASSERT(MUTEX_HELD(&__aio_mutex));

	if ((next = _aio_done_tail) != NULL) {
		_aio_done_tail = next->req_next;
		ASSERT(_aio_donecnt > 0);
		_aio_donecnt--;
		_aio_hash_del(next->req_resultp);
		resultp = next->req_resultp;
		ASSERT(next->req_state == AIO_REQ_DONE);
		_aio_req_free(next);
		return (resultp);
	}
	/* is queue empty? */
	if (next == NULL && _aio_outstand_cnt == 0) {
		return ((aio_result_t *)-1);
	}
	return (NULL);
}

/*
 * add an AIO request onto the next work queue. a circular list of
 * workers is used to choose the next worker. each worker has two
 * work queues. if the lock for the first queue is busy then the
 * request is placed on the second queue. the request is always
 * placed on one of the two queues depending on which one is locked.
 */
void
_aio_req_add(aio_req_t *aiorp, aio_worker_t **nextworker, int mode)
{
	struct aio_worker *aiowp;
	struct aio_worker *first;
	int clogged = 0;
	int found = 0;
	int load_bal_flg;
	int idleflg;
	int qactive;

	aiorp->req_next = NULL;
	ASSERT(*nextworker != NULL);
	aiowp = *nextworker;
	/*
	 * try to acquire the next worker's work queue. if it is locked,
	 * then search the list of workers until a queue is found unlocked,
	 * or until the list is completely traversed at which point another
	 * worker will be created.
	 */
	first = aiowp;
	if (mode == AIOREAD || mode == AIOWRITE) {
		_aio_lock();
		_aio_outstand_cnt++;
		_aio_unlock();
		load_bal_flg = 1;
	}
	switch (mode) {
		case AIOREAD:
			/* try to find an idle worker. */
			do {
				if (!_lwp_mutex_trylock(&aiowp->work_qlock1)) {
					if (aiowp->work_idleflg) {
						found = 1;
						break;
					}
					_lwp_mutex_unlock(&aiowp->work_qlock1);
				}
			} while ((aiowp = aiowp->work_forw) != first);
			if (found)
				break;
			/*FALLTHROUGH*/
		case AIOWRITE:
			while (_lwp_mutex_trylock(&aiowp->work_qlock1)) {
#ifdef DEBUG
				_qlocked++;
#endif
				if (((aiowp = aiowp->work_forw)) == first) {
					clogged = 1;
					break;
				}
			}
			/*
			 * create more workers when the workers appear
			 * overloaded. either all the workers are busy
			 * draining their queues, no worker's queue lock
			 * could be acquired, or the selected worker has
			 * exceeded its minimum work load, but has not
			 * exceeded the max number of workers.
			 */
			if (clogged) {
#ifdef DEBUG
				_new_workers++;
				_clogged++;
#endif
				if (_aio_worker_cnt < _max_workers) {
					if (_aio_create_worker(aiorp,
						aiorp->req_op))
						_aiopanic(
						"_aio_req_add: clogged");
					return;
				}

				/*
				 * No worker available and we have created
				 * _max_workers, keep going through the
				 * list until we get a lock
				 */
				while (_lwp_mutex_trylock(
					&aiowp->work_qlock1)) {
					/*
					 * give someone else a chance
					 */
					_yield();
					aiowp = aiowp->work_forw;
				}

			}
			ASSERT(MUTEX_HELD(&aiowp->work_qlock1));
			aiowp->work_minload1++;
			if (_aio_worker_cnt < _max_workers &&
			    aiowp->work_minload1 > _minworkload) {
				aiowp->work_minload1 = 0;
				_lwp_mutex_unlock(&aiowp->work_qlock1);
#ifdef DEBUG
				_qfullcnt[aiowp->work_lid]++;
				_new_workers++;
				_newworker[aiowp->work_lid]++;
				_avedone = _aio_submitcnt2/_new_workers;
#endif
				_lwp_mutex_lock(&__aio_mutex);
				*nextworker = aiowp->work_forw;
				_lwp_mutex_unlock(&__aio_mutex);
				if (_aio_create_worker(aiorp, aiorp->req_op))
					_aiopanic("aio_req_add: add worker");
				return;
			}
			break;
		case AIOFSYNC:
			aiorp->req_op = mode;
			/*FALLTHROUGH*/
		case AIOSIGEV:
			load_bal_flg = 0;
			_lwp_mutex_lock(&aiowp->work_qlock1);
			break;
	}
	/*
	 * Put request onto worker's work queue.
	 */
	if (aiowp->work_tail1 == NULL) {
		ASSERT(aiowp->work_cnt1 == 0);
		aiowp->work_tail1 = aiorp;
		aiowp->work_next1 = aiorp;
	} else {
		aiowp->work_head1->req_next = aiorp;
		if (aiowp->work_next1 == NULL)
			aiowp->work_next1 = aiorp;
	}
	aiorp->req_state = AIO_REQ_QUEUED;
	aiorp->req_worker = aiowp;
	aiowp->work_head1 = aiorp;
	qactive = aiowp->work_cnt1++;
	_lwp_mutex_unlock(&aiowp->work_qlock1);
	if (load_bal_flg) {
		_aio_lock();
		*nextworker = aiowp->work_forw;
		_aio_unlock();
	}
	/*
	 * Awaken worker if it is not currently active.
	 */
	if (!qactive) {
		_lwp_mutex_lock(&aiowp->work_lock);
		idleflg = aiowp->work_idleflg;
		aiowp->work_idleflg = 0;
		_lwp_mutex_unlock(&aiowp->work_lock);
		if (idleflg)
			_lwp_cond_signal(&aiowp->work_idle_cv);
	}
}

/*
 * get an AIO request for a specified worker. each worker has
 * two work queues. find the first one that is not empty and
 * remove this request from the queue and return it back to the
 * caller. if both queues are empty, then return a NULL.
 */
aio_req_t *
_aio_req_get(aio_worker_t *aiowp)
{
	aio_req_t *next;

	_lwp_mutex_lock(&aiowp->work_qlock1);
	if ((next = aiowp->work_next1) != NULL) {
		/*
		 * remove a POSIX request from the queue; the
		 * request queue is a singularly linked list
		 * with a previous pointer. The request is removed
		 * by updating the previous pointer.
		 *
		 * non-posix requests are left on the queue to
		 * eventually be placed on the done queue.
		 */

		if (next->req_type == AIO_POSIX_REQ) {
			if (aiowp->work_prev1 == NULL) {
				aiowp->work_tail1 = next->req_next;
				if (aiowp->work_tail1 == NULL)
					aiowp->work_head1 = NULL;
			} else {
				aiowp->work_prev1->req_next = next->req_next;
				if (aiowp->work_head1 == next)
					aiowp->work_head1 = next->req_next;
			}

		} else {
			aiowp->work_prev1 = next;
			ASSERT(aiowp->work_done1 >= 0);
			aiowp->work_done1++;
		}
		ASSERT(next != next->req_next);
		aiowp->work_next1 = next->req_next;
		ASSERT(aiowp->work_cnt1 >= 1);
		aiowp->work_cnt1--;
		if (next->req_op == AIOWRITE || next->req_op == AIOREAD)
			aiowp->work_minload1--;
#ifdef DEBUG
		_firstqcnt[aiowp->work_lid]++;
#endif
		next->req_state = AIO_REQ_INPROGRESS;
		_aio_cancel_on(aiowp);
	}
	aiowp->work_req = next;
	ASSERT(next != NULL || (next == NULL && aiowp->work_cnt1 == 0));
	_lwp_mutex_unlock(&aiowp->work_qlock1);
	return (next);
}

static void
_aio_req_del(aio_worker_t *aiowp, aio_req_t *rp, int ostate)
{
	aio_req_t **last, *next;

	ASSERT(aiowp != NULL);
	ASSERT(MUTEX_HELD(&aiowp->work_qlock1));
	if (POSIX_AIO(rp)) {
		if (ostate != AIO_REQ_QUEUED)
			return;
	}
	last = &aiowp->work_tail1;
	ASSERT(ostate == AIO_REQ_QUEUED || ostate == AIO_REQ_INPROGRESS);
	while ((next = *last) != NULL) {
		if (next == rp) {
			*last = next->req_next;
			if (aiowp->work_next1 == next)
				aiowp->work_next1 = next->req_next;
			if (aiowp->work_head1 == next)
				aiowp->work_head1 = next->req_next;
			if (aiowp->work_prev1 == next)
				aiowp->work_prev1 = next->req_next;
			if (ostate == AIO_REQ_QUEUED) {
				ASSERT(aiowp->work_cnt1 >= 1);
				aiowp->work_cnt1--;
			} else {
				ASSERT(ostate == AIO_REQ_INPROGRESS &&
				    !POSIX_AIO(rp));
				aiowp->work_done1--;
			}
			return;
		}
		last = &next->req_next;
	}
	/* NOTREACHED */
}

/*
 * An AIO request is indentified by an aio_result_t pointer. The AIO
 * library maps this aio_result_t pointer to its internal representation
 * via a hash table. This function adds an aio_result_t pointer to
 * the hash table.
 */
static int
_aio_hash_insert(aio_result_t *resultp, aio_req_t *aiorp)
{
	uintptr_t i;
	aio_req_t *next, **last;

	ASSERT(MUTEX_HELD(&__aio_mutex));
	i = AIOHASH(resultp);
	last = (_aio_hash + i);
	while ((next = *last) != NULL) {
		if (resultp == next->req_resultp)
			return (-1);
		last = &next->req_link;
	}
	*last = aiorp;
	ASSERT(aiorp->req_link == NULL);
	return (0);
}

/*
 * remove an entry from the hash table.
 */
struct aio_req *
_aio_hash_del(aio_result_t *resultp)
{
	struct aio_req *next, **prev;
	uintptr_t i;

	ASSERT(MUTEX_HELD(&__aio_mutex));
	i = AIOHASH(resultp);
	prev = (_aio_hash + i);
	while ((next = *prev) != NULL) {
		if (resultp == next->req_resultp) {
			*prev = next->req_link;
			return (next);
		}
		prev = &next->req_link;
	}
	ASSERT(next == NULL);
	return ((struct aio_req *)NULL);
}

/*
 *  find an entry on the hash table
 */
struct aio_req *
_aio_hash_find(aio_result_t *resultp)
{
	struct aio_req *next, **prev;
	uintptr_t i;

	/*
	 * no user AIO
	 */
	if (_aio_hash == NULL)
		return (NULL);

	i = AIOHASH(resultp);
	prev = (_aio_hash + i);
	while ((next = *prev) != NULL) {
		if (resultp == next->req_resultp) {
			return (next);
		}
		prev = &next->req_link;
	}
	return (NULL);
}
/*
 * Allocate and free aios. They are cached.
 */
aio_req_t *
_aio_req_alloc(void)
{
	aio_req_t *aiorp;
	int err;

	_aio_lock();
	while (_aio_freelist == NULL) {
		_aio_unlock();
		err = 0;
		_lwp_mutex_lock(&__aio_cachefillock);
		if (__aio_cachefilling)
			_lwp_cond_wait(&__aio_cachefillcv, &__aio_cachefillock);
		else
			err = _fill_aiocache(HASHSZ);
		_lwp_mutex_unlock(&__aio_cachefillock);
		if (err)
			return ((aio_req_t *)-1);
		_aio_lock();
	}
	aiorp = _aio_freelist;
	_aio_freelist = _aio_freelist->req_link;
	aiorp->req_type = 0;
	aiorp->req_link = NULL;
	aiorp->req_next = NULL;
	aiorp->lio_head = NULL;
	aiorp->aio_sigevent.sigev_notify = SIGEV_NONE;
	_aio_freelist_cnt--;
	_aio_unlock();
	return (aiorp);
}

/*
 * fill the aio request cache with empty aio request structures.
 */
int
_fill_aiocache(int n)
{
	aio_req_t *next, *aiorp, *first;
	int cnt;
	uintptr_t ptr;
	int i;

	__aio_cachefilling = 1;
	if ((ptr = (uintptr_t)malloc(sizeof (struct aio_req) * n)) == NULL) {
		__aio_cachefilling = 0;
		_lwp_cond_broadcast(&__aio_cachefillcv);
		return (-1);
	}
	if (ptr & 0x7)
		_aiopanic("_fill_aiocache");
	first = (struct aio_req *)ptr;
	next = first;
	cnt = n - 1;
	for (i = 0; i < cnt; i++) {
		aiorp = next++;
		aiorp->req_state = AIO_REQ_FREE;
		aiorp->req_link = next;
		memset((caddr_t)&aiorp->req_lock, 0, sizeof (lwp_mutex_t));
	}
	__aio_cachefilling = 0;
	_lwp_cond_broadcast(&__aio_cachefillcv);
	next->req_link = NULL;
	memset((caddr_t)&next->req_lock, 0, sizeof (lwp_mutex_t));
	_aio_lock();
	_aio_freelist_cnt = n;
	_aio_freelist = first;
	_aio_unlock();
	return (0);
}

/*
 * put an aio request back onto the freelist.
 */
void
_aio_req_free(aio_req_t *aiorp)
{
	ASSERT(MUTEX_HELD(&__aio_mutex));
	aiorp->req_state = AIO_REQ_FREE;
	aiorp->req_link = _aio_freelist;
	_aio_freelist = aiorp;
	_aio_freelist_cnt++;
}

/*
 * global aio lock that masks SIGIO signals.
 */
void
_aio_lock(void)
{
	__sigio_masked = 1;
	_lwp_mutex_lock(&__aio_mutex);
	__sigio_maskedcnt++;
}

/*
 * release global aio lock. send SIGIO signal if one
 * is pending.
 */
void
_aio_unlock(void)
{
	if (__sigio_maskedcnt--)
		__sigio_masked = 0;
	_lwp_mutex_unlock(&__aio_mutex);
	if (__sigio_pending)
		__aiosendsig();
}

/*
 * AIO interface for POSIX
 */
int
_aio_rw(aiocb_t *cb, aio_lio_t *lio_head, aio_worker_t **nextworker,
    int mode, int flg)
{
	aio_req_t *aiorp = NULL;
	aio_args_t *ap = NULL;
	int kerr;
	int umode;

	if (cb == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/* initialize kaio */
	if (!_kaio_ok)
		_kaio_init();

	cb->aio_state = NOCHECK;

	/*
	 * If _aio_rw() is called because a list I/O
	 * kaio() failed, we dont want to repeat the
	 * system call
	 */

	if (flg & AIO_KAIO) {
		/*
		 * Try kernel aio first.
		 * If errno is ENOTSUP, fall back to the lwp implementation.
		 */
		if ((_kaio_ok > 0) && (KAIO_SUPPORTED(cb->aio_fildes)))  {
			cb->aio_resultp.aio_errno = EINPROGRESS;
			kerr = (int)_kaio(mode, cb);
			if (kerr == 0) {
				cb->aio_state = CHECK;
				return (0);
			} else if (errno != ENOTSUP) {
				cb->aio_resultp.aio_errno = errno;
				cb->aio_resultp.aio_return = -1;
				cb->aio_state = NOCHECK;
				return (-1);
			}
			SET_KAIO_NOT_SUPPORTED(cb->aio_fildes);
		}
	}

	cb->aio_resultp.aio_errno = EINPROGRESS;
	cb->aio_state = USERAIO;

	if (!__uaio_ok) {
		if (__uaio_init() == -1)
			return (-1);
	}

	aiorp = _aio_req_alloc();
	if (aiorp == (aio_req_t *)-1) {
		errno = EAGAIN;
		return (-1);
	}

	/*
	 * If an LIO request, add the list head to the
	 * aio request
	 */
	aiorp->lio_head = lio_head;
	aiorp->req_type = AIO_POSIX_REQ;
	umode = ((mode == AIOFSYNC) ? mode : mode - AIOAREAD);
	aiorp->req_op = umode;

	if (cb->aio_sigevent.sigev_notify == SIGEV_SIGNAL) {
		aiorp->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
		aiorp->aio_sigevent.sigev_signo =
			cb->aio_sigevent.sigev_signo;
		aiorp->aio_sigevent.sigev_value.sival_int =
			cb->aio_sigevent.sigev_value.sival_int;
	}

	aiorp->req_resultp = &cb->aio_resultp;
	ap = &(aiorp->req_args);
	ap->fd = cb->aio_fildes;
	ap->buf = (caddr_t)cb->aio_buf;
	ap->bufsz = cb->aio_nbytes;
	ap->offset = cb->aio_offset;

	_aio_lock();
	if ((flg & AIO_NO_DUPS) && _aio_hash_insert(&cb->aio_resultp, aiorp)) {
		_aio_req_free(aiorp);
		_aio_unlock();
		errno = EINVAL;
		return (-1);
	} else {
		_aio_unlock();
		_aio_req_add(aiorp, nextworker, umode);
		return (0);
	}
}

#if	defined(_LARGEFILE64_SOURCE) && !defined(_LP64)
/*
 * 64-bit AIO interface for POSIX
 */
int
_aio_rw64(aiocb64_t *cb, aio_lio_t *lio_head, aio_worker_t **nextworker,
    int mode, int flg)
{
	aio_req_t *aiorp = NULL;
	aio_args_t *ap = NULL;
	int kerr;
	int umode;

	if (cb == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/* initialize kaio */
	if (!_kaio_ok)
		_kaio_init();

	cb->aio_state = NOCHECK;

	/*
	 * If _aio_rw() is called because a list I/O
	 * kaio() failed, we dont want to repeat the
	 * system call
	 */

	if (flg & AIO_KAIO) {
		/*
		 * Try kernel aio first.
		 * If errno is ENOTSUP, fall back to the lwp implementation.
		 */
		if ((_kaio_ok > 0) && (KAIO_SUPPORTED(cb->aio_fildes))) {
			cb->aio_resultp.aio_errno = EINPROGRESS;
			kerr = (int)_kaio(mode, cb);
			if (kerr == 0) {
				cb->aio_state = CHECK;
				return (0);
			} else if (errno != ENOTSUP) {
				cb->aio_resultp.aio_errno = errno;
				cb->aio_resultp.aio_return = -1;
				cb->aio_state = NOCHECK;
				return (-1);
			}
			SET_KAIO_NOT_SUPPORTED(cb->aio_fildes);
		}
	}

	cb->aio_resultp.aio_errno = EINPROGRESS;
	cb->aio_state = USERAIO;

	if (!__uaio_ok) {
		if (__uaio_init() == -1)
			return (-1);
	}


	aiorp = _aio_req_alloc();
	if (aiorp == (aio_req_t *)-1) {
		errno = EAGAIN;
		return (-1);
	}

	/*
	 * If an LIO request, add the list head to the
	 * aio request
	 */
	aiorp->lio_head = lio_head;
	aiorp->req_type = AIO_POSIX_REQ;
	umode = ((mode == AIOFSYNC) ? mode : mode - AIOAREAD64);
	aiorp->req_op = umode;

	if (cb->aio_sigevent.sigev_notify == SIGEV_SIGNAL) {
		aiorp->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
		aiorp->aio_sigevent.sigev_signo =
			cb->aio_sigevent.sigev_signo;
		aiorp->aio_sigevent.sigev_value.sival_int =
			cb->aio_sigevent.sigev_value.sival_int;
	}
	aiorp->req_resultp = &cb->aio_resultp;
	ap = &(aiorp->req_args);
	ap->fd = cb->aio_fildes;
	ap->buf = (caddr_t)cb->aio_buf;
	ap->bufsz = cb->aio_nbytes;
	ap->offset = cb->aio_offset;

	_aio_lock();
	if ((flg & AIO_NO_DUPS) && _aio_hash_insert(&cb->aio_resultp, aiorp)) {
		_aio_req_free(aiorp);
		_aio_unlock();
		errno = EINVAL;
		return (-1);
	} else {
		_aio_unlock();
		_aio_req_add(aiorp, nextworker, umode);
		return (0);
	}
}
#endif	/* (_LARGEFILE64_SOURCE) && !defined(_LP64) */

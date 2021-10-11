/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)scalls.c	1.2	99/12/06 SMI"

#include "liblwp.h"
#include <varargs.h>
#include <poll.h>
#include <stropts.h>
#include <sys/uio.h>
#include <sys/select.h>

mutex_t	fork_lock = DEFAULTMUTEX;
int doing_fork1;
void *fork1_freelist;

#pragma weak	fork1		= _fork1
#pragma weak	_liblwp_fork1	= _fork1
pid_t
_fork1(void)
{
	ulwp_t *self = curthread;
	void *freelist;
	void *ptr;
	pid_t pid;

	/*
	 * fork_lock is special -- we can't enter a critical region
	 * until after the lock is acquired because the second lwp to
	 * reach this point would become unstoppable and the first lwp
	 * would hang waiting for the second lwp to stop itself.
	 * Therefore we don't use lmutex_lock() to acquire fork_lock
	 * and we explicitly call enter_critical() after acquisition..
	 */
	(void) _mutex_lock(&fork_lock);
	enter_critical();

	doing_fork1 = 1;

	/* libc and user-defined locks */
	_prefork_handler();

	/* internal locks */
	_lprefork_handler();

	/*
	 * Stop using schedctl for the duration of the fork.
	 * The child will need to establish mappings of its own.
	 */
	self->ul_schedctl_called = 1;
	self->ul_schedctl = NULL;
	pid = __fork1();
	self->ul_schedctl_called = 0;
	self->ul_schedctl = NULL;

	if (pid == 0) {		/* child */
		_lpid = _getpid();
		_lpostfork_child_handler();
		_postfork_child_handler();
	} else {
		_lpostfork_parent_handler();
		_postfork_parent_handler();
	}

	/* don't call free() until after we exit the critical section */
	freelist = fork1_freelist;
	fork1_freelist = NULL;
	doing_fork1 = 0;
	(void) _mutex_unlock(&fork_lock);
	exit_critical();

	/* deal with deferred free()s */
	while ((ptr = freelist) != NULL) {
		freelist = *(void **)ptr;
		free(ptr);
	}

	return (pid);
}

/*
 * We want to define fork() such a way that if user links with
 * -lthread, the original Solaris implemntation of fork (i.e .
 * forkall) should be called. If the user links with -lpthread
 * which is a filter library for posix calls, we want to make
 * fork() behave like Solaris fork1().
 */
#pragma weak	fork		= _fork
#pragma weak	_liblwp_fork	= _fork
pid_t
_fork(void)
{
	ulwp_t *self = curthread;
	pid_t pid;

	if (_libpthread_loaded != 0)	/* if linked with -lpthread */
		return (_fork1());

	(void) _mutex_lock(&fork_lock);
	enter_critical();

	suspend_fork();
	/*
	 * Stop using schedctl for the duration of the fork.
	 * The child will need to establish mappings of its own.
	 */
	self->ul_schedctl_called = 1;
	self->ul_schedctl = NULL;
	pid = __fork();
	self->ul_schedctl_called = 0;
	self->ul_schedctl = NULL;

	if (pid == 0)		/* child */
		_lpid = _getpid();
	continue_fork(0);

	(void) _mutex_unlock(&fork_lock);
	exit_critical();
	return (pid);
}

/*
 * This is to make the library behave just like the standard libthread.
 * The behavior is supposed to changed to be just __alarm().
 */
#pragma weak alarm = _alarm
#pragma weak _liblwp_alarm = _alarm
unsigned
_alarm(unsigned sec)
{
	extern unsigned __alarm(unsigned);
	extern unsigned __lwp_alarm(unsigned);

	if (_libpthread_loaded != 0)
		return (__alarm(sec));
	else
		return (__lwp_alarm(sec));
}

/*
 * Hacks for system calls to provide cancellation
 * and improve garbage collection.
 */

#define	PROLOGUE						\
	{							\
		ulwp_t *self = curthread;			\
		self->ul_save_async = self->ul_cancel_async;	\
		if (!self->ul_cancel_disabled) {		\
			self->ul_cancel_async = 1;		\
			if (self->ul_cancel_pending)		\
				_pthread_exit(PTHREAD_CANCELED);\
		}						\
		_save_nv_regs(&self->ul_savedregs);		\
		self->ul_validregs = 1;

#define	EPILOGUE						\
		self->ul_validregs = 0;				\
		self->ul_cancel_async = self->ul_save_async;	\
	}

/*
 * Called from _thrp_join() (thr_join() is a cancellation point)
 */
int
lwp_wait(thread_t tid, thread_t *found)
{
	int error;

	PROLOGUE
	while ((error = __lwp_wait(tid, found)) == EINTR)
		;
	EPILOGUE
	return (error);
}

#pragma weak	_liblwp_poll	= poll
int
poll(struct pollfd *fds, nfds_t nfd, int timeout)
{
	extern int _poll(struct pollfd *, nfds_t, int);
	int rv;

	PROLOGUE
	rv = _poll(fds, nfd, timeout);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_read	= read
ssize_t
read(int fd, void *buf, size_t size)
{
	extern ssize_t _read(int, void *, size_t);
	ssize_t rv;

	PROLOGUE
	rv = _read(fd, buf, size);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_write	= write
ssize_t
write(int fd, const void *buf, size_t size)
{
	extern ssize_t _write(int, const void *, size_t);
	ssize_t rv;

	PROLOGUE
	rv = _write(fd, buf, size);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_getmsg	= getmsg
int
getmsg(int fd, struct strbuf *ctlptr, struct strbuf *dataptr,
	int *flagsp)
{
	extern int _getmsg(int, struct strbuf *, struct strbuf *, int *);
	int rv;

	PROLOGUE
	rv = _getmsg(fd, ctlptr, dataptr, flagsp);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_getpmsg	= getpmsg
int
getpmsg(int fd, struct strbuf *ctlptr, struct strbuf *dataptr,
	int *bandp, int *flagsp)
{
	extern int _getpmsg(int, struct strbuf *, struct strbuf *,
		int *, int *);
	int rv;

	PROLOGUE
	rv = _getpmsg(fd, ctlptr, dataptr, bandp, flagsp);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_putmsg	= putmsg
int
putmsg(int fd, const struct strbuf *ctlptr,
	const struct strbuf *dataptr, int flags)
{
	extern int _putmsg(int, const struct strbuf *,
		const struct strbuf *, int);
	int rv;

	PROLOGUE
	rv = _putmsg(fd, ctlptr, dataptr, flags);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp__xpg4_putmsg	= __xpg4_putmsg
int
__xpg4_putmsg(int fd, const struct strbuf *ctlptr,
	const struct strbuf *dataptr, int flags)
{
	extern int _putmsg(int, const struct strbuf *,
		const struct strbuf *, int);
	int rv;

	PROLOGUE
	rv = _putmsg(fd, ctlptr, dataptr, flags|MSG_XPG4);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_putpmsg	= putpmsg
int
putpmsg(int fd, const struct strbuf *ctlptr,
	const struct strbuf *dataptr, int band, int flags)
{
	extern int _putpmsg(int, const struct strbuf *,
		const struct strbuf *, int, int);
	int rv;

	PROLOGUE
	rv = _putpmsg(fd, ctlptr, dataptr, band, flags);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp__xpg4_putpmsg	= __xpg4_putpmsg
int
__xpg4_putpmsg(int fd, const struct strbuf *ctlptr,
	const struct strbuf *dataptr, int band, int flags)
{
	extern int _putpmsg(int, const struct strbuf *,
		const struct strbuf *, int, int);
	int rv;

	PROLOGUE
	rv = _putpmsg(fd, ctlptr, dataptr, band, flags|MSG_XPG4);
	EPILOGUE
	return (rv);
}

/* ARGSUSED */
int
_liblwp__nanosleep(const struct timespec *ts, struct timespec *tsr)
{
	int rv;

	PROLOGUE
	rv = _libc_nanosleep(ts, tsr);
	EPILOGUE
	return (rv);
}

#pragma weak	sleep		= _sleep
#pragma weak	_liblwp_sleep	= _sleep
unsigned
_sleep(unsigned sec)
{
	unsigned rem;
	struct timespec ts;
	struct timespec tsr;

	ts.tv_sec = (time_t)sec;
	ts.tv_nsec = 0;
	PROLOGUE
	(void) _libc_nanosleep(&ts, &tsr);
	EPILOGUE
	rem = (unsigned)tsr.tv_sec;
	if (tsr.tv_nsec >= NANOSEC / 2)
		rem++;
	return (rem);
}

#pragma weak	_liblwp_usleep	= usleep
int
usleep(useconds_t usec)
{
	struct timespec ts;

	ts.tv_sec = usec / MICROSEC;
	ts.tv_nsec = (long)(usec % MICROSEC) * 1000;
	PROLOGUE
	(void) _libc_nanosleep(&ts, NULL);
	EPILOGUE
	return (0);
}

#pragma weak	_liblwp_close	= close
int
close(int fildes)
{
	extern int _close(int);
	int rv;

	PROLOGUE
	rv = _close(fildes);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_creat	= creat
int
creat(const char *path, mode_t mode)
{
	extern int _creat(const char *, mode_t);
	int rv;

	PROLOGUE
	rv = _creat(path, mode);
	EPILOGUE
	return (rv);
}

#if !defined(_LP64)
#pragma weak	_liblwp_creat64	= creat64
int
creat64(const char *path, mode_t mode)
{
	extern int _creat64(const char *, mode_t);
	int rv;

	PROLOGUE
	rv = _creat64(path, mode);
	EPILOGUE
	return (rv);
}
#endif	/* !_LP64 */

#pragma weak	_liblwp_fcntl	= fcntl
int
fcntl(int fildes, int cmd, ...)
{
	extern int _fcntl(int, int, intptr_t);
	intptr_t arg;
	int rv;
	va_list ap;

	va_start(ap);
	arg = va_arg(ap, intptr_t);
	va_end(ap);
	if (cmd != F_SETLKW)
		rv = _fcntl(fildes, cmd, arg);
	else {
		PROLOGUE
		rv = _fcntl(fildes, cmd, arg);
		EPILOGUE
	}
	return (rv);
}

#pragma weak	_liblwp_fsync	= fsync
int
fsync(int fildes)
{
	extern int _fsync(int);
	int rv;

	PROLOGUE
	rv = _fsync(fildes);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_kill	= kill
#pragma weak	kill
int
kill(pid_t pid, int sig)
{
	extern int _kill(pid_t, int);

	return (_kill(pid, sig));	/* not a cancellation point */
}

#pragma weak	_liblwp_lockf	= lockf
int
lockf(int fildes, int function, off_t size)
{
	extern int _lockf(int, int, off_t);
	int rv;

	PROLOGUE
	rv = _lockf(fildes, function, size);
	EPILOGUE
	return (rv);
}

#if !defined(_LP64)
#pragma weak	_liblwp_lockf64	= lockf64
int
lockf64(int fildes, int function, off64_t size)
{
	extern int _lockf64(int, int, off64_t);
	int rv;

	PROLOGUE
	rv = _lockf64(fildes, function, size);
	EPILOGUE
	return (rv);
}
#endif	/* !_LP64 */

#pragma weak	_liblwp_msgrcv	= msgrcv
ssize_t
msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg)
{
	extern int _msgrcv(int, void *, size_t, long, int);
	int rv;

	PROLOGUE
	rv = _msgrcv(msqid, msgp, msgsz, msgtyp, msgflg);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_msgsnd	= msgsnd
int
msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
{
	extern int _msgsnd(int, const void *, size_t, int);
	int rv;

	PROLOGUE
	rv = _msgsnd(msqid, msgp, msgsz, msgflg);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_msync	= msync
int
msync(caddr_t addr, size_t len, int flags)
{
	extern int _msync(caddr_t, size_t, int);
	int rv;

	PROLOGUE
	rv = _msync(addr, len, flags);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_open	= open
int
open(const char *path, int oflag, ...)
{
	extern int _open(const char *, int, mode_t);
	mode_t mode = 0;
	int rv;
	va_list ap;

	va_start(ap);
	mode = va_arg(ap, mode_t);
	va_end(ap);
	PROLOGUE
	rv = _open(path, oflag, mode);
	EPILOGUE
	return (rv);
}

#if !defined(_LP64)
#pragma weak	_liblwp_open64	= open64
int
open64(const char *path, int oflag, ...)
{
	extern int _open64(const char *, int, mode_t);
	mode_t mode = 0;
	int rv;
	va_list ap;

	va_start(ap);
	mode = va_arg(ap, mode_t);
	va_end(ap);
	PROLOGUE
	rv = _open64(path, oflag, mode);
	EPILOGUE
	return (rv);
}
#endif	/* !_LP64 */

#pragma weak	_liblwp_pause	= pause
int
pause(void)
{
	extern int _pause(void);
	int rv;

	PROLOGUE
	rv = _pause();
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_pread	= pread
ssize_t
pread(int fildes, void *buf, size_t nbyte, off_t offset)
{
	extern ssize_t _pread(int, void *, size_t, off_t);
	ssize_t rv;

	PROLOGUE
	rv = _pread(fildes, buf, nbyte, offset);
	EPILOGUE
	return (rv);
}

#if !defined(_LP64)
#pragma weak	_liblwp_pread64	= pread64
ssize_t
pread64(int fildes, void *buf, size_t nbyte, off64_t offset)
{
	extern ssize_t _pread64(int, void *, size_t, off64_t);
	ssize_t rv;

	PROLOGUE
	rv = _pread64(fildes, buf, nbyte, offset);
	EPILOGUE
	return (rv);
}
#endif	/* !_LP64 */

#pragma weak	_liblwp_pwrite	= pwrite
ssize_t
pwrite(int fildes, const void *buf, size_t nbyte, off_t offset)
{
	extern ssize_t _pwrite(int, const void *, size_t, off_t);
	ssize_t rv;

	PROLOGUE
	rv = _pwrite(fildes, buf, nbyte, offset);
	EPILOGUE
	return (rv);
}

#if !defined(_LP64)
#pragma weak	_liblwp_pwrite64	= pwrite64
ssize_t
pwrite64(int fildes, const void *buf, size_t nbyte, off64_t offset)
{
	extern ssize_t _pwrite64(int, const void *, size_t, off64_t);
	ssize_t rv;

	PROLOGUE
	rv = _pwrite64(fildes, buf, nbyte, offset);
	EPILOGUE
	return (rv);
}
#endif	/* !_LP64 */

#pragma weak	_liblwp_readv	= readv
ssize_t
readv(int fildes, const struct iovec *iov, int iovcnt)
{
	extern ssize_t _readv(int, const struct iovec *, int);
	ssize_t rv;

	PROLOGUE
	rv = _readv(fildes, iov, iovcnt);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_select	= select
int
select(int nfds, fd_set *readfds, fd_set *writefds,
	fd_set *errorfds, struct timeval *timeout)
{
	extern int _select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
	int rv;

	PROLOGUE
	rv = _select(nfds, readfds, writefds, errorfds, timeout);
	EPILOGUE
	return (rv);
}

int
setcontext(const ucontext_t *ucp)
{
	/* never returns */
	return (_setcontext(ucp));
}

#pragma weak	_liblwp_sigpause	= sigpause
int
sigpause(int sig)
{
	extern int _sigpause(int);
	int rv;

	PROLOGUE
	rv = _sigpause(sig);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_sigpending	= sigpending
int
sigpending(sigset_t *set)
{
	extern int _sigpending(sigset_t *);
	int rv;

	rv = _sigpending(set);	/* not a cancellation point */
	if (rv == 0)
		delete_reserved_signals(set);
	return (rv);
}

#pragma weak	sigsuspend		= _sigsuspend
#pragma weak	_liblwp_sigsuspend	= _sigsuspend
int
_sigsuspend(const sigset_t *set)
{
	extern int __sigsuspend_trap(const sigset_t *);
	sigset_t lset;
	int rv;

	lset = *set;
	delete_reserved_signals(&lset);
	PROLOGUE
	rv = __sigsuspend_trap(&lset);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp__sigtimedwait	= __sigtimedwait
int
__sigtimedwait(const sigset_t *set, siginfo_t *info,
	const struct timespec *timeout)
{
	extern int _libc_sigtimedwait(const sigset_t *, siginfo_t *,
		const struct timespec *);
	sigset_t lset;
	int rv;

	lset = *set;
	delete_reserved_signals(&lset);
	PROLOGUE
	rv = _libc_sigtimedwait(&lset, info, timeout);
	EPILOGUE
	return (rv);
}

#pragma weak	sigwait		= _sigwait
#pragma weak	_liblwp_sigwait	= _sigwait
int
_sigwait(const sigset_t *set)
{
	extern int _libc_sigtimedwait(const sigset_t *, siginfo_t *,
		const struct timespec *);
	sigset_t lset;
	int rv;

	lset = *set;
	delete_reserved_signals(&lset);
	PROLOGUE
	rv = _libc_sigtimedwait(&lset, NULL, NULL);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_tcdrain	= tcdrain
int
tcdrain(int fildes)
{
	extern int _tcdrain(int);
	int rv;

	PROLOGUE
	rv = _tcdrain(fildes);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_wait	= wait
pid_t
wait(int *stat_loc)
{
	extern pid_t _wait(int *);
	pid_t rv;

	PROLOGUE
	rv = _wait(stat_loc);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_wait3	= wait3
pid_t
wait3(int *statusp, int options, struct rusage *rusage)
{
	extern pid_t _wait3(int *, int, struct rusage *);
	pid_t rv;

	PROLOGUE
	rv = _wait3(statusp, options, rusage);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_waitid	= waitid
int
waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options)
{
	extern int _waitid(idtype_t, id_t, siginfo_t *, int);
	int rv;

	PROLOGUE
	rv = _waitid(idtype, id, infop, options);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_waitpid	= waitpid
pid_t
waitpid(pid_t pid, int *stat_loc, int options)
{
	extern pid_t _waitpid(pid_t, int *, int);
	pid_t rv;

	PROLOGUE
	rv = _waitpid(pid, stat_loc, options);
	EPILOGUE
	return (rv);
}

#pragma weak	_liblwp_writev	= writev
ssize_t
writev(int fildes, const struct iovec *iov, int iovcnt)
{
	extern ssize_t _writev(int, const struct iovec *, int);
	ssize_t rv;

	PROLOGUE
	rv = _writev(fildes, iov, iovcnt);
	EPILOGUE
	return (rv);
}

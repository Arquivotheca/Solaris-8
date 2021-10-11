#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)posix4.spec	1.2	99/05/14 SMI"
#
# lib/librt/spec/posix4.spec

function	aio_cancel
include		<aio.h>
declaration	int aio_cancel(int fildes, struct aiocb *aiocbp)
version		SUNW_0.7
errno		EBADF ENOSYS
end

function	aio_fsync
include		<aio.h>
declaration	int aio_fsync(int op, aiocb_t *aiocbp)
version		SUNW_0.7
errno		EAGAIN EBADF EINVAL ENOSYS
end

function	aio_read
include		<aio.h>
declaration	int aio_read(struct aiocb *aiocbp)
version		SUNW_0.7
errno		EAGAIN ENOSYS EBADF EINVAL ECANCELED EFBIG
end

function	aio_write
include		<aio.h>
declaration	int aio_write(struct aiocb *aiocbp)
version		SUNW_0.7
errno		EAGAIN ENOSYS EBADF EINVAL ECANCELED EFBIG
end

function	aio_return
include		<aio.h>
declaration	ssize_t aio_return(struct aiocb * aiocbp)
version		SUNW_0.7
errno		EINVAL ENOSYS
end

function	aio_error
include		<aio.h>
declaration	int aio_error(const struct aiocb *aiocbp)
version		SUNW_0.7
errno		EINVAL ENOSYS
end

function	aio_suspend
include		<aio.h>
declaration	int aio_suspend(const struct aiocb *const list[], int nent, \
			const struct timespec *timeout)
version		SUNW_0.7
errno		EAGAIN EINTR ENOSYS
end

function	fdatasync
include		<unistd.h>
declaration	int fdatasync(int fildes)
version		SUNW_0.7
errno		EBADF EINVAL ENOSYS
end

function	lio_listio
include		<aio.h>
declaration	int lio_listio(int mode, struct aiocb *const list[], int nent, \
			struct sigevent *sig)
version		SUNW_0.7
errno		EAGAIN EINVAL EINTR EIO ENOSYS ECANCELED \
			EINPROGRESS EOVERFLOW EFBIG
end

function	aio_cancel64 extends librt/spec/posix4.spec aio_cancel
declaration	int aio_cancel64(int fildes, struct aiocb64 *aiocbp)
arch		i386 sparc
version		SUNW_1.1
end

function	aio_error64 extends librt/spec/posix4.spec aio_error
declaration	int aio_error64(const struct aiocb64 *aiocbp)
arch		i386 sparc
version		SUNW_1.1
end

function	aio_fsync64 extends librt/spec/posix4.spec aio_fsync
declaration	int aio_fsync64(int op, struct aiocb64 *aiocbp)
arch		i386 sparc
version		SUNW_1.1
end

function	aio_read64 extends librt/spec/posix4.spec aio_read
declaration	int aio_read64(struct aiocb64 *aiocbp)
arch		i386 sparc
version		SUNW_1.1
end

function	aio_return64 extends librt/spec/posix4.spec aio_return
declaration	ssize_t aio_return64(struct aiocb64 * aiocbp)
arch		i386 sparc
version		SUNW_1.1
end

function	aio_suspend64 extends librt/spec/posix4.spec aio_suspend
declaration	int aio_suspend64(const struct aiocb64 *const list[], \
			int nent, const struct timespec *timeout)
arch		i386 sparc
version		SUNW_1.1
end

function	aio_write64 extends librt/spec/posix4.spec aio_write
declaration	int aio_write64(struct aiocb64 *aiocbp)
arch		i386 sparc
version		SUNW_1.1
end

function	lio_listio64 extends librt/spec/posix4.spec lio_listio
declaration	int lio_listio64(int mode, struct aiocb64 *const list[], \
			int nent, struct sigevent *sig)
arch		i386 sparc
version		SUNW_1.1
end

function	mq_close
include		<mqueue.h>
declaration	int mq_close(mqd_t mqdes)
version		SUNW_0.7
errno		EBADF ENOSYS
exception	$return == -1
end

function	mq_notify
include		<mqueue.h>
declaration	int mq_notify(mqd_t mqdes, const struct sigevent *notification)
version		SUNW_0.7
errno		EBADF EBUSY ENOSYS
exception	$return == -1
end

function	mq_open
include		<mqueue.h>
declaration	mqd_t mq_open(const char *name, int oflag, ...)
version		SUNW_0.7
errno		EACCESS EEXIST EINTR EINVAL EMFILE ENAMETOOLONG ENFILE \
			ENOENT ENOSPC ENOSYS
exception	$return == (mqd_t)(-1)
end

function	mq_receive
include		<mqueue.h>
declaration	ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, \
			size_t msg_len, unsigned int *msg_prio)
version		SUNW_0.7
errno		EAGAIN EBADF EMSGSIZE EINTR ENOSYS
exception	$return == (ssize_t)(-1)
end

function	mq_send
include		<mqueue.h>, <unistd.h>
declaration	int mq_send(mqd_t mqdes, const char *msg_ptr, \
			size_t msg_len, unsigned int msg_prio)
version		SUNW_0.7
errno		EAGAIN EBADF EINTR EMSGSIZE ENOSYS
exception	$return == -1
end

function	mq_setattr
include		<mqueue.h>
declaration	int mq_setattr(mqd_t mqdes, const struct mq_attr *mqstat, \
			struct mq_attr* omqstat)
version		SUNW_0.7
errno		EBADF ENOSYS
exception	$return == -1
end

function	mq_getattr
include		<mqueue.h>
declaration	int mq_getattr(mqd_t mqdes, struct mq_attr *mqstat)
version		SUNW_0.7
errno		EBADF ENOSYS
exception	$return == -1
end

function	mq_unlink
include		<mqueue.h>
declaration	int mq_unlink(const char *name)
version		SUNW_0.7
errno		EACCESS ENAMETOOLONG ENOENT ENOSYS
exception	$return == -1
end

function	nanosleep
include		<time.h>
declaration	int nanosleep(const struct timespec *rqtp, \
			struct timespec *rmtp)
version		SUNW_0.7
errno		EINTR EINVAL ENOSYS
end

function	sched_get_priority_max
include		<sched.h>
declaration	int sched_get_priority_max(int policy)
version		SUNW_0.7
errno		EINVAL ENOSYS ESRCH
end

function	sched_get_priority_min
include		<sched.h>
declaration	int sched_get_priority_min(int policy)
version		SUNW_0.7
errno		EINVAL ENOSYS ESRCH
end

function	sched_rr_get_interval
include		<sched.h>
declaration	int sched_rr_get_interval(pid_t pid, struct timespec *interval)
version		SUNW_0.7
errno		EINVAL ENOSYS ESRCH
end

function	sched_setparam
include		<sched.h>
declaration	int sched_setparam(pid_t pid, const struct sched_param *param)
version		SUNW_0.7
errno		EINVAL ENOSYS EPERM ESRCH
end

function	sched_getparam
include		<sched.h>
declaration	int sched_getparam(pid_t pid, struct sched_param *param)
version		SUNW_0.7
errno		EINVAL ENOSYS EPERM ESRCH
end

function	sched_setscheduler
include		<sched.h>
declaration	int sched_setscheduler(pid_t pid, int policy, \
			const struct sched_param *param)
version		SUNW_0.7
errno		EINVAL ENOSYS EPERM ESRCH
end

function	sched_getscheduler
include		<sched.h>
declaration	int sched_getscheduler(pid_t pid)
version		SUNW_0.7
errno		EINVAL ENOSYS EPERM ESRCH
end

function	sched_yield
include		<sched.h>
declaration	int sched_yield(void)
version		SUNW_0.7
errno		ENOSYS
end

function	sem_close
include		<semaphore.h>
declaration	int sem_close(sem_t *sem)
version		SUNW_0.7
errno		EINVAL ENOSYS
end

function	sem_destroy
include		<semaphore.h>
declaration	int sem_destroy(sem_t *sem)
version		SUNW_0.7
errno		EINVAL ENOSYS EBUSY
end

function	sem_getvalue
include		<semaphore.h>
declaration	int sem_getvalue(sem_t *sem, int *sval)
version		SUNW_0.7
errno		EINVAL ENOSYS
end

function	sem_init
include		<semaphore.h>, <unistd.h>
declaration	int sem_init(sem_t *sem, int pshared, unsigned int value)
version		SUNW_0.7
errno		EINVAL ENOSPC ENOSYS EPERM
end

function	sem_open
include		<semaphore.h>, <unistd.h>, <sys/stat.h>
declaration	sem_t *sem_open(const char *name, int oflag, ...)
version		SUNW_0.7
errno		EACCES EEXIST EINTR EINVAL EMFILE ENAMETOOLONG ENFILE \
			ENOENT ENOSPC ENOSYS
end

function	sem_post
include		<semaphore.h>
declaration	int sem_post(sem_t *sem)
version		SUNW_0.7
errno		EINVAL ENOSYS
end

function	sem_unlink
include		<semaphore.h>
declaration	int sem_unlink(const char *name)
version		SUNW_0.7
errno		EACCES ENAMETOOLONG ENOENT ENOSYS
end

function	sem_wait
include		<semaphore.h>
declaration	int sem_wait(sem_t *sem)
version		SUNW_0.7
errno		EAGAIN EINVAL EINTR ENOSYS EDEADLK
end

function	sem_trywait
include		<semaphore.h>
declaration	int sem_trywait(sem_t *sem)
version		SUNW_0.7
errno		EAGAIN EINVAL EINTR ENOSYS EDEADLK
end

function	shm_open
include		<sys/mman.h>, <sys/types.h>, <sys/stat.h>, <fcntl.h>
declaration	int shm_open(const char *name, int oflag, mode_t mode)
version		SUNW_0.7
errno		EACCES EEXIST EINTR EINVAL EMFILE ENAMETOOLONG ENFILE \
			ENOENT ENOSPC ENOSYS
end

function	shm_unlink
declaration	int shm_unlink(const char *name)
version		SUNW_0.7
errno		EACCES ENAMETOOLONG ENOENT ENOSYS
end

function	sigqueue
include		<signal.h>
declaration	int  sigqueue(pid_t  pid, int signo, const union sigval value)
version		SUNW_0.7
errno		EAGAIN EINVAL ENOSYS EPERM ESRCH
end

function	sigwaitinfo
include		<signal.h>
declaration	int sigwaitinfo(const sigset_t *set, siginfo_t *info)
version		SUNW_0.7
errno		EINTR ENOSYS EAGAIN EINVAL
end

function	sigtimedwait
include		<signal.h>
declaration	int sigtimedwait(const sigset_t *set, siginfo_t *info, \
			const struct timespec *timeout)
version		SUNW_0.7
errno		EINTR ENOSYS EAGAIN EINVAL
end

function	timer_create
include		<signal.h>, <time.h>
declaration	int timer_create(clockid_t clock_id, struct sigevent *evp, \
			timer_t *timerid)
version		SUNW_0.7
errno		EAGAIN EINVAL ENOSYS
end

function	timer_delete
include		<time.h>
declaration	int timer_delete(timer_t timerid)
version		SUNW_0.7
errno		EINVAL ENOSYS
end

function	timer_settime
include		<time.h>
declaration	int timer_settime(timer_t timerid, int flags, \
			const struct itimerspec *value, \
			struct itimerspec *ovalue)
version		SUNW_0.7
errno		EINVAL ENOSYS
end

function	timer_gettime
include		<time.h>
declaration	int timer_gettime(timer_t timerid, struct itimerspec *value)
version		SUNW_0.7
errno		EINVAL ENOSYS
end

function	timer_getoverrun
include		<time.h>
declaration	int timer_getoverrun(timer_t timerid)
version		SUNW_0.7
errno		EINVAL ENOSYS
end

function	clock_settime
include		<time.h>
declaration	int clock_settime(clockid_t clock_id, const struct timespec *tp)
version		SUNW_0.7
errno		EINVAL ENOSYS EPERM
end

function	clock_gettime
include		<time.h>
declaration	int clock_gettime(clockid_t clock_id, struct timespec *tp)
version		SUNW_0.7
errno		EINVAL ENOSYS EPERM
end

function	clock_getres
include		<time.h>
declaration	int clock_getres(clockid_t clock_id, struct timespec *res)
version		SUNW_0.7
errno		EINVAL ENOSYS EPERM
end

function	_clock_getres
version		SUNWprivate_1.1
end

function	_clock_gettime
version		SUNWprivate_1.1
end

function	_clock_settime
version		SUNWprivate_1.1
end

function	_nanosleep
version		SUNWprivate_1.1
end

function	_timer_create
version		SUNWprivate_1.1
end

function	_timer_delete
version		SUNWprivate_1.1
end

function	_timer_getoverrun
version		SUNWprivate_1.1
end

function	_timer_gettime
version		SUNWprivate_1.1
end

function	_timer_settime
version		SUNWprivate_1.1
end

function	fork extends libc/spec/sys.spec fork
version		SUNW_1.1
end

function	close extends libc/spec/sys.spec
version		SUNW_1.2
end

#
# Weak Specs
#
function	__posix_aio_fork
weak		fork
version		SUNWprivate_1.1
end

function	_sem_open
weak		sem_open
version		SUNWprivate_1.1
end

function	_sem_close
weak		sem_close
version		SUNWprivate_1.1
end

function	_sem_unlink
weak		sem_unlink
version		SUNWprivate_1.1
end

function	_sem_init
weak		sem_init
version		SUNWprivate_1.1
end

function	_sem_destroy
weak		sem_destroy
version		SUNWprivate_1.1
end

function	_sem_wait
weak		sem_wait
version		SUNWprivate_1.1
end

function	_sem_trywait
weak		sem_trywait
version		SUNWprivate_1.1
end

function	_sem_post
weak		sem_post
version		SUNWprivate_1.1
end

function	_sem_getvalue
weak		sem_getvalue
version		SUNWprivate_1.1
end

function	_sigwaitinfo
weak		sigwaitinfo
version		SUNWprivate_1.1
end

function	_sigtimedwait
weak		sigtimedwait
version		SUNWprivate_1.1
end

function	_sigqueue
weak		sigqueue
version		SUNWprivate_1.1
end

function	__posix_aio_close
weak		close
version		SUNWprivate_1.1
end

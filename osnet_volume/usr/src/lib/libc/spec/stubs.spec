#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)stubs.spec	1.6	99/10/25 SMI"
#
# lib/libc/spec/stubs.spec

function	pthread_atfork
include		<pthread.h>, <sys/types.h>
declaration	int pthread_atfork(void *prepare, void *parent, void *child)
version		SUNW_1.1
errno		ENOMEM
end

function	pthread_attr_destroy
include		<pthread.h>
declaration	int pthread_attr_destroy(pthread_attr_t *attr)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_getdetachstate
include		<pthread.h>
declaration	int pthread_attr_getdetachstate(const pthread_attr_t *attr, \
			int *detachstate)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_getinheritsched
include		<pthread.h>
declaration	int pthread_attr_getinheritsched(const pthread_attr_t *attr, \
			int *inheritsched)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_getschedparam
include		<pthread.h>
declaration	int pthread_attr_getschedparam(const pthread_attr_t *attr, \
			struct sched_param *param)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_getschedpolicy
include		<pthread.h>
declaration	int pthread_attr_getschedpolicy(const pthread_attr_t *attr, \
			int *policy)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_getscope
include		<pthread.h>
declaration	int pthread_attr_getscope(const pthread_attr_t *attr, \
			int *contentionscope)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_getstackaddr
include		<pthread.h>
declaration	int pthread_attr_getstackaddr(const pthread_attr_t *attr, \
			void **stackaddr)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_getstacksize
include		<pthread.h>
declaration	int pthread_attr_getstacksize(const pthread_attr_t *attr, \
			size_t *stacksize)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_init
include		<pthread.h>
declaration	int pthread_attr_init(pthread_attr_t *attr)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_setdetachstate
include		<pthread.h>
declaration	int pthread_attr_setdetachstate(pthread_attr_t *attr, \
			int detachstate)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_setinheritsched
include		<pthread.h>
declaration	int pthread_attr_setinheritsched(pthread_attr_t *attr, \
			int inheritsched)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_setschedparam
include		<pthread.h>
declaration	int pthread_attr_setschedparam(pthread_attr_t *attr, \
			const struct sched_param *param)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_setschedpolicy
include		<pthread.h>
declaration	int pthread_attr_setschedpolicy(pthread_attr_t *attr, \
			int policy)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_setscope
include		<pthread.h>
declaration	int pthread_attr_setscope(pthread_attr_t *attr, \
			int contentionscope)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_setstackaddr
include		<pthread.h>
declaration	int pthread_attr_setstackaddr(pthread_attr_t *attr, \
			void *stackaddr)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_attr_setstacksize
include		<pthread.h>
declaration	int pthread_attr_setstacksize(pthread_attr_t *attr, \
			size_t stacksize)
version		SUNW_1.1
errno		ENOMEM EINVAL ENOTSUP
end

function	pthread_cancel
include		<pthread.h>
declaration	int pthread_cancel(pthread_t target_thread)
version		SUNW_1.1
errno		ESRCH
end

function	pthread_cond_broadcast
include		<pthread.h>
declaration	int pthread_cond_broadcast(pthread_cond_t *cond)
version		SUNW_0.9
errno		EINVAL
end

function	pthread_cond_destroy
include		<pthread.h>
declaration	int pthread_cond_destroy(pthread_cond_t *cond)
version		SUNW_0.9
errno		EBUSY, EINVAL
end

function	pthread_cond_init
include		<pthread.h>
declaration	int pthread_cond_init(pthread_cond_t *cond, \
			const pthread_condattr_t *attr)
version		SUNW_0.9
errno		EAGAIN, ENOMEM, EBUSY, EINVAL
end

function	pthread_cond_signal
include		<pthread.h>
declaration	int pthread_cond_signal(pthread_cond_t *cond)
version		SUNW_0.9
errno		EINVAL
end

function	pthread_cond_timedwait
include		<pthread.h>
declaration	int pthread_cond_timedwait(pthread_cond_t *cond, \
			pthread_mutex_t *mutex, const struct timespec *abstime)
version		SUNW_0.9
errno		ETIMEDOUT, EINVAL
end

function	pthread_cond_wait
include		<pthread.h>
declaration	int pthread_cond_wait(pthread_cond_t *cond, \
			pthread_mutex_t *mutex)
version		SUNW_0.9
errno		EINVAL
end

function	pthread_condattr_destroy
include		<pthread.h>
declaration	int pthread_condattr_destroy(pthread_condattr_t *attr)
version		SUNW_0.9
errno		ENOMEM EINVAL
end

function	pthread_condattr_getpshared
include		<pthread.h>
declaration	int pthread_condattr_getpshared( \
			const pthread_condattr_t *attr, int *process_shared)
version		SUNW_0.9
errno		ENOMEM EINVAL
end

function	pthread_condattr_init
include		<pthread.h>
declaration	int pthread_condattr_init(pthread_condattr_t *attr)
version		SUNW_0.9
errno		ENOMEM EINVAL
end

function	pthread_condattr_setpshared
include		<pthread.h>
declaration	int pthread_condattr_setpshared(pthread_condattr_t *attr, \
			int process_shared)
version		SUNW_0.9
errno		ENOMEM EINVAL
end

function	pthread_create
include		<pthread.h>
declaration	int pthread_create(pthread_t *thread, \
			const pthread_attr_t *attr, \
			void *(*start_routine)(void*), void *arg)
version		SUNW_1.1
errno		EAGAIN EINVAL ENOMEM
end

function	pthread_detach
include		<pthread.h>
declaration	int pthread_detach(pthread_t tid)
version		SUNW_1.1
errno		EINVAL ESRCH
end

function	pthread_equal
include		<pthread.h>
declaration	int pthread_equal(pthread_t t1, pthread_t t2)
version		SUNW_1.1
end

function	pthread_exit
include		<pthread.h>
declaration	void pthread_exit(void *status)
version		SUNW_1.1
end

function	pthread_getschedparam
include		<pthread.h>, <sched.h>
declaration	int pthread_getschedparam(pthread_t tid, int *policy, \
			struct sched_param *param)
version		SUNW_1.1
errno		ESRCH ENOTSUP EINVAL
end

function	pthread_getspecific
include		<pthread.h>
declaration	void *pthread_getspecific(pthread_key_t key)
version		SUNW_1.1
end

function	pthread_join
include		<pthread.h>
declaration	int pthread_join(pthread_t tid, void **status)
version		SUNW_1.1
errno		ESRCH EDEADLK
end

function	pthread_key_create
include		<pthread.h>
declaration	int pthread_key_create(pthread_key_t *keyp, \
			void (*destructor)(void *))
version		SUNW_1.1
errno		EAGAIN ENOMEM EINVAL
end

function	pthread_key_delete
include		<pthread.h>
declaration	int pthread_key_delete(pthread_key_t key)
version		SUNW_1.1
errno		EAGAIN ENOMEM EINVAL
end

function	pthread_kill
include		<pthread.h>, <signal.h>
declaration	int pthread_kill(pthread_t tid, int signo)
version		SUNW_1.1
errno		ESRCH EINVAL
end

function	pthread_mutex_destroy
include		<pthread.h>
declaration	int pthread_mutex_destroy(pthread_mutex_t *mutex)
version		SUNW_0.9
errno		EBUSY, EINVAL
end

function	pthread_mutex_getprioceiling
include		<pthread.h>
declaration	int pthread_mutex_getprioceiling(const pthread_mutex_t *mutex, \
			int *prioceiling)
version		SUNW_0.9
end

function	pthread_mutex_init
include		<pthread.h>
declaration	int pthread_mutex_init(pthread_mutex_t *mutex, \
			const pthread_mutexattr_t *attr)
version		SUNW_0.9
errno		EAGAIN, ENOMEM, EBUSY, EPERM, EINVAL
end

function	pthread_mutex_lock
include		<pthread.h>
declaration	int pthread_mutex_lock(pthread_mutex_t *mutex)
version		SUNW_0.9
errno		EINVAL, EDEADLK
end

function	pthread_mutex_setprioceiling
include		<pthread.h>
declaration	int pthread_mutex_setprioceiling(pthread_mutex_t *mutex, \
			int prioceiling, int *old_ceiling)
version		SUNW_0.9
end

function	pthread_mutex_trylock
include		<pthread.h>
declaration	int pthread_mutex_trylock(pthread_mutex_t *mutex)
version		SUNW_0.9
errno		EINVAL, EBUSY
end

function	pthread_mutex_unlock
include		<pthread.h>
declaration	int pthread_mutex_unlock(pthread_mutex_t *mutex)
version		SUNW_0.9
errno		EINVAL, EPERM
end

function	pthread_mutexattr_destroy
include		<pthread.h>
declaration	int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
version		SUNW_0.9
errno		ENOMEM EINVAL ENOSYS
end

function	pthread_mutexattr_getprioceiling
include		<pthread.h>
declaration	int pthread_mutexattr_getprioceiling( \
			const pthread_mutexattr_t *attr, int *prioceiling)
version		SUNW_0.9
end

function	pthread_mutexattr_getprotocol
include		<pthread.h>, <sched.h>
declaration	int pthread_mutexattr_getprotocol( \
			const pthread_mutexattr_t *attr, int *protocol)
version		SUNW_0.9
end

function	pthread_mutexattr_getpshared
include		<pthread.h>
declaration	int pthread_mutexattr_getpshared( \
			const pthread_mutexattr_t *attr, int *process_shared)
version		SUNW_0.9
errno		ENOMEM EINVAL ENOSYS
end

function	pthread_mutexattr_init
include		<pthread.h>
declaration	int pthread_mutexattr_init(pthread_mutexattr_t *attr)
version		SUNW_0.9
errno		ENOMEM EINVAL ENOSYS
end

function	pthread_mutexattr_setprotocol
include		<pthread.h>, <sched.h>
declaration	int pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, \
			int protocol)
version		SUNW_0.9
end

function	pthread_mutexattr_setpshared
include		<pthread.h>
declaration	int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, \
			int process_shared)
version		SUNW_0.9
errno		ENOMEM EINVAL ENOSYS
end

function	pthread_once
include		<pthread.h>
declaration	int pthread_once(pthread_once_t *once_control, \
			void (*init_routine)(void))
version		SUNW_1.1
errno		EINVAL
end

function	pthread_self
include		<pthread.h>
declaration	pthread_t pthread_self(void)
version		SUNW_1.1
end

function	pthread_setcancelstate
include		<pthread.h>
declaration	int pthread_setcancelstate(int state, int *oldstate)
version		SUNW_1.1
errno		EINVAL
end

function	pthread_setcanceltype
include		<pthread.h>
declaration	int pthread_setcanceltype(int type, int *oldtype)
version		SUNW_1.1
errno		EINVAL
end

function	pthread_setschedparam
include		<pthread.h>, <sched.h>
declaration	int pthread_setschedparam(pthread_t tid, int policy, \
			const struct sched_param *param)
version		SUNW_1.1
errno		ESRCH ENOTSUP EINVAL
end

function	pthread_setspecific
include		<pthread.h>
declaration	int pthread_setspecific(pthread_key_t key, const void *value)
version		SUNW_1.1
errno		ENOMEM EINVAL
end

function	pthread_sigmask
include		<pthread.h>, <signal.h>
declaration	int pthread_sigmask(int how, const sigset_t *newmask, \
			sigset_t *oldmask)
version		SUNW_1.1
errno		EINVAL EFAULT
end

function	pthread_testcancel
include		<pthread.h>
declaration	void pthread_testcancel(void)
version		SUNW_1.1
end

function	thr_continue
include		<thread.h>
declaration	int thr_continue(thread_t tid)
version		SUNW_0.8
end

function	thr_create
include		<thread.h>
declaration	int thr_create(void *stack_base, size_t stack_size, \
			void *(*start_func)(void *), void *arg, long flags, \
			thread_t *new_thread_ID)
version		SUNW_0.8
end

function	thr_exit
include		<thread.h>
declaration	void thr_exit(void *status)
version		SUNW_0.8
end

function	thr_getconcurrency
include		<thread.h>
declaration	int thr_getconcurrency(void)
version		SUNW_0.8
end

function	thr_getprio
include		<thread.h>
declaration	int thr_getprio(thread_t tid, int *priop)
version		SUNW_0.8
end

function	thr_getspecific
include		<thread.h>
declaration	int thr_getspecific(thread_key_t key, void **valuep)
version		SUNW_0.8
end

function	thr_join
include		<thread.h>
declaration	int thr_join(thread_t tid, thread_t *dtidp, void **statusp)
version		SUNW_0.8
end

function	thr_keycreate
include		<thread.h>
declaration	int thr_keycreate(thread_key_t *keyp, \
			void (*destructor)(void *value))
version		SUNW_0.8
end

function	thr_kill
include		<thread.h>, <signal.h>
declaration	int thr_kill(thread_t tid, int signo)
version		SUNW_0.8
end

function	thr_main
include		<thread.h>
declaration	int thr_main(void)
version		SUNW_1.1
errno
end

function	thr_min_stack
include		<thread.h>
declaration	size_t thr_min_stack(void)
version		SUNW_0.9
end

function	thr_self
include		<thread.h>
declaration	thread_t thr_self(void)
version		SUNW_0.8
end

function	thr_setconcurrency
include		<thread.h>
declaration	int thr_setconcurrency(int level)
version		SUNW_0.8
end

function	thr_setprio
include		<thread.h>
declaration	int thr_setprio(thread_t tid, int prio)
version		SUNW_0.8
end

function	thr_setspecific
include		<thread.h>
declaration	int thr_setspecific(thread_key_t key, void *value)
version		SUNW_0.8
end

function	thr_sigsetmask
include		<thread.h>, <signal.h>
declaration	int thr_sigsetmask(int how, const sigset_t *newp, \
			sigset_t *oldp)
version		SUNW_0.8
end

function	thr_stksegment
include		<thread.h>, <sys/signal.h>
declaration	int thr_stksegment(stack_t *sp)
version		SUNW_0.9
errno		EFAULT EAGAIN
end

function	thr_suspend
include		<thread.h>
declaration	int thr_suspend(thread_t tid)
version		SUNW_0.8
end

function	thr_yield
include		<thread.h>
declaration	void thr_yield(void)
version		SUNW_0.8
end

#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)pthread.spec	1.3	99/10/25 SMI"
#
# lib/libthread/spec/pthread.spec

function	pthread_atfork
declaration	int pthread_atfork(void (*prepare)(void), \
			void (*parent)(void), void (*child)(void))
version		SUNW_0.9
errno		ENOMEM
exception	$return != 0
end

function	pthread_attr_destroy
include		<pthread.h>
declaration	int pthread_attr_destroy(pthread_attr_t *attr)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_getdetachstate
include		<pthread.h>
declaration	int pthread_attr_getdetachstate(const pthread_attr_t *attr, \
			int *detachstate)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_getinheritsched 
include		<pthread.h>
declaration	int pthread_attr_getinheritsched(const pthread_attr_t *attr, \
			int *inheritsched)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_getschedparam 
include		<pthread.h>
declaration	int pthread_attr_getschedparam(const pthread_attr_t *attr, \
			struct sched_param *param)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_getschedpolicy 
include		<pthread.h>
declaration	int pthread_attr_getschedpolicy(const pthread_attr_t *attr, \
			int *policy)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_getscope 
include		<pthread.h>
declaration	int pthread_attr_getscope(const pthread_attr_t *attr, \
			int *contentionscope)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_getstackaddr 
include		<pthread.h>
declaration	int pthread_attr_getstackaddr(const pthread_attr_t *attr, \
			void **stackaddr)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_getstacksize 
include		<pthread.h>
declaration	int pthread_attr_getstacksize(const pthread_attr_t *attr, \
			size_t *stacksize)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_init 
include		<pthread.h>
declaration	int pthread_attr_init(pthread_attr_t *attr)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_setdetachstate 
include		<pthread.h>
declaration	int pthread_attr_setdetachstate(pthread_attr_t *attr, \
			int detachstate)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_setinheritsched 
include		<pthread.h>
declaration	int pthread_attr_setinheritsched(pthread_attr_t *attr, \
			int inheritsched)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_setschedparam 
include		<pthread.h>
declaration	int pthread_attr_setschedparam(pthread_attr_t *attr, \
			const struct sched_param *param)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_setschedpolicy 
include		<pthread.h>, <sched.h>
declaration	int pthread_attr_setschedpolicy(pthread_attr_t *attr, \
			int policy)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_setscope 
include		<pthread.h>
declaration	int pthread_attr_setscope(pthread_attr_t *attr, \
			int contentionscope)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_setstackaddr 
include		<pthread.h>
declaration	int pthread_attr_setstackaddr(pthread_attr_t *attr, \
			void *stackaddr)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_attr_setstacksize 
include		<pthread.h>
declaration	int pthread_attr_setstacksize(pthread_attr_t *attr, \
			size_t stacksize)
version		SUNW_0.9
errno		ENOMEM ENOTSUP EINVAL
exception	$return != 0
end

function	pthread_cancel 
include		<pthread.h>
declaration	int pthread_cancel(pthread_t target_thread)
version		SUNW_0.9
errno		ESRCH 
exception	$return != 0
end

function	__pthread_cleanup_pop 
version		SUNW_0.9
end

function	__pthread_cleanup_push 
version		SUNW_0.9
end

function	__pthread_init
version		SUNWprivate_1.1
end

function	pthread_cond_broadcast 
declaration	int pthread_cond_broadcast(pthread_cond_t *cond)
version		SUNW_0.9
errno		EFAULT EINVAL EINTR ETIME ETIMEDOUT
exception	$return != 0
end

function	pthread_cond_destroy 
declaration	int pthread_cond_destroy(pthread_cond_t *cond)
version		SUNW_0.9
errno		EFAULT EINVAL EINTR ETIME ETIMEDOUT
exception	$return != 0
end

function	pthread_cond_init 
declaration	int pthread_cond_init(pthread_cond_t *cond, \
			const pthread_condattr_t *attr)
version		SUNW_0.9
errno		EFAULT EINVAL EINTR ETIME ETIMEDOUT
exception	$return != 0
end

function	pthread_cond_signal 
declaration	int pthread_cond_signal(pthread_cond_t *cond)
version		SUNW_0.9
errno		EFAULT EINVAL EINTR ETIME ETIMEDOUT
exception	$return != 0
end

function	pthread_cond_timedwait 
declaration	int pthread_cond_timedwait(pthread_cond_t *cond, \
			pthread_mutex_t *mutex, const struct timespec *abstime)
version		SUNW_0.9
errno		EFAULT EINVAL EINTR ETIME ETIMEDOUT
exception	$return != 0
end

function	pthread_cond_wait 
declaration	int pthread_cond_wait(pthread_cond_t *cond, \
			pthread_mutex_t *mutex)
version		SUNW_0.9
errno		EFAULT EINVAL EINTR ETIME ETIMEDOUT
exception	$return != 0
end

function	pthread_condattr_destroy 
include		<pthread.h>
declaration	int pthread_condattr_destroy(pthread_condattr_t *attr)
version		SUNW_0.9
errno		ENOMEM EINVAL
exception	$return != 0
end

function	pthread_condattr_getpshared 
include		<pthread.h>
declaration	int pthread_condattr_getpshared (const pthread_condattr_t *attr, \
			int *process_shared)
version		SUNW_0.9
errno		ENOMEM EINVAL
exception	$return != 0
end

function	pthread_condattr_init 
include		<pthread.h>
declaration	int pthread_condattr_init(pthread_condattr_t *attr)
version		SUNW_0.9
errno		ENOMEM EINVAL
exception	$return != 0
end

function	pthread_condattr_setpshared 
include		<pthread.h>
declaration	int pthread_condattr_setpshared(pthread_condattr_t *attr, \
			int process_shared)
version		SUNW_0.9
errno		ENOMEM EINVAL
exception	$return != 0
end

function	pthread_create
declaration	int pthread_create(pthread_t *thread, \
			const pthread_attr_t *attr, \
			void *(*start_func)(void *), void *)
version		SUNW_0.9
errno		EAGAIN EINVAL ENOMEM
exception	$return != 0
end

function	pthread_detach 
declaration	int pthread_detach(pthread_t threadID)
version		SUNW_0.9
errno		EINVAL ESRCH
exception	$return != 0
end

function	pthread_equal 
include		<pthread.h>
declaration	int pthread_equal(pthread_t t1, pthread_t t2)
version		SUNW_0.9
exception	$return == 0
end

function	pthread_exit 
declaration	void pthread_exit(void *status)
version		SUNW_0.9
end

function	pthread_getschedparam 
declaration	int pthread_getschedparam(pthread_t target_thread, \
			int *policy, struct sched_param *param)
version		SUNW_0.9
errno		ESRCH ENOTSUP EINVAL
exception	$return != 0 /* */
end

function	pthread_getspecific 
declaration	void *pthread_getspecific(pthread_key_t key)
version		SUNW_0.9
errno		EAGAIN ENOMEM EINVAL
exception	$return == 0
end

function	pthread_join 
declaration	int pthread_join(pthread_t target_thread, void **status)
version		SUNW_0.9
errno		ESRCH EDEADLK
exception	$return != 0
end

function	pthread_key_create
declaration	int pthread_key_create(pthread_key_t *key, \
			 void (*destructor)(void *))
version		SUNW_0.9
errno		EAGAIN ENOMEM EINVAL
exception	$return != 0
end

function	pthread_key_delete 
declaration	int pthread_key_delete(pthread_key_t key)
version		SUNW_0.9
errno		EAGAIN ENOMEM EINVAL
exception	$return != 0
end

function	pthread_kill 
declaration	int pthread_kill(pthread_t thread, int sig)
version		SUNW_0.9
errno		ESRCH EINVAL
exception	$return != 0
end

function	pthread_mutex_destroy 
declaration	int pthread_mutex_destroy(pthread_mutex_t *mp)
version		SUNW_0.9
errno		EFAULT EINVAL EBUSY
exception	$return != 0
end

function	pthread_mutex_getprioceiling
include		<pthread.h>
declaration	int pthread_mutex_getprioceiling(const pthread_mutex_t *mutex, \
			int *prioceiling)
version		SUNW_0.9
exception	$return != 0
end

function	pthread_mutex_init 
declaration	int pthread_mutex_init(pthread_mutex_t *mp, \
			const pthread_mutexattr_t *attr)
version		SUNW_0.9
errno		EFAULT EINVAL EBUSY
exception	$return != 0
end

function	pthread_mutex_lock 
declaration	int pthread_mutex_lock(pthread_mutex_t *mp)
version		SUNW_0.9
errno		EFAULT EINVAL EBUSY
exception	$return != 0
end

function	pthread_mutex_setprioceiling 
include		<pthread.h>
declaration	int pthread_mutex_setprioceiling(pthread_mutex_t *mutex, \
			int prioceiling, int *old_ceiling)
version		SUNW_0.9
exception	$return != 0
end

function	pthread_mutex_trylock 
declaration	int pthread_mutex_trylock(pthread_mutex_t *mp)
version		SUNW_0.9
errno		EFAULT EINVAL EBUSY
exception	$return != 0
end

function	pthread_mutex_unlock 
declaration	int pthread_mutex_unlock(pthread_mutex_t *mp)
version		SUNW_0.9
errno		EFAULT EINVAL EBUSY
exception	$return != 0
end

function	pthread_mutexattr_destroy 
include		<pthread.h>
declaration	int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
version		SUNW_0.9
errno		ENOMEM EINVAL ENOSYS
exception	$return != 0
end

function	pthread_mutexattr_getprioceiling 
version		SUNW_0.9
end

function	pthread_mutexattr_getprotocol 
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
exception	$return != 0
end

function	pthread_mutexattr_init 
include		<pthread.h>
declaration	int pthread_mutexattr_init(pthread_mutexattr_t *attr)
version		SUNW_0.9
errno		ENOMEM EINVAL ENOSYS
exception	$return != 0
end

function	pthread_mutexattr_setprioceiling 
version		SUNW_0.9
end

function	pthread_mutexattr_setprotocol 
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
exception	$return != 0
end

function	pthread_once
include		<pthread.h>
declaration	int pthread_once(pthread_once_t *once_control, \
			void (*init_routine)(void))
version		SUNW_0.9
errno		EINVAL 
exception	$return != 0
end

function	pthread_self 
declaration	pthread_t pthread_self(void)
version		SUNW_0.9
exception	$return != 0
end

function	pthread_setcancelstate 
include		<pthread.h>
declaration	int pthread_setcancelstate(int state, int * oldstate)
version		SUNW_0.9
errno		EINVAL 
exception	$return != 0
end

function	pthread_setcanceltype 
include		<pthread.h>
declaration	int pthread_setcanceltype(int type, int * oldtype)
version		SUNW_0.9
errno		EINVAL 
exception	$return != 0
end

function	pthread_setschedparam 
declaration	int pthread_setschedparam(pthread_t target_thread, int policy, \
			const struct sched_param *param)
version		SUNW_0.9
errno		ESRCH ENOTSUP EINVAL
exception	$return != 0 /* */
end

function	pthread_setspecific 
declaration	int pthread_setspecific(pthread_key_t key, const void *value)
version		SUNW_0.9
errno		EAGAIN ENOMEM EINVAL
exception	$return != 0
end

function	pthread_sigmask
include		<pthread.h>, <signal.h>
declaration	int pthread_sigmask(int how, const sigset_t *set, \
			sigset_t *oset)
version		SUNW_0.9
errno		EINVAL EFAULT
exception	$return != 0
end

function	pthread_testcancel 
include		<pthread.h>
declaration	void pthread_testcancel()
version		SUNW_0.9
end

function	pthread_mutexattr_setrobust_np
include		<pthread.h>
declaration	int pthread_mutexattr_setrobust_np(pthread_mutexattr_t *attr, \
			int robustness)
version		SUNW_1.5
errno		ENOTSUP EINVAL ENOSYS
exception	$return != 0
end

function	pthread_mutexattr_getrobust_np 
include		<pthread.h>
declaration	int pthread_mutexattr_getrobust_np( \
			const pthread_mutexattr_t *attr, int *robustness)
version		SUNW_1.5
errno		ENOTSUP EINVAL ENOSYS
exception	$return != 0
end

function	pthread_mutex_consistent_np
include		<pthread.h>
declaration	int pthread_mutex_consistent_np(pthread_mutex_t *mp)
version		SUNW_1.5
errno		EINVAL ENOSYS
exception	$return != 0
end


function	_pthread_rwlock_trywrlock
version		SUNWprivate_1.1
end

function	_resume
version		SUNWprivate_1.1
end

function	_resume_ret
version		SUNWprivate_1.1
end

function	pthread_attr_getguardsize
version		SUNW_1.4
end

function	pthread_attr_setguardsize
version		SUNW_1.4
end

function	pthread_getconcurrency
version		SUNW_1.4
end

function	pthread_rwlock_destroy
version		SUNW_1.4
end

function	pthread_rwlock_init
version		SUNW_1.4
end

function	pthread_rwlock_rdlock
version		SUNW_1.4
end

function	pthread_rwlock_tryrdlock
version		SUNW_1.4
end

function	pthread_rwlock_trywrlock
version		SUNW_1.4
end

function	pthread_rwlock_unlock
version		SUNW_1.4
end

function	pthread_rwlock_wrlock
version		SUNW_1.4
end

function	pthread_rwlockattr_destroy
version		SUNW_1.4
end

function	pthread_rwlockattr_getpshared
version		SUNW_1.4
end

function	pthread_rwlockattr_init
version		SUNW_1.4
end

function	pthread_rwlockattr_setpshared
version		SUNW_1.4
end

function	pthread_setconcurrency
version		SUNW_1.4
end

function	__xpg4_putmsg extends libc/spec/sys.spec __xpg4_putmsg
version		SUNW_1.4
end

function	__xpg4_putpmsg extends libc/spec/sys.spec __xpg4_putpmsg
version		SUNW_1.4
end

function	_canceloff
version		SUNWprivate_1.1
end

function	_cancelon
version		SUNWprivate_1.1
end

function	getmsg extends libc/spec/sys.spec getmsg
version		SUNW_1.4
end

function	getpmsg extends libc/spec/sys.spec getpmsg
version		SUNW_1.4
end

function	lockf64 extends libc/spec/interface64.spec lockf64
arch		sparc i386
version		SUNW_1.4
end

function	lockf extends libc/spec/sys.spec lockf
version		SUNW_1.4
end

function	msgrcv extends libc/spec/sys.spec msgrcv
version		SUNW_1.4
end

function	msgsnd extends libc/spec/sys.spec msgsnd
version		SUNW_1.4
end

function	poll extends libc/spec/sys.spec poll
version		SUNW_1.4
end

function	putmsg extends libc/spec/gen.spec putmsg
version		SUNW_1.4
end

function	putpmsg extends libc/spec/gen.spec putpmsg
version		SUNW_1.4
end

function	select extends libc/spec/gen.spec select
version		SUNW_1.4
end

function	sigpause extends libc/spec/sys.spec sigpause
version		SUNW_1.4
end

function	usleep extends libc/spec/gen.spec usleep
version		SUNW_1.4
end

function	wait3	extends libc/spec/gen.spec wait3
version		SUNW_1.4
end

function	waitid extends libc/spec/sys.spec waitid
version		SUNW_1.4
end

function	__pthread_min_stack
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_gettype
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_settype
version		SUNWprivate_1.1
end

function	_pthread_setcleanupinit
version		SUNWprivate_1.1
end

function	pthread_mutexattr_gettype
version		SUNW_1.4
end

function	pthread_mutexattr_settype
version		SUNW_1.4
end

#
# Weak interfaces
#

function	_pthread_rwlockattr_setpshared
weak		pthread_rwlockattr_setpshared
version		SUNWprivate_1.1
end

function	_pthread_create
weak		pthread_create
version		SUNWprivate_1.1
end

function	_pthread_join
weak		pthread_join
version		SUNWprivate_1.1
end

function	_pthread_detach
weak		pthread_detach
version		SUNWprivate_1.1
end

function	_pthread_once
weak		pthread_once
version		SUNWprivate_1.1
end

function	_pthread_equal
weak		pthread_equal
version		SUNWprivate_1.1
end

function	_pthread_atfork
weak		pthread_atfork
version		SUNWprivate_1.1
end

function	_pthread_setschedparam
weak		pthread_setschedparam
version		SUNWprivate_1.1
end

function	_pthread_getschedparam
weak		pthread_getschedparam
version		SUNWprivate_1.1
end

function	_pthread_getspecific
weak		pthread_getspecific
version		SUNWprivate_1.1
end

function	_pthread_setspecific
weak		pthread_setspecific
version		SUNWprivate_1.1
end

function	_pthread_key_create
weak		pthread_key_create
version		SUNWprivate_1.1
end

function	_pthread_key_delete
weak		pthread_key_delete
version		SUNWprivate_1.1
end

function	_pthread_exit
weak		pthread_exit
version		SUNWprivate_1.1
end

function	_pthread_kill
weak		pthread_kill
version		SUNWprivate_1.1
end

function	_pthread_self
weak		pthread_self
version		SUNWprivate_1.1
end

function	_pthread_sigmask
weak		pthread_sigmask
version		SUNWprivate_1.1
end

function	_pthread_cancel
weak		pthread_cancel
version		SUNWprivate_1.1
end

function	_pthread_testcancel
weak		pthread_testcancel
version		SUNWprivate_1.1
end

function	_pthread_setcanceltype
weak		pthread_setcanceltype
version		SUNWprivate_1.1
end

function	_pthread_setcancelstate
weak		pthread_setcancelstate
version		SUNWprivate_1.1
end

function	_pthread_attr_init
weak		pthread_attr_init
version		SUNWprivate_1.1
end

function	_pthread_attr_destroy
weak		pthread_attr_destroy
version		SUNWprivate_1.1
end

function	_pthread_attr_setstacksize
weak		pthread_attr_setstacksize
version		SUNWprivate_1.1
end

function	_pthread_attr_getstacksize
weak		pthread_attr_getstacksize
version		SUNWprivate_1.1
end

function	_pthread_attr_setstackaddr
weak		pthread_attr_setstackaddr
version		SUNWprivate_1.1
end

function	_pthread_attr_getstackaddr
weak		pthread_attr_getstackaddr
version		SUNWprivate_1.1
end

function	_pthread_attr_setdetachstate
weak		pthread_attr_setdetachstate
version		SUNWprivate_1.1
end

function	_pthread_attr_getdetachstate
weak		pthread_attr_getdetachstate
version		SUNWprivate_1.1
end

function	_pthread_attr_setscope
weak		pthread_attr_setscope
version		SUNWprivate_1.1
end

function	_pthread_attr_getscope
weak		pthread_attr_getscope
version		SUNWprivate_1.1
end

function	_pthread_attr_setinheritsched
weak		pthread_attr_setinheritsched
version		SUNWprivate_1.1
end

function	_pthread_attr_getinheritsched
weak		pthread_attr_getinheritsched
version		SUNWprivate_1.1
end

function	_pthread_attr_setschedpolicy
weak		pthread_attr_setschedpolicy
version		SUNWprivate_1.1
end

function	_pthread_attr_getschedpolicy
weak		pthread_attr_getschedpolicy
version		SUNWprivate_1.1
end

function	_pthread_attr_setschedparam
weak		pthread_attr_setschedparam
version		SUNWprivate_1.1
end

function	_pthread_attr_getschedparam
weak		pthread_attr_getschedparam
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_init
weak		pthread_mutexattr_init
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_destroy
weak		pthread_mutexattr_destroy
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_setpshared
weak		pthread_mutexattr_setpshared
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_getpshared
weak		pthread_mutexattr_getpshared
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_setprotocol
weak		pthread_mutexattr_setprotocol
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_getprotocol
weak		pthread_mutexattr_getprotocol
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_setprioceiling
weak		pthread_mutexattr_setprioceiling
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_getprioceiling
weak		pthread_mutexattr_getprioceiling
version		SUNWprivate_1.1
end

function	_pthread_mutex_setprioceiling
weak		pthread_mutex_setprioceiling
version		SUNWprivate_1.1
end

function	_pthread_mutex_getprioceiling
weak		pthread_mutex_getprioceiling
version		SUNWprivate_1.1
end

function	_pthread_mutex_init
weak		pthread_mutex_init
version		SUNWprivate_1.1
end

function	_pthread_condattr_init
weak		pthread_condattr_init
version		SUNWprivate_1.1
end

function	_pthread_condattr_destroy
weak		pthread_condattr_destroy
version		SUNWprivate_1.1
end

function	_pthread_condattr_setpshared
weak		pthread_condattr_setpshared
version		SUNWprivate_1.1
end

function	_pthread_condattr_getpshared
weak		pthread_condattr_getpshared
version		SUNWprivate_1.1
end

function	_pthread_cond_init
weak		pthread_cond_init
version		SUNWprivate_1.1
end

function	_pthread_mutex_destroy
weak		pthread_mutex_destroy
version		SUNWprivate_1.1
end

function	_pthread_mutex_lock
weak		pthread_mutex_lock
version		SUNWprivate_1.1
end

function	_pthread_mutex_unlock
weak		pthread_mutex_unlock
version		SUNWprivate_1.1
end

function	_pthread_mutex_trylock
weak		pthread_mutex_trylock
version		SUNWprivate_1.1
end

function	_pthread_cond_destroy
weak		pthread_cond_destroy
version		SUNWprivate_1.1
end

function	_pthread_cond_wait
weak		pthread_cond_wait
version		SUNWprivate_1.1
end

function	_pthread_cond_timedwait
weak		pthread_cond_timedwait
version		SUNWprivate_1.1
end

function	_pthread_cond_signal
weak		pthread_cond_signal
version		SUNWprivate_1.1
end

function	_pthread_cond_broadcast
weak		pthread_cond_broadcast
version		SUNWprivate_1.1
end

function	_pthread_attr_getguardsize
weak		pthread_attr_getguardsize
version		SUNWprivate_1.1
end

function	_pthread_attr_setguardsize
weak		pthread_attr_setguardsize
version		SUNWprivate_1.1
end

function	_pthread_getconcurrency
weak		pthread_getconcurrency
version		SUNWprivate_1.1
end

function	_pthread_setconcurrency
weak		pthread_setconcurrency
version		SUNWprivate_1.1
end

function	_pthread_rwlock_init
weak		pthread_rwlock_init
version		SUNWprivate_1.1
end

function	_pthread_rwlock_destroy
weak		pthread_rwlock_destroy
version		SUNWprivate_1.1
end

function	_pthread_rwlock_rdlock
weak		pthread_rwlock_rdlock
version		SUNWprivate_1.1
end

function	_pthread_rwlock_tryrdlock
weak		pthread_rwlock_tryrdlock
version		SUNWprivate_1.1
end

function	_pthread_rwlock_wrlock
weak		pthread_rwlock_trywrlock
version		SUNWprivate_1.1
end

function	_pthread_rwlock_unlock
weak		pthread_rwlock_unlock
version		SUNWprivate_1.1
end

function	_pthread_rwlockattr_init
weak		pthread_rwlockattr_init
version		SUNWprivate_1.1
end

function	_pthread_rwlockattr_destroy
weak		pthread_rwlockattr_destroy
version		SUNWprivate_1.1
end

function	_pthread_rwlockattr_getpshared
weak		pthread_rwlockattr_getpshared
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_setrobust_np
weak		pthread_mutexattr_setrobust_np
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_getrobust_np
weak		pthread_mutexattr_getrobust_np
version		SUNWprivate_1.1
end

function	_pthread_mutex_consistent_np
weak		pthread_mutex_consistent_np
version		SUNWprivate_1.1
end

/*	Copyright (c) 1993,1997 by Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)pthread.c	1.17	99/10/25 SMI"

#include <pthread.h>
#include <signal.h>

/*
 * Stub library for programmer's interface to the dynamic linker.  Used
 * to satisfy ld processing, and serves as a precedence place-holder at
 * execution-time.  These routines are never actually called.
 */

#ifdef __STDC__

#pragma weak pthread_create = _pthread_create
#pragma weak pthread_join = _pthread_join
#pragma weak pthread_detach = _pthread_detach
#pragma weak pthread_once = _pthread_once
#pragma weak pthread_equal = _pthread_equal
#pragma weak pthread_atfork = _pthread_atfork
#pragma weak pthread_setschedparam = _pthread_setschedparam
#pragma weak pthread_getschedparam = _pthread_getschedparam
#pragma weak pthread_getspecific = _pthread_getspecific
#pragma weak pthread_setspecific = _pthread_setspecific
#pragma weak pthread_key_create = _pthread_key_create
#pragma weak pthread_key_delete = _pthread_key_delete
#pragma weak pthread_exit = _pthread_exit
#pragma weak pthread_kill = _pthread_kill
#pragma weak pthread_self = _pthread_self
#pragma weak pthread_sigmask = _pthread_sigmask
#pragma weak pthread_cancel = _pthread_cancel
#pragma weak pthread_testcancel = _pthread_testcancel
#pragma weak pthread_setcanceltype = _pthread_setcanceltype
#pragma weak pthread_setcancelstate = _pthread_setcancelstate
#pragma weak pthread_attr_init = _pthread_attr_init
#pragma weak pthread_attr_destroy = _pthread_attr_destroy
#pragma weak pthread_attr_setstacksize = _pthread_attr_setstacksize
#pragma weak pthread_attr_getstacksize = _pthread_attr_getstacksize
#pragma weak pthread_attr_setstackaddr = _pthread_attr_setstackaddr
#pragma weak pthread_attr_getstackaddr = _pthread_attr_getstackaddr
#pragma weak pthread_attr_setdetachstate = _pthread_attr_setdetachstate
#pragma weak pthread_attr_getdetachstate = _pthread_attr_getdetachstate
#pragma weak pthread_attr_setscope = _pthread_attr_setscope
#pragma weak pthread_attr_getscope = _pthread_attr_getscope
#pragma weak pthread_attr_setinheritsched = _pthread_attr_setinheritsched
#pragma weak pthread_attr_getinheritsched = _pthread_attr_getinheritsched
#pragma weak pthread_attr_setschedpolicy = _pthread_attr_setschedpolicy
#pragma weak pthread_attr_getschedpolicy = _pthread_attr_getschedpolicy
#pragma weak pthread_attr_setschedparam = _pthread_attr_setschedparam
#pragma weak pthread_attr_getschedparam = _pthread_attr_getschedparam
#pragma weak pthread_mutexattr_init = _pthread_mutexattr_init
#pragma weak pthread_mutexattr_destroy =  _pthread_mutexattr_destroy
#pragma weak pthread_mutexattr_setpshared =  _pthread_mutexattr_setpshared
#pragma weak pthread_mutexattr_getpshared =  _pthread_mutexattr_getpshared
#pragma weak pthread_mutexattr_setprotocol =  _pthread_mutexattr_setprotocol
#pragma weak pthread_mutexattr_getprotocol =  _pthread_mutexattr_getprotocol
#pragma weak pthread_mutexattr_setprioceiling = \
					_pthread_mutexattr_setprioceiling
#pragma weak pthread_mutexattr_getprioceiling = \
					_pthread_mutexattr_getprioceiling
#pragma weak pthread_mutexattr_setrobust_np =  _pthread_mutexattr_setrobust_np
#pragma weak pthread_mutexattr_getrobust_np =  _pthread_mutexattr_getrobust_np
#pragma weak pthread_mutex_setprioceiling =  _pthread_mutex_setprioceiling
#pragma weak pthread_mutex_getprioceiling =  _pthread_mutex_getprioceiling
#pragma weak pthread_mutex_init = _pthread_mutex_init
#pragma weak pthread_mutex_consistent_np =  _pthread_mutex_consistent_np
#pragma weak pthread_condattr_init = _pthread_condattr_init
#pragma weak pthread_condattr_destroy = _pthread_condattr_destroy
#pragma weak pthread_condattr_setpshared = _pthread_condattr_setpshared
#pragma weak pthread_condattr_getpshared = _pthread_condattr_getpshared
#pragma weak pthread_cond_init = _pthread_cond_init
#pragma weak pthread_mutex_destroy = _pthread_mutex_destroy
#pragma weak pthread_mutex_lock = _pthread_mutex_lock
#pragma weak pthread_mutex_unlock = _pthread_mutex_unlock
#pragma weak pthread_mutex_trylock = _pthread_mutex_trylock
#pragma weak pthread_cond_destroy = _pthread_cond_destroy
#pragma weak pthread_cond_wait = _pthread_cond_wait
#pragma weak pthread_cond_timedwait = _pthread_cond_timedwait
#pragma weak pthread_cond_signal = _pthread_cond_signal
#pragma weak pthread_cond_broadcast = _pthread_cond_broadcast
#pragma weak pthread_attr_getguardsize = _pthread_attr_getguardsize
#pragma weak pthread_attr_setguardsize = _pthread_attr_setguardsize
#pragma weak pthread_getconcurrency = _pthread_getconcurrency
#pragma weak pthread_setconcurrency = _pthread_setconcurrency
#pragma weak pthread_mutex_attr_settype = _pthread_mutex_attr_settype
#pragma weak pthread_mutex_attr_gettype = _pthread_mutex_attr_gettype
#pragma weak pthread_rwlock_init = _pthread_rwlock_init
#pragma weak pthread_rwlock_destroy = _pthread_rwlock_destroy
#pragma weak pthread_rwlock_rdlock = _pthread_rwlock_rdlock
#pragma weak pthread_rwlock_tryrdlock = _pthread_rwlock_tryrdlock
#pragma weak pthread_rwlock_wrlock = _pthread_rwlock_wrlock
#pragma weak pthread_rwlock_trywrlock = _pthread_rwlock_wrlock
#pragma weak pthread_rwlock_unlock = _pthread_rwlock_unlock
#pragma weak pthread_rwlockattr_init = _pthread_rwlockattr_init
#pragma weak pthread_rwlockattr_destroy = _pthread_rwlockattr_destroy
#pragma weak pthread_rwlockattr_getpshared = _pthread_rwlockattr_getpshared
#pragma weak pthread_rwlockattr_setpshared = _pthread_rwlockattr_setpshared

#endif /* __STDC__ */

/*
 * function prototypes - thread related calls
 */
/* ARGSUSED */
int
_pthread_attr_init(pthread_attr_t *attr)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_destroy(pthread_attr_t *attr)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_setscope(pthread_attr_t *attr, int contentionscope)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_getscope(const pthread_attr_t *attr, int *scope)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_setinheritsched(pthread_attr_t *attr, int inherit)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inheritsched)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_setschedparam(pthread_attr_t *attr,
					const struct sched_param *param)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_attr_getschedparam(const pthread_attr_t *attr,
					struct sched_param *param)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
					void (*start_routine)(void *),
					void *arg)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
	return (0);
}

/* ARGSUSED */
int
_pthread_join(pthread_t thread, void **status)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_detach(pthread_t thread)
{
	return (0);
}

/* ARGSUSED */
void
_pthread_exit(void *value_ptr)
{

}

/* ARGSUSED */
int
_pthread_kill(pthread_t thread, int sig)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_cancel(pthread_t thread)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_setschedparam(pthread_t thread, int policy,
					const struct sched_param *param)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_getschedparam(pthread_t thread, int *policy,
					struct sched_param *param)
{
	return (0);
}

/* pthread_cleanup_push(void (*routine)(void *), void *arg) */
/* ARGSUSED */
void
__pthread_cleanup_push(void (*routine)(void *), void *arg, caddr_t fp,
							_cleanup_t *cl)
{

}

/* pthread_cleanup_pop(int execute) */
/* ARGSUSED */
void
__pthread_cleanup_pop(int ex, _cleanup_t *cl)
{

}

/* ARGSUSED */
int
_pthread_setcancelstate(int state, int *oldstate)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_setcanceltype(int type, int *oldtype)
{
	return (0);
}

void
_pthread_testcancel(void)
{

}

/* ARGSUSED */
int
_pthread_equal(pthread_t t1, pthread_t t2)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_atfork(void (*prepare) (void),
			void (*parent) (void), void (*child) (void))
{
	return (0);
}

/* ARGSUSED */
int
_pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_key_create(pthread_key_t *key, void (*destructor(void *)))
{
	return (0);
}

/* ARGSUSED */
int
_pthread_key_delete(pthread_key_t key)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_setspecific(pthread_key_t key, const void *value)
{
	return (0);
}

/* ARGSUSED */
void *
_pthread_getspecific(pthread_key_t key)
{
	return ((void *)0);
}

pthread_t
_pthread_self(void)
{
	return ((pthread_t)0);
}


/*
 * function prototypes - synchronization related calls
 */
/* ARGSUSED */
int
_pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutexattr_setrobust_np(pthread_mutexattr_t *attr, int robustness)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutexattr_getrobust_np(const pthread_mutexattr_t *attr,
    int *robustness)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutex_consistent_np(pthread_mutex_t *mutex)
{
	return (0);
}


/* ARGSUSED */
int
_pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int protocol)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr, int *protocol)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr, int prioceiling)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *attr,
						int *ceiling)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutex_lock(pthread_mutex_t *mutex)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutex_setprioceiling(pthread_mutex_t *mutex, int prioceiling,
						int *old_ceiling)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_mutex_getprioceiling(const pthread_mutex_t *mutex, int *ceiling)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_condattr_init(pthread_condattr_t *attr)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_condattr_destroy(pthread_condattr_t *attr)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_condattr_getpshared(const pthread_condattr_t *attr, int *pshared)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_cond_destroy(pthread_cond_t *cond)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_cond_broadcast(pthread_cond_t *cond)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_cond_signal(pthread_cond_t *cond)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	return (0);
}

/* ARGSUSED */
int
_pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
					const struct timespec *abstime)
{
	return (0);
}

size_t
__pthread_min_stack()
{
	return (0);
}

int
_pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *guardsize)
{
	return (0);
}

int
_pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize)
{
	return (0);
}

int
_pthread_getconcurrency(void)
{
	return (0);
}

int
_pthread_setconcurrency(int newval)
{
	return (0);
}

int
_pthread_mutex_attr_settype(pthread_mutexattr_t *attr, int type)
{
	return (0);
}

int
_pthread_mutex_attr_gettype(pthread_mutexattr_t *attr, int *type)
{
	return (0);
}

int
_pthread_rwlock_init(pthread_rwlock_t *rwlock,
				const pthread_rwlockattr_t *attr)
{
	return (0);
}

int
_pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
	return (0);
}

int
_pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
	return (0);
}

int
_pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock)
{
	return (0);
}

int
_pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
	return (0);
}

int
_pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock)
{
	return (0);
}

int
_pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
	return (0);
}

int
_pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
{
	return (0);
}

int
_pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
{
	return (0);
}

int
_pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *attr,
					int *pshared)
{
	return (0);
}

int
_pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr, int pshared)
{
	return (0);
}

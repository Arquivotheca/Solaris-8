/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)fork1.c	1.43	99/06/03 SMI"

#ifdef __STDC__
#pragma weak fork1 = _fork1
#pragma weak fork = _fork

#pragma weak _ti_fork1 = _fork1
#pragma weak _ti_fork = _fork
#endif /* __STDC__ */

#include "libthread.h"

/*
 * The following two functions are used to make the rwlocks fork1 safe
 * see file common/rwlock.c for more information.
 */
extern void _rwlsub_lock(void);
extern void _rwlsub_unlock(void);

pid_t
_fork1(void)
{
	pid_t pid;

	_prefork_handler();
	_rw_wrlock(&tsd_common.lock);
	_rwlsub_lock();
	/*
	 * Grab all internal threads lib locks. When new internal locks
	 * are added, they should be added here. This is to guarantee
	 * two things - consistent libthread data structures in child,
	 * e.g. the TSD key table, and to prevent a deadlock in child due
	 * to an internal lock.
	 */
	_lmutex_lock(&_resv_tls_common.lock);
	_lmutex_lock(&_sc_lock);
	_lmutex_lock(&_calloutlock);
	while (_sc_dontfork)
		_cond_wait(&_sc_dontfork_cv, &_sc_lock);
	_lmutex_lock(&_stkcachelock);
	while (_defaultstkcache.busy)
		_cond_wait(&_defaultstkcache.cv, &_stkcachelock);
	_lmutex_lock(&_tsslock);
	_lmutex_lock(&_tidlock);
	_lmutex_lock(&_concurrencylock);
	_lwp_mutex_lock(&_sighandlerlock);
	/*
	 * Note that the reap lock is a low-level lock - anything acquired
	 * after this lock cannot be a mutex on which the thread can sleep
	 * since the _reaplock is acquired in _resume_ret() if the yielding
	 * thread that wakes up this one, is a zombie. The ldt_lock in libc
	 * and _schedlock are lwp-level locks so it is OK to grab them - not
	 * only that, but they *should* be acquired after the reaplock.
	 * The ldt_lock is acquired by __freegs() in _lwp_terminate() which
	 * holds _reaplock when calling __freegs(), thus establishing this
	 * order. The _schedlock is the lowest level lock. The linker locks
	 * have to be acquired last - otherwise, a jump into the linker after
	 * grabbing these locks could result in a deadlock.
	 */
	_lwp_mutex_lock(&_reaplock);
#ifdef i386
	/*
	 * Special routines in libc to acquire freegslock and ldt_lock.
	 * Lock ordering is arbitrary since no place other than here
	 * acquires both locks at the same time.  __setupgs() and
	 * __freegs() grab both locks, but not at the same time.
	 */
	__freegs_lock();
	__ldt_lock();
#endif
	_sched_lock();
	/*
	 * The order in which _schedlock and _pmasklock should be acquired has
	 * been established in the function _sigredirect(), where _pmasklock
	 * is acquired with the _schedlock held. Have to follow the same order
	 * here. So grab _pmasklock after _schedlock.
	 */
	_lwp_mutex_lock(&_pmasklock);
	_lprefork_handler();
	pid = __fork1();
	if (pid == 0) { /* child */
		/* indicate that SIGWAITING has been disabled by __fork1() */
		_sigwaitingset = 0;
		_lpostfork_child_handler();
		_resetlib();
		_lwp_mutex_unlock(&_pmasklock);
		_sched_unlock();
#ifdef i386
		__ldt_unlock();
		__freegs_unlock();
#endif
		_lwp_mutex_unlock(&_reaplock);
		/*
		 * Now release all internal locks grabbed in parent above.
		 */
		_lwp_mutex_unlock(&_sighandlerlock);
		_lmutex_unlock(&_concurrencylock);
		_lmutex_unlock(&_tidlock);
		_lmutex_unlock(&_tsslock);
		_lmutex_unlock(&_stkcachelock);
		_lmutex_unlock(&_calloutlock);
		_lmutex_unlock(&_sc_lock);
		_lmutex_unlock(&_resv_tls_common.lock);
		_rwlsub_unlock();
		_rw_unlock(&tsd_common.lock);
		_postfork_child_handler();
#ifdef i386
		_cleanup_ldt();
#endif
		/*
		 * Since _schedlock needs to be called by _reaper_create(),
		 * _reaper_create() should not be called with _schedlock held.
		 * Since _reaper_create() is called only from the child of a
		 * fork1() which does not yet have threads or lwps, this is OK.
		 */
		ASSERT(!(MUTEX_HELD(&_schedlock)));
		_sys_thread_create(_dynamiclwps, __LWP_ASLWP);
		_sc_init();
		_reaper_create();
	} else {
		/*
		 * Unlock all locks in parent too.
		 */
		_lpostfork_parent_handler();
		_lwp_mutex_unlock(&_pmasklock);
		_sched_unlock();
#ifdef i386
		__ldt_unlock();
		__freegs_unlock();
#endif
		_lwp_mutex_unlock(&_reaplock);
		_lwp_mutex_unlock(&_sighandlerlock);
		_lmutex_unlock(&_concurrencylock);
		_lmutex_unlock(&_tidlock);
		_lmutex_unlock(&_tsslock);
		_lmutex_unlock(&_stkcachelock);
		_lmutex_unlock(&_calloutlock);
		_lmutex_unlock(&_sc_lock);
		_lmutex_unlock(&_resv_tls_common.lock);
		_rwlsub_unlock();
		_rw_unlock(&tsd_common.lock);
		_postfork_parent_handler();
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

pid_t
_fork(void)
{
	pid_t pid;

	if (_libpthread_loaded != 0)
		/* if linked with -lpthread */
		return (_fork1());

	/* else linked with -lthread */
	_lmutex_lock(&_sc_lock);
	while (_sc_dontfork)
		_cond_wait(&_sc_dontfork_cv, &_sc_lock);
	_sched_lock();
	pid = __fork();
	if (pid == 0) {
		_sc_cleanup(0);		/* clean up schedctl ptrs in child */
		_sched_unlock();
		_lmutex_unlock(&_sc_lock);
		_sc_init();		/* initialize new data */
	} else {
		_sched_unlock();
		_lmutex_unlock(&_sc_lock);
	}
	return (pid);
}

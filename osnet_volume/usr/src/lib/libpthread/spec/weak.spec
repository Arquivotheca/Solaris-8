#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)weak.spec	1.2	99/10/25 SMI"
#
# lib/libpthread/spec/weak.spec

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

function	_pthread_mutex_attr_settype
weak		pthread_mutex_attr_settype
version		SUNWprivate_1.1
end		

function	_pthread_mutex_attr_gettype
weak		pthread_mutex_attr_gettype
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

function	_fork
weak		fork
version		SUNWprivate_1.1
end		

function	_fork1
weak		fork1
version		SUNWprivate_1.1
end		

function	_sigaction
weak		sigaction
version		SUNWprivate_1.1
end		

function	_sigprocmask
weak		sigprocmask
version		SUNWprivate_1.1
end		

function	_sigwait
weak		sigwait
version		SUNWprivate_1.1
end		

function	_sigsuspend
weak		sigsuspend
version		SUNWprivate_1.1
end		

function	_sigsetjmp
weak		sigsetjmp
version		SUNWprivate_1.1
end		

function	_siglongjmp
weak		siglongjmp
version		SUNWprivate_1.1
end		

function	_sleep
weak		sleep
version		SUNWprivate_1.1
end		

function	_alarm
weak		alarm
version		SUNWprivate_1.1
end		

function	_setitimer
weak		setitimer
version		SUNWprivate_1.1
end		

function	_cond_broadcast
weak		cond_broadcast
version		SUNWprivate_1.1
end		

function	_cond_init
weak		cond_init
version		SUNWprivate_1.1
end		

function	_cond_destroy
weak		cond_destroy
version		SUNWprivate_1.1
end		

function	_cond_signal
weak		cond_signal
version		SUNWprivate_1.1
end		

function	_cond_timedwait
weak		cond_timedwait
version		SUNWprivate_1.1
end		

function	_cond_wait
weak		cond_wait
version		SUNWprivate_1.1
end		

function	_mutex_destroy
weak		mutex_destroy
version		SUNWprivate_1.1
end		

function	_mutex_init
weak		mutex_init
version		SUNWprivate_1.1
end		

function	_mutex_lock
weak		mutex_lock
version		SUNW_0.9
end		

function	_mutex_trylock
weak		mutex_trylock
version		SUNWprivate_1.1
end		

function	_mutex_unlock
weak		mutex_unlock
version		SUNWprivate_1.1
end		

function	_rw_rdlock
weak		rw_rdlock
version		SUNWprivate_1.1
end		

function	_rw_tryrdlock
weak		rw_tryrdlock
version		SUNWprivate_1.1
end		

function	_rw_trywrlock
weak		rw_trywrlock
version		SUNWprivate_1.1
end		

function	_rw_unlock
weak		rw_unlock
version		SUNWprivate_1.1
end		

function	_rw_wrlock
weak		rw_wrlock
version		SUNWprivate_1.1
end		

function	_rwlock_init
weak		rwlock_init
version		SUNWprivate_1.1
end		

function	_sema_init
weak		sema_init
version		SUNWprivate_1.1
end		

function	_sema_destroy
weak		sema_destroy
version		SUNWprivate_1.1
end		

function	_sema_post
weak		sema_post
version		SUNWprivate_1.1
end		

function	_sema_trywait
weak		sema_trywait
version		SUNWprivate_1.1
end		

function	_sema_wait
weak		sema_wait
version		SUNWprivate_1.1
end		

function	_thr_continue
weak		thr_continue
version		SUNWprivate_1.1
end		

function	_thr_create
weak		thr_create
version		SUNWprivate_1.1
end		

function	_thr_exit
weak		thr_exit
version		SUNWprivate_1.1
end		

function	_thr_getconcurrency
weak		thr_getconcurrency
version		SUNWprivate_1.1
end		

function	_thr_getprio
weak		thr_getprio
version		SUNWprivate_1.1
end		

function	_thr_getspecific
weak		thr_getspecific
version		SUNWprivate_1.1
end		

function	_thr_join
weak		thr_join
version		SUNWprivate_1.1
end		

function	_thr_keycreate
weak		thr_keycreate
version		SUNWprivate_1.1
end		

function	_thr_kill
weak		thr_kill
version		SUNWprivate_1.1
end		

function	_thr_self
weak		thr_self
version		SUNWprivate_1.1
end		

function	_thr_setconcurrency
weak		thr_setconcurrency
version		SUNWprivate_1.1
end		

function	_thr_setprio
weak		thr_setprio
version		SUNWprivate_1.1
end		

function	_thr_setspecific
weak		thr_setspecific
version		SUNWprivate_1.1
end		

function	_thr_sigsetmask
weak		thr_sigsetmask
version		SUNWprivate_1.1
end		

function	_thr_suspend
weak		thr_suspend
version		SUNWprivate_1.1
end		

function	_thr_yield
weak		thr_yield
version		SUNWprivate_1.1
end		

function	_thr_main
weak		thr_main
version		SUNWprivate_1.1
end		

function	_thr_min_stack
weak		thr_min_stack
version		SUNWprivate_1.1
end		

function	_thr_stksegment
weak		thr_stksegment
version		SUNWprivate_1.1
end		


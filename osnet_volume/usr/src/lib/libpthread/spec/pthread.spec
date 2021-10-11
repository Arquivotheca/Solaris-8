#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)pthread.spec	1.3	99/12/06 SMI"
#
# lib/libpthread/spec/pthread.spec

function	pthread_atfork extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_destroy extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_getdetachstate extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_getinheritsched extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_getschedparam extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_getschedpolicy extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_getscope extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_getstackaddr extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_getstacksize extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_init extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_setdetachstate extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_setinheritsched extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_setschedparam extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_setschedpolicy extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_setscope extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_setstackaddr extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_attr_setstacksize extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_cancel extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	__pthread_cleanup_pop 
version		SUNW_0.9
end		

function	__pthread_cleanup_push 
version		SUNW_0.9
end		

# libthread initialization.
function	__pthread_init
version		SUNWprivate_1.1
end		

function	pthread_cond_broadcast extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_cond_destroy extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_cond_init extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_cond_signal extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_cond_timedwait extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_cond_wait extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_condattr_destroy extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_condattr_getpshared extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_condattr_init extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_condattr_setpshared extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_create  extends libthread/spec/pthread.spec pthread_create
version		SUNW_0.9
errno		EAGAIN EINVAL ENOMEM
exception	$return != 0
end		

function	pthread_detach extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_equal extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_exit extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_getschedparam extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_getspecific extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_join extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_key_create  extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_key_delete extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_kill extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutex_destroy extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutex_getprioceiling  extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutex_init extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutex_lock extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutex_setprioceiling extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutex_trylock extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutex_unlock extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutexattr_destroy extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutexattr_getprioceiling extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutexattr_getprotocol  extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutexattr_getpshared extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutexattr_init extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutexattr_setprioceiling extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutexattr_setprotocol extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_mutexattr_setpshared extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_once  extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_self extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_setcancelstate extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_setcanceltype extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_setschedparam extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_setspecific extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_sigmask extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	pthread_testcancel extends libthread/spec/pthread.spec
version		SUNW_0.9
end		

function	_pthread_rwlock_trywrlock extends libthread/spec/pthread.spec
version		SUNWprivate_1.1
end		

function	_resume extends libthread/spec/pthread.spec
version		SUNWprivate_1.1
end		

function	_resume_ret extends libthread/spec/pthread.spec
version		SUNWprivate_1.1
end		

function	pthread_attr_getguardsize extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	pthread_attr_setguardsize extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	pthread_getconcurrency extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	pthread_mutex_attr_gettype
version		SUNW_1.2
end		

function	pthread_mutex_attr_settype
version		SUNW_1.2
end		

function	pthread_rwlock_destroy extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	pthread_rwlock_init extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	pthread_rwlock_rdlock extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	pthread_rwlock_tryrdlock extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	pthread_rwlock_trywrlock extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	pthread_rwlock_unlock extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	pthread_rwlock_wrlock extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	pthread_rwlockattr_destroy extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	pthread_rwlockattr_getpshared extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	pthread_rwlockattr_init extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	pthread_rwlockattr_setpshared extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	pthread_setconcurrency extends libthread/spec/pthread.spec
version		SUNW_1.2
end		

function	__xpg4_putmsg extends libc/spec/sys.spec __xpg4_putmsg
version		SUNW_1.2
end		

function	__xpg4_putpmsg extends libc/spec/sys.spec __xpg4_putpmsg
version		SUNW_1.2
end		

# 2.7, UNIX98.
function	_canceloff
version		SUNWprivate_1.1
end		

# 2.7, UNIX98.
function	_cancelon
version		SUNWprivate_1.1
end		

function	getmsg extends libc/spec/sys.spec
version		SUNW_1.2
end		

function	getpmsg extends libc/spec/sys.spec
version		SUNW_1.2
end		

function	lockf64 extends libc/spec/interface64.spec
arch		sparc i386
version		SUNW_1.2
end		

function	lockf extends libc/spec/sys.spec
version		SUNW_1.2
end		

function	msgrcv extends libc/spec/sys.spec
version		SUNW_1.2
end		

function	msgsnd extends libc/spec/sys.spec
version		SUNW_1.2
end		

function	poll extends libc/spec/sys.spec
version		SUNW_1.2
end		

function	putmsg extends libc/spec/gen.spec
version		SUNW_1.2
end		

function	putpmsg extends libc/spec/gen.spec
version		SUNW_1.2
end		

function	select extends libc/spec/gen.spec
version		SUNW_1.2
end		

function	sigpause extends libc/spec/sys.spec
version		SUNW_1.2
end		

function	usleep extends libc/spec/gen.spec
version		SUNW_1.2
end		

function	wait3	extends libc/spec/gen.spec
version		SUNW_1.2
end		

function	waitid extends libc/spec/sys.spec
version		SUNW_1.2
end		

function	__thr_door_unbind
version		SUNWprivate_1.1
end		

# 2.8, robustness interfaces (for prio inheritance mutexes)

function	pthread_mutexattr_setrobust_np extends libthread/spec/pthread.spec
version		SUNW_1.3
end		

function	pthread_mutexattr_getrobust_np extends libthread/spec/pthread.spec
version		SUNW_1.3
end		

function	pthread_mutex_consistent_np extends libthread/spec/pthread.spec
version		SUNW_1.3
end		

function	_libthread_sema_wait
version		SUNWprivate_1.1
end		

#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)thread.spec	1.4	99/12/06 SMI"
#
# lib/libthread/spec/thread.spec

function	thr_continue extends libc/spec/stubs.spec thr_continue
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_create extends libc/spec/stubs.spec thr_create
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_exit extends libc/spec/stubs.spec thr_exit
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_getconcurrency extends libc/spec/stubs.spec thr_getconcurrency
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_getprio extends libc/spec/stubs.spec thr_getprio
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_getspecific extends libc/spec/stubs.spec thr_getspecific
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_join extends libc/spec/stubs.spec thr_join
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_keycreate extends libc/spec/stubs.spec thr_keycreate
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_kill extends libc/spec/stubs.spec thr_kill
include		<thread.h>, <signal.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_main extends libc/spec/stubs.spec thr_main
include		<thread.h>
version		i386=SUNW_0.9 sparc=SISCD_2.3b sparcv9=SUNW_0.9
end		

function	thr_min_stack extends libc/spec/stubs.spec thr_min_stack
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_self extends libc/spec/stubs.spec thr_self
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_setconcurrency extends libc/spec/stubs.spec thr_setconcurrency
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_setprio extends libc/spec/stubs.spec thr_setprio
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_setspecific extends libc/spec/stubs.spec thr_setspecific
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_sigsetmask extends libc/spec/stubs.spec thr_sigsetmask
include		<thread.h>, <signal.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_stksegment extends libc/spec/stubs.spec thr_stksegment
include		<thread.h>, <sys/signal.h>
version		i386=SUNW_0.9 sparc=SISCD_2.3b sparcv9=SUNW_0.9
end		

function	thr_suspend extends libc/spec/stubs.spec thr_suspend
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_yield extends libc/spec/stubs.spec thr_yield
include		<thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	thr_probe_getfunc_addr
version		SUNWprivate_1.1
end		

function	thr_probe_setup
version		SUNWprivate_1.1
end		

function	__thr_door_unbind
version		SUNWprivate_1.1
end		

function	_thr_detach
version		SUNWprivate_1.1
end		

function	_thr_errnop
version		SUNWprivate_1.1
end		

function	_thr_key_delete
version		SUNWprivate_1.1
end		

function	_thr_libthread
version		SUNWprivate_1.1
end		

function	lwp_self
version		SUNW_0.7
end		

function	_thr_continue_allmutators
version		SUNWprivate_1.1
end		

function	_thr_continue_mutator
version		SUNWprivate_1.1
end		

function	_thr_getstate
version		SUNWprivate_1.1
end		

function	_thr_mutators_barrier
version		SUNWprivate_1.1
end		

function	_thr_setmutator
version		SUNWprivate_1.1
end		

function	_thr_setstate
version		SUNWprivate_1.1
end		

function	_thr_sighndlrinfo
version		SUNWprivate_1.1
end		

function	thr_slot_sync_allocate
version		SUNWprivate_1.1
end		

function	thr_slot_sync_deallocate
version		SUNWprivate_1.1
end		

function	thr_slot_get
version		SUNWprivate_1.1
end		

function	thr_slot_set
version		SUNWprivate_1.1
end		

function	_thr_suspend_allmutators
version		SUNWprivate_1.1
end		

function	_thr_suspend_mutator
version		SUNWprivate_1.1
end		

function	_thr_wait_mutator
version		SUNWprivate_1.1
end		

function	thr_continue_allmutators
version		SUNWprivate_1.1
end		

function	thr_continue_mutator
version		SUNWprivate_1.1
end		

function	thr_getstate
version		SUNWprivate_1.1
end		

function	thr_mutators_barrier
version		SUNWprivate_1.1
end		

function	thr_setmutator
version		SUNWprivate_1.1
end		

function	thr_setstate
version		SUNWprivate_1.1
end		

function	thr_sighndlrinfo
version		SUNWprivate_1.1
end		

function	thr_suspend_allmutators
version		SUNWprivate_1.1
end		

function	thr_suspend_mutator
version		SUNWprivate_1.1
end		

function	thr_wait_mutator
version		SUNWprivate_1.1
end		

#
# Weak interfaces
#

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
version		SUNWprivate_1.1
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

function	_libthread_sema_wait
version		SUNWprivate_1.1
end		

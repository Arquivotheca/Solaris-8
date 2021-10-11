/*	Copyright (c) 1994-1998 by Sun Microsystems, Inc. */
/*	All rights reserved. */


/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)thr.c	1.8	98/05/22 SMI"

#include <thread.h>
#include <synch.h>


/*
 * Stub library for programmer's interface to the dynamic linker.  Used
 * to satisfy ld processing, and serves as a precedence place-holder at
 * execution-time.  These routines are never actually called.
 */



#pragma weak cond_broadcast = _cond_broadcast
#pragma weak cond_init = _cond_init
#pragma weak cond_destroy = _cond_destroy
#pragma weak cond_signal = _cond_signal
#pragma weak cond_timedwait = _cond_timedwait
#pragma weak cond_wait = _cond_wait
#pragma weak mutex_destroy = _mutex_destroy
#pragma weak mutex_init = _mutex_init
#pragma weak mutex_lock = _mutex_lock
#pragma weak mutex_trylock = _mutex_trylock
#pragma weak mutex_unlock = _mutex_unlock
#pragma weak rw_rdlock = _rw_rdlock
#pragma weak rw_tryrdlock = _rw_tryrdlock
#pragma weak rw_trywrlock = _rw_trywrlock
#pragma weak rw_unlock = _rw_unlock
#pragma weak rw_wrlock = _rw_wrlock
#pragma weak rwlock_init = _rwlock_init
#pragma weak sema_init = _sema_init
#pragma weak sema_destroy = _sema_destroy
#pragma weak sema_post = _sema_post
#pragma weak sema_trywait = _sema_trywait
#pragma weak sema_wait = _sema_wait
#pragma weak thr_continue = _thr_continue
#pragma weak thr_create = _thr_create
#pragma weak thr_exit = _thr_exit
#pragma weak thr_getconcurrency = _thr_getconcurrency
#pragma weak thr_getprio = _thr_getprio
#pragma weak thr_getspecific = _thr_getspecific
#pragma weak thr_join = _thr_join
#pragma weak thr_keycreate = _thr_keycreate
#pragma weak thr_kill = _thr_kill
#pragma weak thr_self = _thr_self
#pragma weak thr_setconcurrency = _thr_setconcurrency
#pragma weak thr_setprio = _thr_setprio
#pragma weak thr_setspecific = _thr_setspecific
#pragma weak thr_sigsetmask = _thr_sigsetmask
#pragma weak thr_suspend = _thr_suspend
#pragma weak thr_yield = _thr_yield
#pragma weak thr_main = _thr_main
#pragma weak thr_min_stack = _thr_min_stack
#pragma weak thr_stksegment = _thr_stksegment

int
_getsp()
{
	return (0);
}

int
_cond_broadcast()
{
	return (0);
}

int
_cond_destroy()
{
	return (0);
}

int
_cond_init()
{
	return (0);
}

int
_cond_signal()
{
	return (0);
}

int
_cond_timedwait()
{
	return (0);
}

int
_cond_wait()
{
	return (0);
}

int
_mutex_destroy()
{
	return (0);
}

int
_mutex_init()
{
	return (0);
}

int
_mutex_lock()
{
	return (0);
}

int
_mutex_trylock()
{
	return (0);
}

int
_mutex_unlock()
{
	return (0);
}

/* ARGSUSED */
int
_mutex_held(mutex_t *mp)
{
	return (0);
}

int
_rw_rdlock()
{
	return (0);
}

int
_rw_tryrdlock()
{
	return (0);
}

int
_rw_trywrlock()
{
	return (0);
}

int
_rw_unlock()
{
	return (0);
}

int
_rw_wrlock()
{
	return (0);
}

int
_rwlock_init()
{
	return (0);
}

/* ARGSUSED */
int
_rw_read_held(rwlock_t *rw)
{
	return (0);
}

/* ARGSUSED */
int
_rw_write_held(rwlock_t *rw)
{
	return (0);
}

int
_sema_init()
{
	return (0);
}

int
_sema_destroy()
{
	return (0);
}

int
_sema_post()
{
	return (0);
}

int
_sema_trywait()
{
	return (0);
}

int
_sema_wait()
{
	return (0);
}

int
_sema_wait_cancel()
{
	return (0);
}

/* ARGSUSED */
int
_sema_held(sema_t *sp)
{
	return (0);
}

int
_thr_continue()
{
	return (0);
}

int
_thr_create()
{
	return (-1);
}

int
_thr_exit()
{
	return (0);
}

int
_thr_getconcurrency()
{
	return (0);
}

int
_thr_getprio()
{
	return (0);
}

int
_thr_getspecific()
{
	return (0);
}

int
_thr_join()
{
	return (0);
}

int
_thr_keycreate()
{
	return (0);
}

int
_thr_kill()
{
	return (0);
}

int
_thr_self()
{
	return (1);
}

int
_thr_setconcurrency()
{
	return (0);
}

int
_thr_setprio()
{
	return (0);
}

int
_thr_setspecific()
{
	return (0);
}

int
_thr_sigsetmask()
{
	return (0);
}

int
_thr_suspend()
{
	return (0);
}

int
_thr_yield()
{
	return (0);
}

int *
_thr_errnop()
{
	return (0);
}

int
_thr_main()
{
	return (-1);
}

size_t
_thr_min_stack()
{
	return (0);
}

int
_thr_stksegment()
{
	return (-1);
}

/*
 * The symbols thr_probe_setup, thr_probe_getfunc_addr, _resume_ret,
 * _resume, and __thr_door_unbind don't have a pragma weak associated
 * with them because they are not exported as weak by libthread
 */
/* ARGSUSED */
void
thr_probe_setup(void *data)
{

}

void * (*thr_probe_getfunc_addr)(void) = 0;

/* ARGSUSED */
void
_resume_ret(void *oldthread)
{

}

/* ARGSUSED */
void
_resume(void *x, caddr_t y, int z)
{

}

int
__thr_door_unbind()
{
	return (-1);
}

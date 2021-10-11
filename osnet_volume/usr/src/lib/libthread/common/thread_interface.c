/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)thread_interface.c	1.39	99/11/02 SMI"

/*
 * Inform libc and the run-time linker of the address of the threads interface.
 *
 * In a multi-threaded dynamic application, the run-time linker must insure
 * concurrency when performing such things as function (.plt) binding, and
 * dlopen and dlclose functions.  To be consistent with the threads mechanisms,
 * (and as light weight as possible), the threads .init routine provides for
 * selected mutex function addresses to be passed to ld.so.1.
 */
#include	"libthread.h"
#include	<thread.h>
#include	<sys/link.h>
#include	<unistd.h>

#include	"thr_int.h"

extern void	_libc_threads_interface(Thr_interface *);

/*
 * Static functions
 */
static	int _llrw_rdlock(rwlock_t *);
static	int _llrw_wrlock(rwlock_t *);
static	int _llrw_unlock(rwlock_t *);
static	int _bind_guard(int);
static	int _bind_clear(int);

Thr_interface _ti_funcs[TI_MAX] = {
/* 01 */	{ TI_MUTEX_LOCK,	_ti_mutex_lock },
/* 02 */	{ TI_MUTEX_UNLOCK,	_ti_mutex_unlock },
/* 03 */	{ TI_LRW_RDLOCK,	_llrw_rdlock },
/* 04 */	{ TI_LRW_WRLOCK,	_llrw_wrlock },
/* 05 */	{ TI_LRW_UNLOCK,	_llrw_unlock },
/* 06 */	{ TI_BIND_GUARD,	_bind_guard },
/* 07 */	{ TI_BIND_CLEAR,	_bind_clear },
/* 08 */	{ TI_LATFORK,		_lpthread_atfork },
/* 09 */	{ TI_THRSELF,		_ti_thr_self },
/* 10 */	{ TI_VERSION,		(int (*)())TI_V_CURRENT },
/* 11 */	{ TI_COND_BROAD,	_ti_cond_broadcast },
/* 12 */	{ TI_COND_DESTROY,	_ti_cond_destroy },
/* 13 */	{ TI_COND_INIT,		_ti_cond_init },
/* 14 */	{ TI_COND_SIGNAL,	_ti_cond_signal },
/* 15 */	{ TI_COND_TWAIT,	_ti_cond_timedwait },
/* 16 */	{ TI_COND_WAIT,		_ti_cond_wait },
/* 17 */	{ TI_FORK,		_ti_fork },
/* 18 */	{ TI_FORK1,		_ti_fork1 },
/* 19 */	{ TI_MUTEX_DEST,	_ti_mutex_destroy },
/* 20 */	{ TI_MUTEX_HELD,	_ti_mutex_held },
/* 21 */	{ TI_MUTEX_INIT,	_ti_mutex_init },
/* 22 */	{ TI_MUTEX_TRYLCK,	_ti_mutex_trylock },
/* 23 */	{ TI_ATFORK,		_ti_pthread_atfork },
/* 24 */	{ TI_RW_RDHELD,		_ti_rw_read_held },
/* 25 */	{ TI_RW_RDLOCK,		_ti_rw_rdlock },
/* 26 */	{ TI_RW_WRLOCK,		_ti_rw_wrlock },
/* 27 */	{ TI_RW_UNLOCK,		_ti_rw_unlock },
/* 28 */	{ TI_TRYRDLOCK,		_ti_rw_tryrdlock },
/* 29 */	{ TI_TRYWRLOCK,		_ti_rw_trywrlock },
/* 30 */	{ TI_RW_WRHELD,		_ti_rw_write_held },
/* 31 */	{ TI_RWLOCKINIT,	_ti_rwlock_init },
/* 32 */	{ TI_SEM_HELD,		_ti_sema_held },
/* 33 */	{ TI_SEM_INIT,		_ti_sema_init },
/* 34 */	{ TI_SEM_POST,		_ti_sema_post },
/* 35 */	{ TI_SEM_TRYWAIT,	_ti_sema_trywait },
/* 36 */	{ TI_SEM_WAIT,		_ti_sema_wait },
/* 37 */	{ TI_SIGACTION,		_ti_sigaction },
/* 38 */	{ TI_SIGPROCMASK,	_ti_sigprocmask },
/* 39 */	{ TI_SIGWAIT,		_ti_sigwait },
/* 40 */	{ TI_SLEEP,		_ti_sleep },
/* 41 */	{ TI_THR_CONT,		_ti_thr_continue },
/* 42 */	{ TI_THR_CREATE,	_ti_thr_create },
/* 43 */	{ TI_THR_ERRNOP,	_ti_thr_errnop },
/* 44 */	{ TI_THR_EXIT,		_ti_thr_exit },
/* 45 */	{ TI_THR_GETCONC,	_ti_thr_getconcurrency },
/* 46 */	{ TI_THR_GETPRIO,	_ti_thr_getprio },
/* 47 */	{ TI_THR_GETSPEC,	_ti_thr_getspecific },
/* 48 */	{ TI_THR_JOIN,		_ti_thr_join },
/* 49 */	{ TI_THR_KEYCREAT,	_ti_thr_keycreate },
/* 50 */	{ TI_THR_KILL,		_ti_thr_kill },
/* 51 */	{ TI_THR_MAIN,		_ti_thr_main },
/* 52 */	{ TI_THR_SETCONC,	_ti_thr_setconcurrency },
/* 53 */	{ TI_THR_SETPRIO,	_ti_thr_setprio },
/* 54 */	{ TI_THR_SETSPEC,	_ti_thr_setspecific },
/* 55 */	{ TI_THR_SIGSET,	_ti_thr_sigsetmask },
/* 56 */	{ TI_THR_STKSEG,	_ti_thr_stksegment },
/* 57 */	{ TI_THR_SUSPEND,	_ti_thr_suspend },
/* 58 */	{ TI_THR_YIELD,		_ti_thr_yield },
/* 59 */	{ TI_CLOSE,		_ti_close },
/* 60 */	{ TI_CREAT,		_ti_creat },
/* 61 */	{ TI_FCNTL,		_ti_fcntl },
/* 62 */	{ TI_FSYNC,		_ti_fsync },
/* 63 */	{ TI_MSYNC,		_ti_msync },
/* 64 */	{ TI_OPEN,		_ti_open },
/* 65 */	{ TI_PAUSE,		_ti_pause },
/* 66 */	{ TI_READ,		_ti_read },
/* 67 */	{ TI_SIGSUSPEND,	_ti_sigsuspend },
/* 68 */	{ TI_TCDRAIN,		_ti_tcdrain },
/* 69 */	{ TI_WAIT,		_ti_wait },
/* 70 */	{ TI_WAITPID,		_ti_waitpid },
/* 71 */	{ TI_WRITE,		_ti_write },
/* 72 */	{ TI_PCOND_BROAD,	_ti_pthread_cond_broadcast },
/* 73 */	{ TI_PCOND_DEST,	_ti_pthread_cond_destroy },
/* 74 */	{ TI_PCOND_INIT,	_ti_pthread_cond_init },
/* 75 */	{ TI_PCOND_SIGNAL,	_ti_pthread_cond_signal },
/* 76 */	{ TI_PCOND_TWAIT,	_ti_pthread_cond_timedwait },
/* 77 */	{ TI_PCOND_WAIT,	_ti_pthread_cond_wait },
/* 78 */	{ TI_PCONDA_DEST,	_ti_pthread_condattr_destroy },
/* 79 */	{ TI_PCONDA_GETPS,	_ti_pthread_condattr_getpshared },
/* 80 */	{ TI_PCONDA_INIT,	_ti_pthread_condattr_init },
/* 81 */	{ TI_PCONDA_SETPS,	_ti_pthread_condattr_setpshared },
/* 82 */	{ TI_PMUTEX_DEST,	_ti_pthread_mutex_destroy },
/* 83 */	{ TI_PMUTEX_GPC,	_ti_pthread_mutex_getprioceiling },
/* 84 */	{ TI_PMUTEX_INIT,	_ti_pthread_mutex_init },
/* 85 */	{ TI_PMUTEX_LOCK,	_ti_pthread_mutex_lock },
/* 86 */	{ TI_PMUTEX_SPC,	_ti_pthread_mutex_setprioceiling },
/* 87 */	{ TI_PMUTEX_TRYL,	_ti_pthread_mutex_trylock },
/* 88 */	{ TI_PMUTEX_UNLCK,	_ti_pthread_mutex_unlock },
/* 89 */	{ TI_PMUTEXA_DEST,	_ti_pthread_mutexattr_destroy },
/* 90 */	{ TI_PMUTEXA_GPC,	_ti_pthread_mutexattr_getprioceiling },
/* 91 */	{ TI_PMUTEXA_GP,	_ti_pthread_mutexattr_getprotocol },
/* 92 */	{ TI_PMUTEXA_GPS,	_ti_pthread_mutexattr_getpshared },
/* 93 */	{ TI_PMUTEXA_INIT,	_ti_pthread_mutexattr_init },
/* 94 */	{ TI_PMUTEXA_SPC,	_ti_pthread_mutexattr_setprioceiling },
/* 95 */	{ TI_PMUTEXA_SP,	_ti_pthread_mutexattr_setprotocol },
/* 96 */	{ TI_PMUTEXA_SPS,	_ti_pthread_mutexattr_setpshared },
/* 97 */	{ TI_THR_MINSTACK,	_ti_thr_min_stack },
/* 98 */	{ TI_SIGTIMEDWAIT,	_ti_sigtimedwait },
/* 99 */	{ TI_ALARM,		_ti_alarm },
/* 100 */	{ TI_SETITIMER,		_ti_setitimer },
/* 103 */	{ TI_SIGPENDING,	_ti_sigpending },
/* 104 */	{ TI__NANOSLEEP,	_ti__nanosleep },
/* 105 */	{ TI_OPEN64,		_ti_open64 },
/* 106 */	{ TI_CREAT64,		_ti_creat64 },
/* 107 */	{ TI_RWLCKDESTROY, 	_ti_rwlock_destroy },
/* 108 */	{ TI_SEMADESTROY,	_ti_sema_destroy },
/* 109 */	{ TI_PSELF,		_ti_pthread_self },
/* 110 */	{ TI_PSIGMASK,		_ti_pthread_sigmask },
/* 111 */	{ TI_PATTR_DEST,	_ti_pthread_attr_destroy },
/* 112 */	{ TI_PATTR_GDS,		_ti_pthread_attr_getdetachstate },
/* 113 */	{ TI_PATTR_GIS,		_ti_pthread_attr_getinheritsched },
/* 114 */	{ TI_PATTR_GSPA,	_ti_pthread_attr_getschedparam },
/* 115 */	{ TI_PATTR_GSPO,	_ti_pthread_attr_getschedpolicy },
/* 116 */	{ TI_PATTR_GSCOP,	_ti_pthread_attr_getscope },
/* 117 */	{ TI_PATTR_GSTAD,	_ti_pthread_attr_getstackaddr },
/* 118 */	{ TI_PATTR_GSTSZ,	_ti_pthread_attr_getstacksize },
/* 119 */	{ TI_PATTR_INIT,	_ti_pthread_attr_init },
/* 120 */	{ TI_PATTR_SDEST,	_ti_pthread_attr_setdetachstate },
/* 121 */	{ TI_PATTR_SIS,		_ti_pthread_attr_setinheritsched },
/* 122 */	{ TI_PATTR_SSPA,	_ti_pthread_attr_setschedparam },
/* 123 */	{ TI_PATTR_SSPO,	_ti_pthread_attr_setschedpolicy },
/* 124 */	{ TI_PATTR_SSCOP,	_ti_pthread_attr_setscope },
/* 125 */	{ TI_PATTR_SSTAD,	_ti_pthread_attr_setstackaddr },
/* 126 */	{ TI_PATTR_SSTSZ,	_ti_pthread_attr_setstacksize },
/* 127 */	{ TI_PCANCEL,		_ti_pthread_cancel },
/* 128 */	{ TI_PCLEANUP_POP,	_ti__pthread_cleanup_pop },
/* 129 */	{ TI_PCLEANUP_PSH,	_ti__pthread_cleanup_push },
/* 130 */	{ TI_PCREATE,		_ti_pthread_create },
/* 131 */	{ TI_PDETACH,		_ti_pthread_detach },
/* 132 */	{ TI_PEQUAL,		_ti_pthread_equal },
/* 133 */	{ TI_PEXIT,		_ti_pthread_exit },
/* 134 */	{ TI_PGSCHEDPARAM,	_ti_pthread_getschedparam },
/* 135 */	{ TI_PGSPECIFIC,	_ti_pthread_getspecific },
/* 136 */	{ TI_PJOIN,		_ti_pthread_join },
/* 137 */	{ TI_PKEY_CREATE,	_ti_pthread_key_create },
/* 138 */	{ TI_PKEY_DELETE,	_ti_pthread_key_delete },
/* 139 */	{ TI_PKILL,		_ti_pthread_kill },
/* 140 */	{ TI_PONCE,		_ti_pthread_once },
/* 141 */	{ TI_PSCANCELST,	_ti_pthread_setcancelstate },
/* 142 */	{ TI_PSCANCELTYPE,	_ti_pthread_setcanceltype },
/* 143 */	{ TI_PSSCHEDPARAM,	_ti_pthread_setschedparam },
/* 144 */	{ TI_PSETSPECIFIC,	_ti_pthread_setspecific },
/* 145 */	{ TI_PTESTCANCEL,	_ti_pthread_testcancel },
/* 146 */	{ TI_KILL,		_ti_kill },
/* 147 */	{ TI_PGETCONCUR,	_ti_pthread_getconcurrency },
/* 148 */	{ TI_PSETCONCUR,	_ti_pthread_setconcurrency },
/* 149 */	{ TI_PATTR_GGDSZ,	_ti_pthread_attr_getguardsize },
/* 150 */	{ TI_PATTR_SGDSZ,	_ti_pthread_attr_setguardsize },
/* 151 */	{ TI_PMUTEXA_GTYP,	_ti_pthread_mutexattr_gettype},
/* 152 */	{ TI_PMUTEXA_STYP,	_ti_pthread_mutexattr_settype},
/* 153 */	{ TI_PRWL_INIT,		_ti_pthread_rwlock_init },
/* 154 */	{ TI_PRWL_DEST,		_ti_pthread_rwlock_destroy },
/* 155 */	{ TI_PRWLA_INIT,	_ti_pthread_rwlockattr_init },
/* 156 */	{ TI_PRWLA_DEST,	_ti_pthread_rwlockattr_destroy },
/* 157 */	{ TI_PRWLA_GPSH,	_ti_pthread_rwlockattr_getpshared},
/* 158 */	{ TI_PRWLA_SPSH,	_ti_pthread_rwlockattr_setpshared},
/* 159 */	{ TI_PRWL_RDLK,		_ti_pthread_rwlock_rdlock },
/* 160 */	{ TI_PRWL_TRDLK,	_ti_pthread_rwlock_tryrdlock },
/* 161 */	{ TI_PRWL_WRLK,		_ti_pthread_rwlock_wrlock },
/* 162 */	{ TI_PRWL_TWRLK,	_ti_pthread_rwlock_trywrlock },
/* 163 */	{ TI_PRWL_UNLK,		_ti_pthread_rwlock_unlock },
/* 164 */	{ TI_GETMSG,		_ti_getmsg },
/* 165 */	{ TI_GETPMSG,		_ti_getpmsg },
/* 166 */	{ TI_PUTMSG,		_ti_putmsg },
/* 167 */	{ TI_PUTPMSG,		_ti_putpmsg },
/* 168 */	{ TI_LOCKF,		_ti_lockf },
/* 169 */	{ TI_MSGRCV,		_ti_msgrcv },
/* 170 */	{ TI_MSGSND,		_ti_msgsnd },
/* 171 */	{ TI_POLL,		_ti_poll },
/* 172 */	{ TI_PREAD,		_ti_pread },
/* 173 */	{ TI_READV,		_ti_readv },
/* 174 */	{ TI_PWRITE,		_ti_pwrite },
/* 175 */	{ TI_WRITEV,		_ti_writev },
/* 176 */	{ TI_SELECT,		_ti_select },
/* 177 */	{ TI_SIGPAUSE,		_ti_sigpause },
/* 178 */	{ TI_USLEEP,		_ti_usleep },
/* 179 */	{ TI_WAIT3,		_ti_wait3 },
/* 181 */	{ TI_LOCKF64,		_ti_lockf64 },
/* 182 */	{ TI_PREAD64,		_ti_pread64 },
/* 183 */	{ TI_PWRITE64,		_ti_pwrite64 },
/* 184 */	{ TI_XPG4PUTMSG,	_ti_xpg4_putmsg },
/* 185 */	{ TI_XPG4PUTPMSG,	_ti_xpg4_putpmsg },
/* 00 */	{ TI_NULL,		0 }
};

/*
 * _thr_libthread() is used to identify the link order
 * of libc.so vs. libthread.so.  There is a copy of each in
 * both libraries.  They return the following:
 *
 *	libc:_thr_libthread(): returns 0
 *	libthread:_thr_libthread(): returns 1
 *
 * A call to this routine can be used to determine whether or
 * not the libc threads interface needs to be initialized or not.
 */
int
_thr_libthread(void)
{
	return (1);
}

static int
_llrw_rdlock(rwlock_t * _lrw_lock)
{
	return (_lrw_rdlock(_lrw_lock));
}

static int
_llrw_wrlock(rwlock_t * _lrw_lock)
{
	return (_lrw_wrlock(_lrw_lock));
}

static int
_llrw_unlock(rwlock_t * _lrw_lock)
{
	return (_lrw_unlock(_lrw_lock));
}

#ifdef BUILD_STATIC
#pragma	weak	_ld_concurrency
#pragma	weak	_libc_threads_interface
#endif

void
_set_rtld_interface(void)
{
	void (*	fptr)(void *);

	if ((fptr = _ld_concurrency) != 0)
		(* fptr)(_ti_funcs);
}

void
_set_libc_interface(void)
{
	void (*	fptr)(Thr_interface *);
	if ((fptr = _libc_threads_interface) != 0)
		(*fptr)(_ti_funcs);
}


void
_unset_rtld_interface(void)
{
	void (* fptr)(void *);

	if ((fptr = _ld_concurrency) != 0)
		(* fptr)((void *)0);
}

void
_unset_libc_interface(void)
{
	void (* fptr)(Thr_interface *);

	if ((fptr = _libc_threads_interface) != 0)
		(* fptr)((Thr_interface *)0);
}

/*
 * When ld.so.1 calls one of the mutex functions while binding a .plt its
 * possible that the function itself may require further .plt binding.  To
 * insure that this binding does not cause recursion we set the t_rtldbind
 * flags within the current thread.
 *
 * Note, because we may have .plt bindings occurring prior to %g7 being
 * initialized we must allow for _curthread() == 0.
 *
 * Args:
 *	bindflags:	value of flag(s) to be set in the t_rtldbind field
 *
 * Returns:
 *	0:	if setting the flag(s) results in no change to the t_rtldbind
 *		value.  ie: all the flags were already set.
 *	1:	if a flag(or flags) were not set they have been set.
 */
static int
_bind_guard(int bindflags)
{
	uthread_t 	*thr;
	extern int _nthreads;

	if (_nthreads == 0) /* .init processing - do not need to acquire lock */
		return (0);
	if ((thr = _curthread()) == NULL)
		/*
		 * thr == 0 implies the thread is a bound thread on its
		 * way to die in _lwp_terminate(), just before calling
		 * _lwp_exit(). Should acquire lock in this case - so return 1.
		 */
		return (1);
	else if ((thr->t_rtldbind & bindflags) == bindflags)
		return (0);
	else {
		thr->t_rtldbind |= bindflags;
		return (1);
	}
}

/*
 * Args:
 *	bindflags:	value of flags to be cleared from the t_rtldbind field
 * Returns:
 *	resulting value of t_rtldbind
 *
 * Note: if ld.so.1 needs to query t_rtldbind it can pass in a value
 *	 of '0' to clear_bind() and examine the return code.
 */
static int
_bind_clear(int bindflags)
{
	uthread_t *	thr;
	if (_totalthreads > 0 && (thr = _curthread()) != NULL)
		return (thr->t_rtldbind &= ~bindflags);
	else
		return (0);
}

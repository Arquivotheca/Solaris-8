/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)mutex.c	1.28	99/10/07 SMI"

/*
 * This file contains the basic functions to implement local mutex's.
 * Mutex's are used for two purposes:
 *
 *	i.	to maintain profile buffer data consistency when multiple
 *		processes are profiling the same library (multi-threaded or
 *		single-threaded applications), and
 *
 *	ii.	to ensure the internal consistency of such things as link-map
 *		and malloc structures (multi-threaded applications only).
 *
 * The first case is supported by maintaining a mutex within the output
 * data buffer (refer to profile.h).
 *
 * The second case is supported using readers/writers locks.  Readers
 * are the binder functions and dlsym, these functions traverse the link maps
 * but don't alter them.  Writers are dlopen and dlclose, and malloc.
 *
 * In a single-threaded application only mutex support for case i) is
 * provided.  Support for case ii) is only really necessary to provide for
 * dlopen/dlclose within a signal handler.  However, because of the signal
 * blocking required to support all mutexs, case ii) becomes prohibitively
 * expensive for normal operation.
 *
 * For multi-threaded applications, support for both cases is required.  To
 * implement both the threads library informs ld.so.1 of the necessary mutex
 * functions.  This allows the run-time linker to use the real threads interface
 * which is capable of handling signal blocking at a much lower cost.
 *
 * The global variable ti_version is used to represent the current version of
 * the libthread interface.  It's possible values are represented below:
 *
 *	0:	initial interface between libthread.so.1 & ld.so.1
 *		the routines that were supported are:
 *
 *			TI_MUTEX_LOCK
 *			TI_MUTEX_UNLOCK
 *			* In this version the TI_RW_*LOCK routines
 *			* actually ignore their arguement. libthread
 *			* uses a single mutex lock for all calls.
 *			TI_RW_RDLOCK
 *			TI_RW_WRLOCK
 *			TI_RW_UNLOCK
 *			TI_BIND_GUARD
 *			TI_BIND_CLEAR
 *		Note: calls to fork1() in libthread would actually do the
 *			'locking' and unlocking of the run-time linkers lock.
 *
 *	1:	This interface contained the following routines:
 *			TI_MUTEX_LOCK
 *			TI_MUTEX_UNLOCK
 *			TI_LRW_RDLOCK
 *			TI_LRW_WRLOCK
 *			TI_LRW_UNLOCK
 *			* The bind_guard()/bind_clear() routines now
 *			* take a arguement which sets a 'binding' flag
 *			* inside of the specific thread.  ld.so.1 now
 *			* has a possible range of 32 values that it can pass.
 *			TI_BIND_GUARD
 *			TI_BIND_CLEAR
 *			TI_ATFORK
 *			TI_THRSELF
 *			TI_VERSION
 *		Note: ld.so.1 now uses the atfork() routine to protect
 *			it's own locks against a call to fork1() and libthread
 *			no longer has knowledge of a specific mutexlock
 *			inside of ld.so.1.
 */
#include	"_synonyms.h"

#include	<synch.h>
#include	<signal.h>
#include	<thread.h>
#include	<synch.h>
#include	"thr_int.h"
#include	"_rtld.h"

/*
 * When handling mutex's locally we need to mask signals.  The signal mask
 * is for everything except SIGWAITING.
 */
static const sigset_t	iset = { ~0U, ~0U, ~0U, ~0U };

/*
 * Define our own local mutex functions.
 */

static int
_rt_mutex_lock(mutex_t * mp, sigset_t * oset)
{
	if (oset)
		(void) sigprocmask(SIG_BLOCK, &iset, oset);
	(void) _lwp_mutex_lock(mp);
	return (0);
}

static int
_rt_mutex_unlock(mutex_t * mp, sigset_t * oset)
{
	(void) _lwp_mutex_unlock(mp);
	if (oset)
		(void) sigprocmask(SIG_SETMASK, oset, NULL);
	return (0);
}

static int	bindguardbitmask = 0;

static int
local_bind_guard(int bit)
{
	if ((bit & bindguardbitmask) == 0) {
		bindguardbitmask |= bit;
		return (1);
	}
	return (0);
}

static int
local_bind_clear(int bit)
{
	bindguardbitmask &= ~bit;
	return (0);
}

static int
_null()
{
	return (0);
}

/*
 * These three routines are used to protect the locks that
 * ld.so.1 has.  They are passed to pthread_atfork() and are used
 * during a fork1() to make sure that we do not do a fork while
 * a lock is being held.
 */
static void
prepare_atfork(void)
{
	(void) bind_guard(THR_FLG_MASK);
	(void) rw_wrlock(&bindlock);
	(void) rw_wrlock(&initlock);
	(void) rw_wrlock(&boundlock);
	(void) rw_wrlock(&printlock);
	(void) rw_wrlock(&malloclock);
}

static void
child_atfork(void)
{
	(void) rw_unlock(&bindlock);
	(void) rw_unlock(&initlock);
	(void) rw_unlock(&boundlock);
	(void) rw_unlock(&malloclock);
	(void) rw_unlock(&printlock);
	(void) bind_clear(THR_FLG_MASK);
}

static void
parent_atfork(void)
{
	(void) rw_unlock(&bindlock);
	(void) rw_unlock(&initlock);
	(void) rw_unlock(&boundlock);
	(void) rw_unlock(&malloclock);
	(void) rw_unlock(&printlock);
	(void) bind_clear(THR_FLG_MASK);
}

/*
 * Define the maximum number of thread interfaces ld.so.1 is interested in,
 * this is a subset of the total number of interfaces communicated between
 * libthread and libc.
 */
#define	STI_MAX			11

/*
 * List of those entries from the Thread_Interface structure that
 * ld.so.1 is interested in.
 */
static int ti_interest_list[STI_MAX] = {
/* TI_NULL */			0,
/* TI_MUTEX_LOCK */		0,
/* TI_MUTEX_UNLOCK */		0,
/* TI_LRW_RDLOCK */		1,
/* TI_LRW_WRLOCK */		1,
/* TI_LRW_UNLOCK */		1,
/* TI_BIND_GUARD */		1,
/* TI_BIND_CLEAR */		1,
/* TI_LATFORK */		1,
/* TI_THRSELF */		1,
/* TI_VERSION */		1,
};

/*
 * Define the linkers concurrency table, a function pointer array for holding
 * the mutex function addresses.
 */
static int (* 	ti_def_table[STI_MAX])() = {
	0,					/* TI_NULL */
	_rt_mutex_lock,				/* TI_MUTEX_LOCK */
	_rt_mutex_unlock,			/* TI_MUTEX_UNLOCK */
	_null,					/* TI_LRW_RDLOCK */
	_null,					/* TI_LRW_WRLOCK */
	_null,					/* TI_LRW_UNLOCK */
	local_bind_guard,			/* TI_BIND_GUARD */
	local_bind_clear,			/* TI_BIND_CLEAR */
	_null,					/* TI_LATFORK */
	_null,					/* TI_THRSELF */
	0					/* TI_VERSION */
};

static int (* 	ti_jmp_table[STI_MAX])() = {
	0,					/* TI_NULL */
	_rt_mutex_lock,				/* TI_MUTEX_LOCK */
	_rt_mutex_unlock,			/* TI_MUTEX_UNLOCK */
	_null,					/* TI_LRW_RDLOCK */
	_null,					/* TI_LRW_WRLOCK */
	_null,					/* TI_LRW_UNLOCK */
	local_bind_guard,			/* TI_BIND_GUARD */
	local_bind_clear,			/* TI_BIND_CLEAR */
	_null,					/* TI_LATFORK */
	_null,					/* TI_THRSELF */
	0					/* TI_VERSION */
};

/*
 * The interface with the threads library which is supplied through libdl.so.1.
 * A non-null argument allows a function pointer array to be passed to us which
 * is used to re-initialize the linker concurrency table.  A null argument
 * causes the table to be reset to the defaults.
 */
void
_ld_concurrency(void * ptr)
{
	int		tag;
	Thr_interface *	funcs = ptr;

	if (funcs) {
		for (tag = funcs->ti_tag; tag; tag = (++funcs)->ti_tag) {
			if ((tag < STI_MAX) && ti_interest_list[tag] &&
			    (funcs->ti_un.ti_func != 0))
				ti_jmp_table[tag] = funcs->ti_un.ti_func;
		}
		(void) bind_clear((int)0xffffffff);
		(void) bind_guard(bindguardbitmask);

		/*
		 *  If the runtime linker locked any of the following
		 * locks using the stub routine, truly lock them
		 * so runtime linker can unlock() them safely later.
		 *  Unlocking not locked object is dangerous and will have
		 * the same affect as giving read/write lock. So the
		 * rw_rwlock() following the unlock() will be a dead lock.
		 *  See bug 4160709.
		 */
		if (bindguardbitmask & THR_FLG_BIND)
			(void) rw_wrlock(&bindlock);
		if (bindguardbitmask & THR_FLG_INIT)
			(void) rw_wrlock(&initlock);
		if (bindguardbitmask & THR_FLG_BOUND)
			(void) rw_wrlock(&boundlock);
		if (bindguardbitmask & THR_FLG_STRERR)
			(void) rw_wrlock(&printlock);
		if (bindguardbitmask & THR_FLG_MALLOC)
			(void) rw_wrlock(&malloclock);

		(void) rt_atfork(prepare_atfork, parent_atfork, child_atfork);
		rtld_flags |= RT_FL_THREADS;
	} else {
		/*
		 * It is possible that we got here via a call to
		 * dlclose().  If this is so then a BIND lock
		 * is currently in place, we must clear that lock.
		 */
		if (bind_clear(0x0) & THR_FLG_BIND) {
			(void) rw_unlock(&bindlock);
			(void) bind_clear(THR_FLG_BIND);
		}
		rtld_flags &= ~RT_FL_THREADS;
		for (tag = 0; tag < STI_MAX; tag++)
			ti_jmp_table[tag] = ti_def_table[tag];
	}
	ti_version = (Xword)ti_jmp_table[TI_VERSION];
}

/*
 * Define the local interface for each of the mutex calls.
 */
int
rt_mutex_lock(mutex_t * mp, sigset_t * oset)
{
	return ((* ti_jmp_table[TI_MUTEX_LOCK])(mp, oset));
}

int
rt_mutex_unlock(mutex_t * mp, sigset_t * oset)
{
	return ((* ti_jmp_table[TI_MUTEX_UNLOCK])(mp, oset));
}

int
rw_rdlock(rwlock_t * rwlp)
{
	return ((* ti_jmp_table[TI_LRW_RDLOCK])(rwlp));
}

int
rw_wrlock(rwlock_t * rwlp)
{
	return ((* ti_jmp_table[TI_LRW_WRLOCK])(rwlp));
}

int
rw_unlock(rwlock_t * rwlp)
{
	return ((* ti_jmp_table[TI_LRW_UNLOCK])(rwlp));
}

int
bind_guard(int bindflag)
{
	return ((* ti_jmp_table[TI_BIND_GUARD])(bindflag));
}

int
bind_clear(int bindflag)
{
	return ((* ti_jmp_table[TI_BIND_CLEAR])(bindflag));
}

int
rt_atfork(void (* prepare)(void), void (* parent)(void), void (* child)(void))
{
	return ((* ti_jmp_table[TI_LATFORK])(prepare, parent, child));
}

thread_t
thr_self()
{
	return ((* ti_jmp_table[TI_THRSELF])());
}

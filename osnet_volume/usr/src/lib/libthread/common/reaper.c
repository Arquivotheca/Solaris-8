/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)reaper.c	1.34	98/02/02 SMI"

#include "libthread.h"
/*
 * Global variables
 */
uthread_t *_onprocq = NULL;	/* circular queue of ONPROC threads */
uthread_t *_zombies = NULL;	/* circular queue of zombie threads */
uthread_t *_deathrow = NULL;	/* circular queue of dead threads */
cond_t _zombied = DEFAULTCV;	/* waiting for zombie threads */
int _zombiecnt = 0;		/* number of zombied threads */
int _reapcnt = 0;		/* number of threads to be reaped */
lwp_mutex_t _reaplock;		/* reaper thread's lock */
lwp_cond_t _untilreaped;	/* wait until _reapcnt < high mark */

/*
 * Static variables
 */
static	int _REAPLIMIT = 16;	/* max number of reapable threads */
static	int _reaper_tid = 0;	/* reaper thread's tid */
static	cond_t _reap_cv;	/* reaper blocks on this cond. var. */
static	int	_wake_reaper = 0; /* when set reaper thread is running */
static	uthread_t *_reaper_thread;

/*
 * Static functions
 */
static	void _reap(void);
static	void _reaper(void);


/*
 * only threads created with THR_DETACHED flag set will be placed
 * on deathrow. a reaper thread runs periodically to reclaim the
 * resources of these dead threads. a thread created without the
 * THR_DETACHED flag is expected to be reclaimed by the application
 * via thr_join().
 */
void
_reapq_add(uthread_t *t)
{

	/*
	 * No need to call _reap_lock() here since _reapq_add() is called only
	 * inside _resume_ret() which already has signals blocked via t_nosig
	 * So, just call the async-unsafe _lwp_mutex_lock() routine here.
	 */
	_lwp_mutex_lock(&_reaplock);
	if (DETACHED(t)) {
		ASSERT(_reaper_tid != NULL);
		if (_deathrow == NULL) {
			_deathrow = t;
			t->t_iforw = t->t_ibackw = t;
		} else {
			_deathrow->t_ibackw->t_iforw = t;
			t->t_iforw = _deathrow;
			t->t_ibackw = _deathrow->t_ibackw;
			_deathrow->t_ibackw = t;
		}
		_reapcnt++;
		if (_nidlecnt || _naging || _reapcnt >= _REAPLIMIT) {
			_cond_signal(&_reap_cv);
		}
	} else {
		if (t->t_usropts & THR_DAEMON)
			_d2bzombies--;
		else
			_u2bzombies--; /* transition to zombie state is over */
		ASSERT(_u2bzombies >= 0);
		ASSERT(_d2bzombies >= 0);
		_zombiecnt++;
		ASSERT(t->t_iforw == t->t_ibackw && t->t_iforw == NULL);
		if (_zombies == NULL) {
			_zombies = t;
			t->t_iforw = t->t_ibackw = t;
		} else {
			_zombies->t_ibackw->t_iforw = t;
			t->t_iforw = _zombies;
			t->t_ibackw = _zombies->t_ibackw;
			_zombies->t_ibackw = t;
		}
		t->t_flag |= T_ZOMBIE;
		t->t_flag &= ~T_2BZOMBIE;
		_cond_broadcast(&_zombied);
	}
	/*
	 * BOUND thread's will release _reaplock when they're no
	 * longer running on their thread's stack. This happens
	 * in lwp_terminate().
	 */
	if (!ISBOUND(t))
		_lwp_mutex_unlock(&_reaplock);
}


static void
_reap(void)
{
	uthread_t *next, *t;
	int oldreapcnt, ix;

	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_REAPER_START, "reap start");
	_reap_lock();
	t = _deathrow;
	_deathrow = NULL;
	_reapcnt = 0;
	_lwp_cond_broadcast(&_untilreaped);
	if (!t) {
		_reap_unlock();
		return;
	}
	_reap_unlock();
	t->t_ibackw->t_iforw = NULL;
	while (t) {
		next = t->t_iforw;
		ASSERT(DETACHED(t));
		if (IDLETHREAD(t))
			_thread_free(t);
		else {
			_lock_bucket((ix = HASH_TID(t->t_tid)));
			_thread_destroy(t, ix);
			_unlock_bucket(ix);
		}
		t = next;
	}
	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_REAPER_END, "reap end");
}

static void
_reaper(void)
{
	while (1) {
		_reap_lock();
		while (_deathrow == NULL) {
			_reap_wait(&_reap_cv);
		}
		_wake_reaper = 0;
		_reap_unlock();
		_reap();
	}
}

void
_reaper_create(void)
{
	uthread_t *t, *first;
	int ix;

	/*
	 * XXX: Use big, default stacks for now, since the dynamic linker may
	 * use a huge stack. When this linker bug gets fixed, revert back to
	 * using smaller stacks for daemon threads like the reaper.
	 */
	if (!_alloc_thread(NULL, 0, &t, _lpagesize)) {
		_panic("_reaper_create: _alloc_thread returns 0! - no memory");
	}
	_reaper_thread = t;
	_lmutex_lock(&_tidlock);
	t->t_tid = (thread_t) ++_lasttid;
	_lmutex_unlock(&_tidlock);
	ix = HASH_TID(t->t_tid);
	_lmutex_lock(&(_allthreads[ix].lock));
	if ((first = _allthreads[ix].first) == NULL)
		_allthreads[ix].first = t->t_next = t->t_prev = t;
	else {
		t->t_prev = first->t_prev;
		t->t_next = first;
		first->t_prev->t_next = t;
		first->t_prev = t;
	}
	_lmutex_unlock(&(_allthreads[ix].lock));
	t->t_pri = THREAD_MAX_PRIORITY;
	t->t_usropts = THR_DETACHED|THR_DAEMON;
	t->t_flag |= T_INTERNAL;
	t->t_state = TS_STOPPED;
	t->t_flag |= T_OFFPROC;
	t->t_nosig = 1;
	t->t_stop = TSTP_INTERNAL;
	maskallsigs(&t->t_hold);
	_lwp_sema_init(&t->t_park, NULL);
	t->t_park.sema_type = USYNC_THREAD;

	_thread_call(t, _reaper, NULL);
	_nthreads++; /* no locking required since the caller is _t0init() */
	_reaper_tid = (thread_t) t->t_tid;
	__thr_continue(_reaper_tid);
}

/*
 * This is a temporary hack to get around problem with using
 * USYNC_PROCESS synch objects that are not allocated from shared
 * memory which is the case for the condition variable "_reapcv".
 */
void
_reap_wait(cond_t *cvp)
{
	_sched_lock();
	curthread->t_flag |= T_WAITCV;
	_t_block((caddr_t)cvp);
	cvp->cond_waiters = 1;
	_sched_unlock_nosig();
	_reap_unlock();
	_swtch(0);
	_sigon();
	_reap_lock();
}

/*
 * this is cancel-version of _reap_wait. Only _thr_join
 * uses the cancel-version because it is a cancellation point.
 * All other internal functions should use _reap_wait.
 */
void
_reap_wait_cancel(cond_t *cvp)
{
	_sched_lock();
	curthread->t_flag |= T_WAITCV;
	_t_block((caddr_t)cvp);
	cvp->cond_waiters = 1;
	_sched_unlock_nosig();
	_reap_unlock();
	_cancelon();
	_swtch(0);
	_canceloff();
	_sigon();
	_reap_lock();
}

void
_reap_lock(void)
{
	_sigoff();
	_lwp_mutex_lock(&_reaplock);
}

void
_reap_unlock(void)
{
	_lwp_mutex_unlock(&_reaplock);
	_sigon();
}

int
_alloc_stack_fromreapq(caddr_t *sp)
{
	uthread_t *t;
	int ix;

	_reap_lock();
	if (_deathrow) {
		t = _deathrow;
		do {
			ASSERT(DETACHED(t));
			if (t->t_flag & T_DEFAULTSTK) {
				t->t_iforw->t_ibackw = t->t_ibackw;
				t->t_ibackw->t_iforw = t->t_iforw;
				if (--_reapcnt == 0)
					_deathrow = NULL;
				else if (_deathrow == t)
					_deathrow = t->t_iforw;
				_reap_unlock();
				if (!IDLETHREAD(t)) {
					_lock_bucket((ix = HASH_TID(t->t_tid)));
					_thread_delete(t, ix);
					_unlock_bucket(ix);
				}
				*sp = t->t_stk -  DEFAULTSTACK;
				return (1);
			}
		} while ((t = t->t_iforw) != _deathrow);
	}
	_reap_unlock();
	return (0);
}

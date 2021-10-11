/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)mutex.c	1.69	99/12/06 SMI"

#ifdef __STDC__

#pragma weak mutex_init = _mutex_init
#pragma weak mutex_destroy = _mutex_destroy

#pragma weak pthread_mutex_destroy = _mutex_destroy
#pragma weak pthread_mutex_lock = _pthread_mutex_lock
#pragma weak pthread_mutex_unlock = _pthread_mutex_unlock
#pragma weak pthread_mutex_trylock = _pthread_mutex_trylock
#pragma weak _pthread_mutex_destroy = _mutex_destroy

#pragma weak _ti_pthread_mutex_lock = _pthread_mutex_lock
#pragma	weak _ti_pthread_mutex_unlock = _pthread_mutex_unlock
#pragma weak _ti_mutex_destroy = _mutex_destroy
#pragma weak _ti_pthread_mutex_destroy = _mutex_destroy
#pragma	weak _ti_mutex_held = _mutex_held
#pragma	weak _ti_mutex_init = _mutex_init
#pragma	weak _ti_pthread_mutex_trylock = _pthread_mutex_trylock


#ifdef i386
#pragma weak mutex_lock = _cmutex_lock
#pragma weak mutex_unlock = _cmutex_unlock
#pragma weak mutex_trylock = _cmutex_trylock

#pragma weak _mutex_lock = _cmutex_lock
#pragma weak _mutex_unlock = _cmutex_unlock
#pragma weak _mutex_trylock = _cmutex_trylock

#pragma	weak _ti_mutex_lock = _cmutex_lock
#pragma	weak _ti_mutex_unlock = _cmutex_unlock
#pragma	weak _ti_mutex_trylock = _cmutex_trylock
#endif /* i386 */

#endif /* __STDC__ */
#include "libthread.h"
#include "tdb_agent.h"
#include <stdlib.h>

static	void _mutex_adaptive_lock();
static	void _mutex_adaptive_wakeup();
static	void _mutex_lwp_lock();

extern	int _mutex_unlock_asm();

struct thread *_lock_owner(mutex_t *);

/*
 * Check if a certain mutex is locked.
 */
int
_mutex_held(mutex_t *mp)
{
	if (_lock_held(mp))
		return (1);
	return (0);
}

/*ARGSUSED2*/
int
_mutex_init(mutex_t *mp, int type, void	*arg)
{
	int rc = 0;

	if (type != USYNC_THREAD && type != USYNC_PROCESS &&
		type != USYNC_PROCESS_ROBUST)
		return (EINVAL);
	if (ROBUSTLOCK(type)) {
		/*
		 * Do this in kernel.
		 * Register the USYNC_PROCESS_ROBUST mutex with the process.
		 */
		rc = ___lwp_mutex_init(mp, type);
	} else {
		mp->mutex_type = type;
		mp->mutex_magic = MUTEX_MAGIC;
		mp->mutex_waiters = 0;
		mp->mutex_flag = 0;
		mp->mutex_flag |= LOCK_INITED;
		_lock_clear_adaptive(mp);
	}
	if (!rc && __tdb_attach_stat != TDB_NOT_ATTACHED)
		_tdb_sync_obj_register((caddr_t)mp, MUTEX_MAGIC);
	return (rc);

}

void
_mutex_set_typeattr(mutex_t *mp, int attr)
{
	mp->mutex_type |= (uint8_t)attr;
}

/*ARGSUSED*/
int
_mutex_destroy(mutex_t *mp)
{
	mp->mutex_magic = 0;
	mp->mutex_flag &= ~LOCK_INITED;
	_tdb_sync_obj_deregister((caddr_t)mp);
	return (0);
}

int
_cmutex_lock(mutex_t *mp)
{
	if (ROBUSTLOCK(mp->mutex_type)) {
		return (_mutex_lock_robust(mp));
	}
	if (!_lock_try_adaptive(mp)) {
		if (mp->mutex_type & USYNC_PROCESS)
			_mutex_lwp_lock(mp);
		else
			_mutex_adaptive_lock(mp);
	}
	return (0);
}

int
_cmutex_unlock(mutex_t *mp)
{
	if (ROBUSTLOCK(mp->mutex_type)) {
		return (_mutex_unlock_robust(mp));
	}
	if (_mutex_unlock_asm(mp) > 0) {
			return (_mutex_wakeup(mp));
	}
	return (0);
}

int
_cmutex_trylock(mutex_t *mp)
{

	if (ROBUSTLOCK(mp->mutex_type)) {
		return (_mutex_trylock_robust(mp));
	}
	if (_lock_try_adaptive(mp)) {
		return (0);
	} else {
		if (__td_event_report(curthread, TD_LOCK_TRY)) {
			curthread->t_td_evbuf.eventnum = TD_LOCK_TRY;
			tdb_event_lock_try();
		}
		return (EBUSY);
	}
}

void
_mutex_sema_unlock(mutex_t *mp)
{
	u_char waiters;

	ASSERT(curthread->t_nosig >= 2);
	if (_mutex_unlock_asm(mp) > 0) {
		if (_t_release((caddr_t)mp, &waiters, 0) > 0)
			mp->mutex_waiters = waiters;
	}
	_sigon();
}

#define	MUTEX_MAX_SPIN	100		/* how long to spin before blocking */
#define	MUTEX_CHECK_FREQ 10		/* frequency of checking lwp state */

static void
_mutex_adaptive_lock(mutex_t *mp)
{
	u_char waiters;
	struct thread *owner_t;
	short state;
	int spin_count = 0;

	while (!_lock_try_adaptive(mp)) {
		/* only check owner every so often, including first time */
		if ((spin_count++) % MUTEX_CHECK_FREQ == 0) {
			_sched_lock();
			mp->mutex_magic = MUTEX_MAGIC;
			if (__tdb_attach_stat != TDB_NOT_ATTACHED)
				_tdb_sync_obj_register((caddr_t)mp,
				    MUTEX_MAGIC);
			/* look at (real) onproc state of owner */
			owner_t = _lock_owner(mp);
			if (spin_count < MUTEX_MAX_SPIN && owner_t != NULL) {
				if (owner_t->t_state == TS_ONPROC &&
				    owner_t->t_lwpdata != NULL) {
					state = owner_t->t_lwpdata->sc_state;
					if (state == SC_ONPROC) {
						/*
						 * thread and lwp are ONPROC,
						 * so spin for a while
						 */
						_sched_unlock();
						continue;
					}
				}
			}
			/* may be a long wait, better block */
			waiters = mp->mutex_waiters;
			mp->mutex_waiters = 1;
			if (!_lock_try_adaptive(mp)) {
				curthread->t_flag &= ~T_WAITCV;
				_t_block((caddr_t)mp);
				_sched_unlock_nosig();
				_swtch(0);
				_sigon();
				spin_count = 0;
				continue;
			}
			mp->mutex_waiters = waiters;	/* got the lock */
			_sched_unlock();
			return;
		}
	}
}

static void
_mutex_adaptive_wakeup(mutex_t *mp)
{
	u_char waiters;

	_sched_lock();
	if (_t_release((caddr_t)mp, &waiters, 0) > 0)
		mp->mutex_waiters = waiters;
	_sched_unlock();
}

static void
_mutex_lwp_lock(mutex_t *mp)
{
	mp->mutex_magic = MUTEX_MAGIC;
	if (__tdb_attach_stat != TDB_NOT_ATTACHED)
		_tdb_sync_obj_register((caddr_t)mp, MUTEX_MAGIC);
	_lwp_mutex_lock(mp);

	/* for libthread_db's benefit */
	*(uthread_t **)&mp->mutex_owner = curthread;
}

int
_mutex_wakeup(mutex_t *mp)
{
	if (mp->mutex_type == USYNC_THREAD)
		_mutex_adaptive_wakeup(mp);
	else
		return (___lwp_mutex_wakeup(mp));
	return (0);
}

/*
 * this is the _mutex_lock() routine that is used internally
 * by the thread's library.
 */
void
_lmutex_lock(mutex_t *mp)
{
	_sigoff();
	_mutex_lock(mp);
}

/*
 * this is the _mutex_trylock() that is used internally by the
 * thread's library.
 */
int
_lmutex_trylock(mutex_t *mp)
{
	_sigoff();
	if (_lock_try_adaptive(mp)) {
		_sigon();
		return (1);
	}
	return (0);
}

/*
 * this is the _mutex_unlock() that is used internally by the
 * thread's library.
 */
void
_lmutex_unlock(mutex_t *mp)
{
	_mutex_unlock(mp);
	_sigon();
}

/*
 * Remove the temporary binding. Mimic what occurs in
 * _thread_setschedparam_main() for a FIFO/RR unbound thread.
 */
void
_pthread_temp_rt_unbind()
{
	uthread_t *t = curthread;
	int policy;
	int mappedprio = 0;
	int prio;
	int mappedprioflag = 0;
	int ix;

	_lock_bucket((ix = HASH_TID(t->t_tid)));
	policy = t->t_policy;
	if (policy == SCHED_FIFO || policy == SCHED_RR) {
		prio = t->t_pri;
		if (CHECK_PRIO(prio)) {
			if (!_validate_rt_prio(policy, prio)) {
				mappedprio = prio;
				prio = _map_rtpri_to_gp(prio);
				ASSERT(!CHECK_PRIO(prio));
				mappedprioflag = 1;
			} else {
				_unlock_bucket(ix);
				_panic("rt_unbind: _validate_rt_prio() fails!");
			}
		}
		if (mappedprioflag)
			t->t_mappedpri = mappedprio;
		else if (t->t_mappedpri != 0)
			t->t_mappedpri = 0;
		/*
		 * For now, the following assignment is OK, although it could
		 * potentially create a user-level prio inversion, since the
		 * following assignment could be resulting in a lowering
		 * of the unbound thread's prio, which should be checked
		 * to see if curthread needs to preempt itself.
		 * This does not seem important to address, since most
		 * applications will never go through this code path...having
		 * created bound threads as a condition of using PI locks.
		 */
		t->t_pri = prio;
		
		/*
		 * We do not need to check the return value from 
		 * _thrp_setlwpprio() here because _thrp_setlwpprio()
		 * must have succeeded in _pthread_temp_rt_bind()
		 */
		(void) _thrp_setlwpprio(t->t_lwpid, _getscheduler(), t->t_pri);
	}
	t->t_rtflag &= ~T_TEMPRTBOUND;
	_unlock_bucket(ix);
}


/*
 *  Temporary binding of unbound thread to lwp. Mimic what occurs in
 * _thread_setschedparam_main() for a FIFO/RR bound thread.
 */
int
_pthread_temp_rt_bind()
{
	uthread_t *t = curthread;
	int prio, policy, ix;
	int	ret;

	_lock_bucket((ix = HASH_TID(t->t_tid)));
	t->t_rtflag |= T_TEMPRTBOUND;
	policy = t->t_policy;
	if (policy == SCHED_FIFO || policy == SCHED_RR) {
		prio = RPRIO(t);
		ret = _thrp_setlwpprio(t->t_lwpid, policy, prio);
		if (ret == 0) {
			/*
			 * Should t->t_pri be updated with the new prio
			 * and t_mappedpri zeroed, to conform to a "bound"
			 * thread? It really does not matter, since the
			 * unbind will do the right thing...but in the
			 * meantime, we have a bound thread, with a possibly
			 * non-zero t_mappedpri, and a misleading t_pri....
			 * So, do it:
			 */
			    t->t_pri = prio;
			    t->t_mappedpri = 0;
		} else if (ret == EPERM) {
			t->t_rtflag &= ~T_TEMPRTBOUND;
			_unlock_bucket(ix);
			return (ret);
		} else {
			t->t_rtflag &= ~T_TEMPRTBOUND;
			_unlock_bucket(ix);
			/*
			 * We should only be here if the priocntl(2)
			 * inside _thrp_setlwpprio() failed for some
			 * reason other than EPERM. 
			 */
			_panic("_thrp_setlwpprio failed");
		}
	}
	_unlock_bucket(ix);
	return (0);
}

static int
_thr_upimutex_unlock(mutex_t *mp)
{
	uthread_t *t = curthread;
	int rc;

	rc = ___lwp_mutex_unlock(mp);
	if (ISTEMPRTBOUND(curthread) && --t->t_npinest == 0) {
		_pthread_temp_rt_unbind();
	}
	return (rc);
}

/*
 * Simulate deadlock at user-level.
 */
static void
_stall(void)
{
	/*
	 * Stall by trying to get the thread's t_lock, which is already held.
	 * So, following call should block in the kernel forever.
	 */
	_lwp_mutex_lock(&curthread->t_lock);
}

/*
 * Acquire a UPI mutex. If caller is an unbound thread, it is temporarily
 * bound to the lwp, while this mutex is held. If the robust bit is set
 * for this mutex, return EOWNERDEAD or ENOTRECOVERABLE. A user-level stall
 * is necessary in deadlock cases, otherwise the lwp could loop in kernel.
 */
static int
_thr_upimutex_lock(mutex_t *mp, int try)
{
	int rc;

	if (!ISBOUND(curthread) && curthread->t_npinest++ == 0) {
		/*
		 * Convert an unbound thread to a temporarily bound thread on
		 * first level of nesting for PI locks. Unbind at mutex unlock
		 * time, when nesting level is zero.
		 */
		if ((rc = _pthread_temp_rt_bind()) != 0) {
			curthread->t_npinest--;
			return (rc);
		}
	}
	if (try == RTMUTEX_TRY) {
		rc = ___lwp_mutex_trylock(mp);
	} else {
		rc = ___lwp_mutex_lock(mp);
	}
	if (rc) {
		ASSERT(rc == EOWNERDEAD || rc == ENOTRECOVERABLE ||
		    rc == EDEADLK || rc == EFAULT);
		if (rc == EOWNERDEAD || rc == ENOTRECOVERABLE) {
			if (mp->mutex_type & PTHREAD_MUTEX_ROBUST_NP) {
				return (rc);
			} else if (rc == EOWNERDEAD) {
				/*
				 * Free kmem by unlocking lock. Now lock is
				 * NOTRECOVERABLE, and all waiters are woken up.
				 * They will all stall below.
				 */
				_thr_upimutex_unlock(mp);
			}
		}
		if (try == RTMUTEX_BLOCK) {
			_stall();
		} else {
			rc = EBUSY;
		}
	}
	return (rc);
}

/*
 * Delete mp from list of ceil mutexes owned by curthread.
 * Return 1 if the head of the chain was updated.
 */
static int
_ceil_mylist_del(mutex_t *mp)
{
	uthread_t *t = curthread;
	mxchain_t  **prev;
	mxchain_t *mxc;

	/*
	 * Nested locks are typically LIFO, and not too deeply nested.
	 * The following should not be expensive in the general case.
	 */
	prev = &t->t_mxchain;
	while ((*prev)->mxchain_mx != mp) {
		prev = &(*prev)->mxchain_next;
	}
	mxc = *prev;
	*prev = mxc->mxchain_next;
	free(mxc);
	if (prev == &t->t_mxchain)
		return (1);
	else
		return (0);
}

/*
 * Add mp to head of list of ceil mutexes owned by curthread.
 * Return ENOMEM if no memory could be allocated.
 */
static int
_ceil_mylist_add(mutex_t *mp)
{
	uthread_t *t = curthread;
	mxchain_t *mxc;

	/*
	 * If malloc() causes a scalability problem, use a thread specific
	 * allocator.
	 */
	mxc = (mxchain_t *) malloc(sizeof (mxchain_t));
	if (mxc == NULL)
		return (ENOMEM);
	mxc->mxchain_mx = mp;
	mxc->mxchain_next = t->t_mxchain;
	t->t_mxchain = mxc; /* add to head of list */
	return (0);
}

/*
 * Inherit priority from ceiling. The inheritance impacts the effective
 * priority, not the assigned priority. See _thread_setschedparam_main().
 */
static void
_ceil_prio_inherit(int ceil)
{
	uthread_t *t = curthread;
	struct sched_param param;

	memset(&param, 0, sizeof (param));
	param.sched_priority = ceil;
	if (_thread_setschedparam_main((pthread_t)t->t_tid,
	    t->t_policy, &param, PRIO_INHERIT)) {
		/*
		 * Panic since unclear what error code to return.
		 * If we do return the error codes returned by above
		 * called routine, update the man page...
		 */
		_panic("_thread_setschedparam_main() fails");
	}

}

/*
 * Waive inherited ceiling priority. Inherit from head of owned ceiling locks
 * if holding at least one ceiling lock. If no ceiling locks are held at this
 * point, disinherit completely, reverting back to assigned priority.
 */
static void
_ceil_prio_waive()
{
	uthread_t *t = curthread;
	struct sched_param param;

	memset(&param, 0, sizeof (param));
	if (t->t_mxchain == NULL) {
		/*
		 * No ceil locks held. Zero the epri, revert back to t_pri.
		 * Since thread's bucket lock is not held, one cannot just
		 * read t_pri here...do it in the called routine...
		 */
		param.sched_priority = t->t_pri; /* ignored */
		if (_thread_setschedparam_main((pthread_t)t->t_tid,
		    t->t_policy, &param, PRIO_DISINHERIT)) {
			_panic("_thread_setschedparam_main() fails");
		}
	} else {
		/*
		 * set prio to that of the thread at the head
		 * of the ceilmutex chain.
		 */
		param.sched_priority =
		    t->t_mxchain->mxchain_mx->mutex_ceiling;
		if (_thread_setschedparam_main((pthread_t)t->t_tid,
		    t->t_policy, &param, PRIO_INHERIT)) {
			_panic("_thread_setschedparam_main() fails");
		}
	}

}

/*
 * Acquire a ceiling mutex. If caller is an unbound thread, it is temporarily
 * bound to the lwp, while this mutex is held.
 */
static int
_thr_ceilmutex_lock(mutex_t *mp, int try)
{
	uint8_t ceil;
	int rc;
	uthread_t *t = curthread;
	int myprio;
	struct sched_param param;

	ceil = mp->mutex_ceiling;
	ASSERT(_validate_rt_prio(SCHED_FIFO, ceil) == 0);
	myprio = DISP_RPRIO(t);
	if (myprio > ceil)
		return (EINVAL);
	rc = _ceil_mylist_add(mp);
	if (rc) {
		return (rc);
	}
	/*
	 * Temp binding for ceiling locks may not be needed, in which case
	 * t_nceilnest would also not be necessary.
	 */
	if (!ISBOUND(t) && t->t_nceilnest++ == 0) {
		if ((rc = _pthread_temp_rt_bind()) != 0) {
			t->t_nceilnest--;
			(void) _ceil_mylist_del(mp);
			return (rc);
		}
	}
	if (myprio < ceil) {
		_ceil_prio_inherit(ceil);
	}
	if (try == RTMUTEX_TRY) {
		/*
		 * It is not necessary to call _lwp_mutex* since curthread
		 * just needs to make a user-level attempt at getting the lock.
		 */
		rc = _mutex_trylock(mp);
	} else {
		/*
		 * Do not call _mutex_lock(). If mp is a USYNC_THREAD lock,
		 * _mutex_lock() could spin adaptively. Hence, curthread could
		 * spin at the ceiling priority, enabling a low priority thread
		 * to boost itself and successfully waste CPU cycles.
		 */
		rc = _lwp_mutex_lock(mp);
	}
	if (rc) {
		/*
		 * Failed to get lock. Waive any inherited priority,
		 * decrement nceilnest, and call unbind if necessary.
		 */
		(void) _ceil_mylist_del(mp);
		if (myprio < ceil) {
			_ceil_prio_waive();
		}
		if (ISTEMPRTBOUND(t) && --t->t_nceilnest == 0) {
			_pthread_temp_rt_unbind();
		}
	}
	return (rc);
}

/*
 * Release a ceiling mutex, waive any inherited priority, and unbind if no
 * ceiling locks are held after unlock.
 */
static int
_thr_ceilmutex_unlock(mutex_t *mp)
{
	uthread_t *t = curthread;
	int newprio, rc, chpri;
	struct sched_param param;

	rc = _lwp_mutex_unlock(mp);
	chpri = _ceil_mylist_del(mp);
	if (chpri) {
		_ceil_prio_waive();
	}
	if (ISTEMPRTBOUND(t) && --t->t_nceilnest == 0) {
		_pthread_temp_rt_unbind();
	}
	return (rc);
}

int
_pthread_mutex_lock(pthread_mutex_t *pm)
{
	mutex_t *mp = (mutex_t *)pm;

	if (ISRTMUTEX(mp->mutex_type)) {
		if (mp->mutex_type & PTHREAD_PRIO_INHERIT)
			return (_thr_upimutex_lock(mp, RTMUTEX_BLOCK));
		else
			return (_thr_ceilmutex_lock(mp, RTMUTEX_BLOCK));
	}
	if (!_lock_try_adaptive(mp)) {
		if (_lock_owner(mp) == curthread) {
			if (mp->mutex_type & PTHREAD_MUTEX_RECURSIVE) {
				if (mp->mutex_rcount == RECURSION_MAX)
					return (EAGAIN);
				mp->mutex_rcount++;
				return (0);
			} else if (mp->mutex_type & PTHREAD_MUTEX_ERRORCHECK) {
				return (EDEADLK);
			}
		}
		if (mp->mutex_type & USYNC_PROCESS)
			_mutex_lwp_lock(mp);
		else
			_mutex_adaptive_lock(mp);
	}
	/* first acquistion of a recursive lock */
	if (mp->mutex_type & PTHREAD_MUTEX_RECURSIVE)
		mp->mutex_rcount++;
	return (0);
}

int
_pthread_mutex_unlock(pthread_mutex_t *pm)
{
	mutex_t *mp = (mutex_t *)pm;

	if (ISRTMUTEX(mp->mutex_type)) {
		if (mp->mutex_type & PTHREAD_PRIO_INHERIT)
			return (_thr_upimutex_unlock(mp));
		else
			return (_thr_ceilmutex_unlock(mp));
	}
	if (mp->mutex_type & PTHREAD_MUTEX_RECURSIVE) {
		if (_lock_owner(mp) == curthread) {
			if (--mp->mutex_rcount)
				return (0);
		} else {
				return (EPERM);
		}
	} else if (mp->mutex_type & PTHREAD_MUTEX_ERRORCHECK) {
		if (_lock_owner(mp) != curthread)
			return (EPERM);
	}
	if (_mutex_unlock_asm(mp) > 0) {
		return (_mutex_wakeup(mp));
	}
	return (0);
}

int
_pthread_mutex_trylock(pthread_mutex_t *pm)
{
	mutex_t *mp = (mutex_t *)pm;

	if (ISRTMUTEX(mp->mutex_type)) {
		if (mp->mutex_type & PTHREAD_PRIO_INHERIT)
			return (_thr_upimutex_lock(mp, RTMUTEX_TRY));
		else
			return (_thr_ceilmutex_lock(mp, RTMUTEX_TRY));
	}
	if (_lock_try_adaptive(mp)) {
		if (mp->mutex_type & PTHREAD_MUTEX_RECURSIVE)
			mp->mutex_rcount++;
		return (0);
	} else {
		if (_lock_owner(mp) == curthread) {
			if (mp->mutex_type & PTHREAD_MUTEX_RECURSIVE) {
				if (mp->mutex_rcount == RECURSION_MAX)
					return (EAGAIN);
				mp->mutex_rcount++;
				return (0);
			} else if (mp->mutex_type & PTHREAD_MUTEX_ERRORCHECK) {
				return (EDEADLK);
			}
		}
		if (__td_event_report(curthread, TD_LOCK_TRY)) {
			curthread->t_td_evbuf.eventnum = TD_LOCK_TRY;
			tdb_event_lock_try();
		}
		return (EBUSY);
	}
}

int
_mutex_lock_robust(mutex_t *mp)
{
	mp->mutex_magic = MUTEX_MAGIC;
	if (__tdb_attach_stat != TDB_NOT_ATTACHED)
		_tdb_sync_obj_register((caddr_t)mp, MUTEX_MAGIC);
	return (___lwp_mutex_lock(mp));
}

int
_mutex_unlock_robust(mutex_t *mp)
{
	return (___lwp_mutex_unlock(mp));
}

int
_mutex_trylock_robust(mutex_t *mp)
{
	int rc;

	rc = ___lwp_mutex_trylock(mp);
	if (!rc && __td_event_report(curthread, TD_LOCK_TRY)) {
		curthread->t_td_evbuf.eventnum = TD_LOCK_TRY;
		tdb_event_lock_try();
	}
	return (rc);
}

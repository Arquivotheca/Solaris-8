/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)synch.c	1.3	99/11/02 SMI"

#include "liblwp.h"

/*
 * Non-weak libc interfaces.
 */
extern	int	__lwp_mutex_trylock(mutex_t *);
extern	int	___lwp_mutex_trylock(mutex_t *);
extern	int	__lwp_mutex_lock(mutex_t *);
extern	int	___lwp_mutex_lock(mutex_t *);
extern	int	___lwp_mutex_unlock(mutex_t *);
extern	int	_lock_try(uint8_t *);
extern	void	_lock_clear(uint8_t *);

/*
 * This mutex is initialized to be held by lwp#1.
 * It is used to block a thread that has returned from a mutex_lock()
 * of a PTHREAD_PRIO_INHERIT mutex with an unrecoverable error.
 */
mutex_t	stall_mutex = DEFAULTMUTEX;

/*
 * Called once at library initialization.
 */
void
mutex_setup()
{
	if (__lwp_mutex_trylock(&stall_mutex) != 0)
		panic("mutex_setup() cannot acquire stall_mutex");
	set_mutex_owner(&stall_mutex);
}

/*
 * The default adaptive spin count of 1000 is experimentally determined.
 * On sun4u machines with any number of processors it could be raised
 * to 10,000 but that (experimentally) makes almost no difference.
 * The environment variable MUTEX_ADAPTIVE_SPIN=count can be used
 * to override and set the count in the range [0 .. 1,000,000].
 * If there is only one on-line cpu, the actual spin count is set to zero.
 */
int	mutex_adaptive_spin = 1000;
int	ncpus = 1;

void
set_mutex_owner(mutex_t *mp)
{
	mp->mutex_owner = (uint64_t)curthread;
	if (mp->mutex_type & (USYNC_PROCESS|USYNC_PROCESS_ROBUST)) {
		/* LINTED pointer cast may result in improper alignment */
		*(int32_t *)&mp->mutex_ownerpid = _lpid;
	}
}

void
_mutex_set_typeattr(mutex_t *mp, int attr)
{
	mp->mutex_type |= (uint8_t)attr;
}

#pragma weak mutex_init = _mutex_init
#pragma weak _liblwp_mutex_init = _mutex_init
/* ARGSUSED2 */
int
_mutex_init(mutex_t *mp, int type, void *arg)
{
	extern int ___lwp_mutex_init(mutex_t *, int);
	int error = 0;

	switch (type) {
	case USYNC_THREAD:
	case USYNC_PROCESS:
		(void) _memset(mp, 0, sizeof (*mp));
		mp->mutex_type = (uint8_t)type;
		mp->mutex_flag = LOCK_INITED;
		break;
	case USYNC_PROCESS_ROBUST:
		error = ___lwp_mutex_init(mp, type);
		break;
	default:
		error = EINVAL;
		break;
	}
	if (error == 0)
		(void) MUTEX_STATS(mp);
	return (error);
}

/*
 * Delete mp from list of ceil mutexes owned by curthread.
 * Return 1 if the head of the chain was updated.
 */
static int
_ceil_mylist_del(mutex_t *mp)
{
	ulwp_t *self = curthread;
	mxchain_t **mcpp;
	mxchain_t *mcp;

	mcpp = &self->ul_mxchain;
	while ((*mcpp)->mxchain_mx != mp)
		mcpp = &(*mcpp)->mxchain_next;
	mcp = *mcpp;
	*mcpp = mcp->mxchain_next;
	liblwp_free(mcp);
	return (mcpp == &self->ul_mxchain);
}

/*
 * Add mp to head of list of ceil mutexes owned by curthread.
 * Return ENOMEM if no memory could be allocated.
 */
static int
_ceil_mylist_add(mutex_t *mp)
{
	ulwp_t *self = curthread;
	mxchain_t *mcp;

	if ((mcp = malloc(sizeof (*mcp))) == NULL)
		return (ENOMEM);
	mcp->mxchain_mx = mp;
	mcp->mxchain_next = self->ul_mxchain;
	self->ul_mxchain = mcp;
	return (0);
}

/*
 * Inherit priority from ceiling.  The inheritance impacts the effective
 * priority, not the assigned priority.  See _thread_setschedparam_main().
 */
static void
_ceil_prio_inherit(int ceil)
{
	ulwp_t *self = curthread;
	struct sched_param param;

	(void) _memset(&param, 0, sizeof (param));
	param.sched_priority = ceil;
	if (_thread_setschedparam_main(self->ul_lwpid,
	    self->ul_policy, &param, PRIO_INHERIT)) {
		/*
		 * Panic since unclear what error code to return.
		 * If we do return the error codes returned by above
		 * called routine, update the man page...
		 */
		panic("_thread_setschedparam_main() fails");
	}
}

/*
 * Waive inherited ceiling priority.  Inherit from head of owned ceiling locks
 * if holding at least one ceiling lock.  If no ceiling locks are held at this
 * point, disinherit completely, reverting back to assigned priority.
 */
static void
_ceil_prio_waive()
{
	ulwp_t *self = curthread;
	struct sched_param param;

	(void) _memset(&param, 0, sizeof (param));
	if (self->ul_mxchain == NULL) {
		/*
		 * No ceil locks held.  Zero the epri, revert back to ul_pri.
		 * Since thread's hash lock is not held, one cannot just
		 * read ul_pri here...do it in the called routine...
		 */
		param.sched_priority = self->ul_pri;	/* ignored */
		if (_thread_setschedparam_main(self->ul_lwpid,
		    self->ul_policy, &param, PRIO_DISINHERIT))
			panic("_thread_setschedparam_main() fails");
	} else {
		/*
		 * Set priority to that of the mutex at the head
		 * of the ceilmutex chain.
		 */
		param.sched_priority =
		    self->ul_mxchain->mxchain_mx->mutex_ceiling;
		if (_thread_setschedparam_main(self->ul_lwpid,
		    self->ul_policy, &param, PRIO_INHERIT))
			panic("_thread_setschedparam_main() fails");
	}

}

/*
 * Common code for calling the ___lwp_mutex_lock() system call.
 */
static int
mutex_lock_kernel(ulwp_t *self, tdb_mutex_stats_t *msp, mutex_t *mp)
{
	hrtime_t begin_sleep;
	int error;

	_save_nv_regs(&self->ul_savedregs);
	self->ul_validregs = 1;
	self->ul_wchan = (uintptr_t)mp;
	if (__td_event_report(self, TD_SLEEP)) {
		self->ul_td_evbuf.eventnum = TD_SLEEP;
		self->ul_td_evbuf.eventdata = mp;
		tdb_event_sleep();
	}
	if (msp) {
		tdb_incr(msp->mutex_sleep);
		begin_sleep = gethrtime();
	}
	error = ___lwp_mutex_lock(mp);
	if (msp)
		msp->mutex_sleep_time += gethrtime() - begin_sleep;
	self->ul_wchan = 0;
	self->ul_validregs = 0;

	return (error);
}

static void
no_preempt()
{
	extern int __lwp_schedctl(uint_t, int, sc_shared_t **);

	enter_critical();
	if (ncpus > 1) {
		ulwp_t *self = curthread;
		sc_shared_t *schedctl = self->ul_schedctl;

		if (schedctl != NULL)
			schedctl->sc_preemptctl.sc_nopreempt = 1;
		else if (!self->ul_schedctl_called) {
			self->ul_schedctl_called = 1;
			if (__lwp_schedctl(SC_PREEMPT, 0, &schedctl) == 0) {
				self->ul_schedctl = schedctl;
				schedctl->sc_preemptctl.sc_nopreempt = 1;
			}
		}
	}
}

static void
preempt()
{
	ulwp_t *self = curthread;
	sc_shared_t *schedctl;

	if ((schedctl = self->ul_schedctl) != NULL) {
		schedctl->sc_preemptctl.sc_nopreempt = 0;
		if (schedctl->sc_preemptctl.sc_yield) {
			schedctl->sc_preemptctl.sc_yield = 0;
			_yield();
		}
	}
	exit_critical();
}

/*
 * Spin for a while, testing to see if the lock has been dropped.
 * If this fails, return EBUSY and let the caller deal with it.
 */
static int
mutex_trylock_adaptive(mutex_t *mp)
{
	/* LINTED pointer cast may result in improper alignment */
	volatile int32_t *pidp = (volatile int32_t *)&mp->mutex_ownerpid;
	volatile uint64_t *ownerp = (volatile uint64_t *)&mp->mutex_owner;
	volatile uint8_t *lockp = (volatile uint8_t *)&mp->mutex_lockw;
	pid_t pid = *pidp;
	pid_t newpid;
	uint64_t owner = *ownerp;
	uint64_t newowner;
	int count;

	/*
	 * This spin loop is unfair to lwps that have already dropped into
	 * the kernel to sleep.  They will starve on a highly-contended mutex.
	 * This is just too bad.  The adaptive spin algorithm is intended
	 * to allow programs with highly-contended locks (that is, broken
	 * programs) to execute with reasonable speed despite their contention.
	 * Being fair would reduce the speed of such programs and well-written
	 * programs will not suffer in any case.
	 */
	if ((count = (ncpus > 1)? mutex_adaptive_spin : 0) == 0)
		return (EBUSY);
	while (--count >= 0) {
		if (*lockp == 0) {
			if (__lwp_mutex_trylock(mp) == 0)
				return (0);
		} else if ((newowner = *ownerp) == owner &&
		    (newpid = *pidp) == pid) {
			continue;
		}
		/*
		 * The owner of the lock changed; start the count over again.
		 * (This may be too aggressive; it needs testing.)
		 */
		owner = newowner;
		pid = newpid;
		count = mutex_adaptive_spin;
	}
	return (EBUSY);
}

/*
 * See <sys/synch32.h> for the reasons for these values
 * and why they are different for sparc and intel.
 */
#if defined(__sparc)
/* lock.lock64.pad[x]	   4 5 6 7 */
#define	LOCKMASK	0xff000000
#define	WAITERMASK	0x000000ff
#define	WAITER		0x00000001
#define	LOCKSET		0xff
#define	LOCKCLEAR	0
#elif defined(__i386) || defined(__ia64)
/* lock.lock64.pad[x]	   7 6 5 4 */
#define	LOCKMASK	0xff000000
#define	WAITERMASK	0x00ff0000
#define	WAITER		0x00010000
#define	LOCKSET		0x01
#define	LOCKCLEAR	0
#else
#error "none of __sparc __i386 __ia64 is defined"
#endif

/*
 * This should be defined in <sys/synch32.h>
 */
#define	mutex_tmplock	lock.lock64.pad[5]

/*
 * Spin for a while, testing to see if the lock has been grabbed.
 * If this fails, call ___lwp_mutex_wakeup() to release a waiter in the kernel.
 */
static int
mutex_unlock_adaptive(mutex_t *mp)
{
	extern uint32_t swap32(uint32_t *target, uint32_t new);
	extern int ___lwp_mutex_wakeup(mutex_t *);

	/* LINTED pointer cast may result in improper alignment */
	uint32_t *lockw = (uint32_t *)&mp->mutex_lockword;
	uint8_t *tmplockp = (uint8_t *)&mp->mutex_tmplock;
	volatile uint8_t *lockp = (volatile uint8_t *)&mp->mutex_lockw;
	int count;
	int error;

	/*
	 * We use the swap primitive to clear the lock, but we must
	 * atomically retain the waiters bit for the remainder of this
	 * code to work.  We first check to see if the waiters bit is
	 * set and if so clear the lock by swapping in a word containing
	 * only the waiters bit.  This could produce a false positive
	 * test for whether there are waiters that need to be waked up,
	 * but this just causes an extra trip to the kernel to do nothing.
	 * The opposite case is more delicate; if there are no waiters,
	 * we swap in a zero lock byte and a zero waiters bit.  The result
	 * of the swap could indicate that there really was a waiter so in
	 * this case we go directly to the kernel without performing any
	 * of the adaptive code because the waiter bit has been cleared
	 * and the adaptive code is unreliable in this case.
	 */
	if (*lockw & WAITERMASK)	/* some waiter exists right now */
		(void) swap32(lockw, WAITER);	/* clear lock, retain waiter */
	else if (swap32(lockw, 0) & WAITERMASK)	/* there was a waiter */
		return (___lwp_mutex_wakeup(mp));	/* go to the kernel */
	else
		return (0);		/* no waiter, nothing to do */

	/*
	 * We spin here for half the time that mutex_trylock_adaptive() spins.
	 * We are trying to balance two conflicting goals:
	 * 1. Avoid waking up anyone if a spinning thread grabs the lock.
	 * 2. Wake up a sleeping thread promptly to get on with useful work.
	 */
	if ((count = (ncpus > 1)? mutex_adaptive_spin / 2 : 0) == 0)
		return (___lwp_mutex_wakeup(mp));

	/*
	 * If we can't get the tmplock, someone else is in here doing the
	 * job for us, so we can safely give up.  Note that the swap32()
	 * clears the tmplock, but that just allows more than one thread
	 * to pass this point which results at most in an extra trip to
	 * the kernel to do nothing, so it is safe.
	 */
	if (_lock_try(tmplockp) == 0)
		return (0);

	/*
	 * There is a waiter that we will have to wake up unless someone
	 * else grabs the lock while we are busy spinning.  Like the spin
	 * loop in mutex_trylock_adaptive(), this spin loop is unfair to
	 * lwps that have already dropped into the kernel to sleep.
	 * They will starve on a highly-contended mutex.  Too bad.
	 */
	while (--count >= 0) {
		if (*lockp != 0) {	/* somebody else grabbed the lock */
			_lock_clear(tmplockp);
			return (0);
		}
	}

	/*
	 * No one else grabbed the lock.
	 * Wake up some lwp in the kernel that is waiting for it.
	 */
	_lock_clear(tmplockp);
	error = ___lwp_mutex_wakeup(mp);
	return (error);
}

/*
 * Return the real priority of a thread.
 */
static int
real_priority(ulwp_t *self)
{
	if (self->ul_epri == 0)
		return (self->ul_mappedpri? self->ul_mappedpri : self->ul_pri);
	return (self->ul_emappedpri? self->ul_emappedpri : self->ul_epri);
}

static void
stall()
{
	for (;;)
		(void) mutex_lock_kernel(curthread, NULL, &stall_mutex);
}

int
mutex_lock_internal(mutex_t *mp, rwlock_t *rwlp, int try)
{
	ulwp_t *self = curthread;
	int mtype = mp->mutex_type;
	tdb_mutex_stats_t *msp = MUTEX_STATS(mp);
	int error;

	ASSERT(try == MUTEX_TRY || try == MUTEX_LOCK);

	if (msp) {
		if (rwlp)
			msp->offset = (caddr_t)mp - (caddr_t)rwlp;
		if (try == MUTEX_TRY)
			tdb_incr(msp->mutex_try);
	}

	if ((mtype & (PTHREAD_MUTEX_RECURSIVE|PTHREAD_MUTEX_ERRORCHECK)) &&
	    _mutex_held(mp)) {
		if (mtype & PTHREAD_MUTEX_RECURSIVE) {
			ASSERT(mp->mutex_rcount != 0);
			if (mp->mutex_rcount == RECURSION_MAX)
				error = EAGAIN;
			else
				error = 0;
		} else {
			error = EDEADLK;
		}
	} else if (mtype &
	    (USYNC_PROCESS_ROBUST|PTHREAD_PRIO_INHERIT|PTHREAD_PRIO_PROTECT)) {
		uint8_t ceil;
		int myprio;

		if (mtype & PTHREAD_PRIO_PROTECT) {
			ceil = mp->mutex_ceiling;
			ASSERT(_validate_rt_prio(SCHED_FIFO, ceil) == 0);
			myprio = real_priority(self);
			if (myprio > ceil)
				return (EINVAL);
			if ((error = _ceil_mylist_add(mp)) != 0)
				return (error);
			if (myprio < ceil)
				_ceil_prio_inherit(ceil);
		}

		if (mtype & PTHREAD_PRIO_INHERIT) {
			/* go straight to the kernel */
			if (try == MUTEX_TRY)
				error = ___lwp_mutex_trylock(mp);
			else	/* MUTEX_LOCK */
				error = mutex_lock_kernel(self, msp, mp);
			/*
			 * The kernel never sets or clears the lock byte
			 * for PTHREAD_PRIO_INHERIT mutexes.
			 * Set it here for debugging consistency.
			 */
			switch (error) {
			case 0:
			case EOWNERDEAD:
				mp->mutex_lockw = LOCKSET;
				break;
			}
		} else if (mtype & USYNC_PROCESS_ROBUST) {
			/* go straight to the kernel */
			if (try == MUTEX_TRY)
				error = ___lwp_mutex_trylock(mp);
			else	/* MUTEX_LOCK */
				error = mutex_lock_kernel(self, msp, mp);
		} else {	/* PTHREAD_PRIO_PROTECT */
			/* try once at user level */
			error = __lwp_mutex_trylock(mp);
			if (error && try == MUTEX_LOCK) {
				/* then go to the kernel */
				error = mutex_lock_kernel(self, msp, mp);
			}
		}

		if (error) {
			if (mtype & PTHREAD_PRIO_INHERIT) {
				switch (error) {
				case EOWNERDEAD:
				case ENOTRECOVERABLE:
					if (mtype & PTHREAD_MUTEX_ROBUST_NP)
						break;
					if (error == EOWNERDEAD) {
						/*
						 * We own the mutex; unlock it.
						 * It becomes ENOTRECOVERABLE.
						 * All waiters are waked up.
						 */
						mp->mutex_lockw = LOCKCLEAR;
						(void) ___lwp_mutex_unlock(mp);
					}
					/* FALLTHROUGH */
				case EDEADLK:
					if (try == MUTEX_LOCK)
						stall();
					error = EBUSY;
					break;
				}
			}
			if ((mtype & PTHREAD_PRIO_PROTECT) &&
			    error != EOWNERDEAD) {
				(void) _ceil_mylist_del(mp);
				if (myprio < ceil)
					_ceil_prio_waive();
			}
		}
	} else {
		/* try once at user level */
		if ((error = __lwp_mutex_trylock(mp)) != 0) {
			/* try a little harder */
			error = mutex_trylock_adaptive(mp);
			if (error && try == MUTEX_LOCK) {
				/* then go to the kernel */
				error = mutex_lock_kernel(self, msp, mp);
			}
		}
	}

	switch (error) {
	case 0:
	case EOWNERDEAD:
	case ELOCKUNMAPPED:
		/* count only the first acquisition of a recursive lock */
		if (!(mtype & PTHREAD_MUTEX_RECURSIVE) ||
		    mp->mutex_rcount++ == 0) {
			if (msp) {
				tdb_incr(msp->mutex_lock);
				msp->mutex_begin_hold = gethrtime();
			}
			self->ul_mutex++;
			set_mutex_owner(mp);
		}
		ASSERT(_lock_held(mp));
		break;
	default:
		if (try == MUTEX_TRY) {
			if (msp)
				tdb_incr(msp->mutex_try_fail);
			if (__td_event_report(self, TD_LOCK_TRY)) {
				self->ul_td_evbuf.eventnum = TD_LOCK_TRY;
				tdb_event_lock_try();
			}
		}
		break;
	}

	return (error);
}

#pragma weak pthread_mutex_lock = _mutex_lock
#pragma weak _pthread_mutex_lock = _mutex_lock
#pragma weak _liblwp_pthread_mutex_lock = _mutex_lock
#pragma weak mutex_lock = _mutex_lock
#pragma weak _liblwp_mutex_lock = _mutex_lock
int
_mutex_lock(mutex_t *mp)
{
	return (mutex_lock_internal(mp, NULL, MUTEX_LOCK));
}

#pragma weak pthread_mutex_trylock = _mutex_trylock
#pragma weak _pthread_mutex_trylock = _mutex_trylock
#pragma weak _liblwp_pthread_mutex_trylock = _mutex_trylock
#pragma weak mutex_trylock = _mutex_trylock
#pragma weak _liblwp_mutex_trylock = _mutex_trylock
int
_mutex_trylock(mutex_t *mp)
{
	return (mutex_lock_internal(mp, NULL, MUTEX_TRY));
}

#pragma weak pthread_mutex_unlock = _mutex_unlock
#pragma weak _pthread_mutex_unlock = _mutex_unlock
#pragma weak _liblwp_pthread_mutex_unlock = _mutex_unlock
#pragma weak mutex_unlock = _mutex_unlock
#pragma weak _liblwp_mutex_unlock = _mutex_unlock
int
_mutex_unlock(mutex_t *mp)
{
	ulwp_t *self = curthread;
	int mtype = mp->mutex_type;
	tdb_mutex_stats_t *msp;
	int error;

	if ((mtype & (PTHREAD_MUTEX_ERRORCHECK|PTHREAD_MUTEX_RECURSIVE)) &&
	    !_mutex_held(mp))
		return (EPERM);

	ASSERT(_mutex_held(mp));

	if ((mtype & PTHREAD_MUTEX_RECURSIVE) && --mp->mutex_rcount != 0)
		return (0);

	if ((msp = MUTEX_STATS(mp)) != NULL) {
		if (msp->mutex_begin_hold)
			msp->mutex_hold_time +=
				gethrtime() - msp->mutex_begin_hold;
		msp->mutex_begin_hold = 0;
	}

	no_preempt();	/* block signals and preemption while we do this */
	mp->mutex_owner = 0;
	if (mtype &
	    (USYNC_PROCESS_ROBUST|PTHREAD_PRIO_INHERIT|PTHREAD_PRIO_PROTECT)) {
		if (mtype & PTHREAD_PRIO_INHERIT) {
			mp->mutex_lockw = LOCKCLEAR;
			error = ___lwp_mutex_unlock(mp);
		} else if (mtype & USYNC_PROCESS_ROBUST) {
			error = ___lwp_mutex_unlock(mp);
		} else {
			error = mutex_unlock_adaptive(mp);
		}
		if (mtype & PTHREAD_PRIO_PROTECT) {
			if (_ceil_mylist_del(mp))
				_ceil_prio_waive();
		}
	} else {
		error = mutex_unlock_adaptive(mp);
	}
	self->ul_mutex--;
	preempt();

	return (error);
}

uchar_t
_lock_held(mutex_t *mp)
{
	return (mp->mutex_lockw);
}

#pragma weak mutex_held = _mutex_held
#pragma weak _liblwp_mutex_held = _mutex_held
int
_mutex_held(mutex_t *mp)
{
	return (mp->mutex_lockw != 0 &&
	    mp->mutex_owner == (uint64_t)curthread &&
	    (!(mp->mutex_type & (USYNC_PROCESS|USYNC_PROCESS_ROBUST)) ||
	    /* LINTED pointer cast may result in improper alignment */
	    *(int32_t *)&mp->mutex_ownerpid == _lpid));
}

#pragma weak pthread_mutex_destroy = _mutex_destroy
#pragma weak _pthread_mutex_destroy = _mutex_destroy
#pragma weak _liblwp_pthread_mutex_destroy = _mutex_destroy
#pragma weak mutex_destroy = _mutex_destroy
#pragma weak _liblwp_mutex_destroy = _mutex_destroy
int
_mutex_destroy(mutex_t *mp)
{
	mp->mutex_magic = 0;
	mp->mutex_flag &= ~LOCK_INITED;
	tdb_sync_obj_deregister(mp);
	return (0);
}

#pragma weak cond_init = _cond_init
#pragma weak _liblwp_cond_init = _cond_init
/* ARGSUSED2 */
int
_cond_init(cond_t *cvp, int type, void *arg)
{
	if (type != USYNC_THREAD && type != USYNC_PROCESS)
		return (EINVAL);
	(void) _memset(cvp, 0, sizeof (*cvp));
	cvp->cond_type = (uint16_t)type;
	(void) COND_STATS(cvp);
	return (0);
}

/*
 * Common code for _cond_wait() and _cond_timedwait()
 */
static int
cond_wait_common(cond_t *cvp, mutex_t *mp, timestruc_t *tsp,
	tdb_cond_stats_t *csp, tdb_mutex_stats_t *msp)
{
	extern int ___lwp_cond_wait(cond_t *, mutex_t *, timestruc_t *);
	int mtype = mp->mutex_type;
	hrtime_t begin_sleep = 0;
	int error;
	ulwp_t *self = curthread;

	_save_nv_regs(&self->ul_savedregs);
	self->ul_validregs = 1;
	self->ul_wchan = (uintptr_t)cvp;
	if (__td_event_report(self, TD_SLEEP)) {
		self->ul_td_evbuf.eventnum = TD_SLEEP;
		self->ul_td_evbuf.eventdata = cvp;
		tdb_event_sleep();
	}
	if (csp)
		begin_sleep = gethrtime();
	if (msp) {
		if (begin_sleep == 0)
			begin_sleep = gethrtime();
		if (msp->mutex_begin_hold)
			msp->mutex_hold_time +=
				begin_sleep - msp->mutex_begin_hold;
		msp->mutex_begin_hold = 0;
	}
	if (mtype & PTHREAD_PRIO_PROTECT) {
		if (_ceil_mylist_del(mp))
			_ceil_prio_waive();
	}
	if (mtype & PTHREAD_PRIO_INHERIT)
		mp->mutex_lockw = LOCKCLEAR;
	mp->mutex_owner = 0;
	self->ul_mutex--;
	error = ___lwp_cond_wait(cvp, mp, tsp);
	self->ul_validregs = 0;
	self->ul_wchan = 0;
	(void) _mutex_lock(mp);
	if (csp) {
		hrtime_t lapse = gethrtime() - begin_sleep;
		if (tsp == NULL)
			csp->cond_wait_sleep_time += lapse;
		else {
			csp->cond_timedwait_sleep_time += lapse;
			if (error == ETIME)
				tdb_incr(csp->cond_timedwait_timeout);
		}
	}
	return (error);
}

int
cond_wait_internal(cond_t *cvp, mutex_t *mp, rwlock_t *rwlp)
{
	tdb_cond_stats_t *csp = COND_STATS(cvp);
	tdb_mutex_stats_t *msp = MUTEX_STATS(mp);

	if (csp) {
		if (rwlp)
			csp->offset = (caddr_t)cvp - (caddr_t)rwlp;
		tdb_incr(csp->cond_wait);
	}
	if (msp && rwlp)
		msp->offset = (caddr_t)mp - (caddr_t)rwlp;

	return (cond_wait_common(cvp, mp, NULL, csp, msp));
}

/*
 * cond_wait() is a cancellation point but _cond_wait() is not.
 * System libraries call the non-cancellation version.
 * It is expected that only applications call the cancellation version.
 */
#pragma weak _liblwp_cond_wait = _cond_wait
int
_cond_wait(cond_t *cvp, mutex_t *mp)
{
	return (cond_wait_internal(cvp, mp, NULL));
}

#pragma weak cond_wait = _cond_wait_cancel
int
_cond_wait_cancel(cond_t *cvp, mutex_t *mp)
{
	int error;

	_cancelon();
	error = _cond_wait(cvp, mp);
	if (error == EINTR)
		_canceloff();
	else
		_canceloff_nocancel();
	return (error);
}

#pragma weak pthread_cond_wait = _pthread_cond_wait
#pragma weak _liblwp_pthread_cond_wait = _pthread_cond_wait
int
_pthread_cond_wait(cond_t *cvp, mutex_t *mp)
{
	int error;

	error = _cond_wait_cancel(cvp, mp);
	return ((error == EINTR)? 0 : error);
}

/*
 * cond_timedwait() is a cancellation point but _cond_timedwait() is not.
 * System libraries call the non-cancellation version.
 * It is expected that only applications call the cancellation version.
 */
#pragma weak _liblwp_cond_timedwait = _cond_timedwait
int
_cond_timedwait(cond_t *cvp, mutex_t *mp, timestruc_t *abstime)
{
	tdb_cond_stats_t *csp = COND_STATS(cvp);
	tdb_mutex_stats_t *msp = MUTEX_STATS(mp);
	struct timeval tv;
	timestruc_t now;
	timestruc_t reltime;

	/*
	 * Convert the absolute timeout to a relative timeout because
	 * the ___lwp_cond_wait() system call expects relative time.
	 * First get the current absolute time as a timestruc_t.
	 */
	(void) _gettimeofday(&tv, NULL);
	now.tv_sec  = tv.tv_sec;
	now.tv_nsec = tv.tv_usec * 1000;

	/* reject bad timeout values (a 3 year timeout is bad?) */
	if ((ulong_t)abstime->tv_nsec >= NANOSEC ||
	    abstime->tv_sec > now.tv_sec + 100000000)
		return (EINVAL);

	if (csp)
		tdb_incr(csp->cond_timedwait);

	if (abstime->tv_nsec >= now.tv_nsec) {
		if (abstime->tv_sec < now.tv_sec) {
			if (csp)
				tdb_incr(csp->cond_timedwait_timeout);
			return (ETIME);
		}
		reltime.tv_sec = abstime->tv_sec - now.tv_sec;
		reltime.tv_nsec = abstime->tv_nsec - now.tv_nsec;
	} else {
		if (abstime->tv_sec <= now.tv_sec) {
			if (csp)
				tdb_incr(csp->cond_timedwait_timeout);
			return (ETIME);
		}
		reltime.tv_sec  = abstime->tv_sec - 1 - now.tv_sec;
		reltime.tv_nsec = abstime->tv_nsec + NANOSEC - now.tv_nsec;
	}

	return (cond_wait_common(cvp, mp, &reltime, csp, msp));
}

#pragma weak cond_timedwait = _cond_timedwait_cancel
int
_cond_timedwait_cancel(cond_t *cvp, mutex_t *mp, timestruc_t *abstime)
{
	int error;

	_cancelon();
	error = _cond_timedwait(cvp, mp, abstime);
	if (error == EINTR)
		_canceloff();
	else
		_canceloff_nocancel();
	return (error);
}

#pragma weak pthread_cond_timedwait = _pthread_cond_timedwait
#pragma weak _liblwp_pthread_cond_timedwait = _pthread_cond_timedwait
int
_pthread_cond_timedwait(cond_t *cvp, mutex_t *mp, timestruc_t *abstime)
{
	int error;

	error = _cond_timedwait_cancel(cvp, mp, abstime);
	if (error == ETIME)
		error = ETIMEDOUT;
	else if (error == EINTR)
		error = 0;
	return (error);
}

int
cond_signal_internal(cond_t *cvp, rwlock_t *rwlp)
{
	extern int __lwp_cond_signal(lwp_cond_t *);
	tdb_cond_stats_t *csp = COND_STATS(cvp);

	if (csp) {
		if (rwlp)
			csp->offset = (caddr_t)cvp - (caddr_t)rwlp;
		tdb_incr(csp->cond_signal);
	}

	return (cvp->cond_waiters? __lwp_cond_signal(cvp) : 0);
}

#pragma weak pthread_cond_signal = _cond_signal
#pragma weak _pthread_cond_signal = _cond_signal
#pragma weak _liblwp_pthread_cond_signal = _cond_signal
#pragma weak cond_signal = _cond_signal
#pragma weak _liblwp_cond_signal = _cond_signal
int
_cond_signal(cond_t *cvp)
{
	return (cond_signal_internal(cvp, NULL));
}

int
cond_broadcast_internal(cond_t *cvp, rwlock_t *rwlp)
{
	extern int __lwp_cond_broadcast(lwp_cond_t *);
	tdb_cond_stats_t *csp = COND_STATS(cvp);

	if (csp) {
		if (rwlp)
			csp->offset = (caddr_t)cvp - (caddr_t)rwlp;
		tdb_incr(csp->cond_broadcast);
	}

	return (cvp->cond_waiters? __lwp_cond_broadcast(cvp) : 0);
}

#pragma weak pthread_cond_broadcast = _cond_broadcast
#pragma weak _pthread_cond_broadcast = _cond_broadcast
#pragma weak _liblwp_pthread_cond_broadcast = _cond_broadcast
#pragma weak cond_broadcast = _cond_broadcast
#pragma weak _liblwp_cond_broadcast = _cond_broadcast
int
_cond_broadcast(cond_t *cvp)
{
	return (cond_broadcast_internal(cvp, NULL));
}

#pragma weak pthread_cond_destroy = _cond_destroy
#pragma weak _pthread_cond_destroy = _cond_destroy
#pragma weak _liblwp_pthread_cond_destroy = _cond_destroy
#pragma weak cond_destroy = _cond_destroy
#pragma weak _liblwp_cond_destroy = _cond_destroy
int
_cond_destroy(cond_t *cvp)
{
	cvp->cond_magic = 0;
	tdb_sync_obj_deregister(cvp);
	return (0);
}

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)timer.c	1.2	99/06/16 SMI"

#include <sys/timer.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/debug.h>

static kmem_cache_t *clock_timer_cache;
static clock_backend_t *clock_backend[CLOCK_MAX];

#define	CLOCK_BACKEND(clk) \
	((clk) < CLOCK_MAX && (clk) >= 0 ? clock_backend[(clk)] : NULL)

/*
 * Tunable to increase the maximum number of POSIX timers per-process.  This
 * may _only_ be tuned in /etc/system or by patching the kernel binary; it
 * _cannot_ be tuned on a running system.
 */
int timer_max = _TIMER_MAX;

/*
 * timer_lock() locks the specified interval timer.  It doesn't look at the
 * ITLK_REMOVE bit; it's up to callers to look at this if they need to
 * care.  p_lock must be held on entry; it may be dropped and reaquired,
 * but timer_lock() will always return with p_lock held.
 *
 * Note that timer_create() doesn't call timer_lock(); it creates timers
 * with the ITLK_LOCKED bit explictly set.
 */
static void
timer_lock(proc_t *p, itimer_t *it)
{
	ASSERT(MUTEX_HELD(&p->p_lock));

	while (it->it_lock & ITLK_LOCKED) {
		it->it_blockers++;
		cv_wait(&it->it_cv, &p->p_lock);
		it->it_blockers--;
	}

	it->it_lock |= ITLK_LOCKED;
}

/*
 * timer_unlock() unlocks the specified interval timer, waking up any
 * waiters.  p_lock must be held on entry; it will not be dropped by
 * timer_unlock().
 */
static void
timer_unlock(proc_t *p, itimer_t *it)
{
	ASSERT(MUTEX_HELD(&p->p_lock));
	ASSERT(it->it_lock & ITLK_LOCKED);
	it->it_lock &= ~ITLK_LOCKED;
	cv_signal(&it->it_cv);
}

/*
 * timer_delete_locked() takes a proc pointer, timer ID and locked interval
 * timer, and deletes the specified timer.  It must be called with p_lock
 * held, and cannot be called on a timer which already has ITLK_REMOVE set;
 * the caller must check this.  timer_delete_locked() will set the ITLK_REMOVE
 * bit and will iteratively unlock and lock the interval timer until all
 * blockers have seen the ITLK_REMOVE and cleared out.  It will then zero
 * out the specified entry in the p_itimer array, and call into the clock
 * backend to complete the deletion.
 *
 * This function will always return with p_lock held.
 */
static void
timer_delete_locked(proc_t *p, timer_t tid, itimer_t *it)
{
	ASSERT(MUTEX_HELD(&p->p_lock));
	ASSERT(!(it->it_lock & ITLK_REMOVE));
	ASSERT(it->it_lock & ITLK_LOCKED);

	it->it_lock |= ITLK_REMOVE;

	/*
	 * If there are threads waiting to lock this timer, we'll unlock
	 * the timer, and block on the cv.  Threads blocking our removal will
	 * have the opportunity to run; when they see the ITLK_REMOVE flag
	 * set, they will immediately unlock the timer.
	 */
	while (it->it_blockers) {
		timer_unlock(p, it);
		cv_wait(&it->it_cv, &p->p_lock);
		timer_lock(p, it);
	}

	ASSERT(p->p_itimer[tid] == it);
	p->p_itimer[tid] = NULL;

	/*
	 * No one is blocked on this timer, and no one will be (we've set
	 * p_itimer[tid] to be NULL; no one can find it).  Now we call into
	 * the clock backend to delete the timer; it is up to the backend to
	 * guarantee that timer_fire() has completed (and will never again
	 * be called) for this timer.
	 */
	mutex_exit(&p->p_lock);

	it->it_backend->clk_timer_delete(it);

	mutex_enter(&p->p_lock);

	/*
	 * We need to be careful freeing the sigqueue for this timer;
	 * if a signal is pending, the sigqueue needs to be freed
	 * synchronously in siginfofree().  The need to free the sigqueue
	 * in siginfofree() is indicated by setting sq_func to NULL.
	 */
	if (it->it_pending > 0) {
		it->it_sigq->sq_func = NULL;
	} else {
		kmem_free(it->it_sigq, sizeof (sigqueue_t));
	}

	ASSERT(it->it_blockers == 0);
	kmem_cache_free(clock_timer_cache, it);
}

/*
 * timer_grab() and its companion routine, timer_release(), are wrappers
 * around timer_lock()/_unlock() which allow the timer_*(3R) routines to
 * (a) share error handling code and (b) not grab p_lock themselves.  Routines
 * which are called with p_lock held (e.g. timer_lwpbind(), timer_lwpexit())
 * must call timer_lock()/_unlock() explictly.
 *
 * timer_grab() takes a proc and a timer ID, and returns a pointer to a
 * locked interval timer.  p_lock must _not_ be held on entry; timer_grab()
 * may acquire p_lock, but will always return with p_lock dropped.
 *
 * If timer_grab() fails, it will return NULL.  timer_grab() will fail if
 * one or more of the following is true:
 *
 *  (a)	The specified timer ID is out of range.
 *
 *  (b)	The specified timer ID does not correspond to a timer ID returned
 *	from timer_create(3R).
 *
 *  (c)	The specified timer ID is currently being removed.
 *
 */
static itimer_t *
timer_grab(proc_t *p, timer_t tid)
{
	itimer_t **itp, *it;

	if (tid >= timer_max || tid < 0)
		return (NULL);

	mutex_enter(&p->p_lock);

	if ((itp = p->p_itimer) == NULL || (it = itp[tid]) == NULL) {
		mutex_exit(&p->p_lock);
		return (NULL);
	}

	timer_lock(p, it);

	if (it->it_lock & ITLK_REMOVE) {
		/*
		 * Someone is removing this timer; it will soon be invalid.
		 */
		timer_unlock(p, it);
		mutex_exit(&p->p_lock);
		return (NULL);
	}

	mutex_exit(&p->p_lock);

	return (it);
}

/*
 * timer_release() releases a timer acquired with timer_grab().  p_lock
 * should not be held on entry; timer_release() will acquire p_lock but
 * will drop it before returning.
 */
static void
timer_release(proc_t *p, itimer_t *it)
{
	mutex_enter(&p->p_lock);
	timer_unlock(p, it);
	mutex_exit(&p->p_lock);
}

/*
 * timer_delete_grabbed() deletes a timer acquired with timer_grab().
 * p_lock should not be held on entry; timer_delete_grabbed() will acquire
 * p_lock, but will drop it before returning.
 */
static void
timer_delete_grabbed(proc_t *p, timer_t tid, itimer_t *it)
{
	mutex_enter(&p->p_lock);
	timer_delete_locked(p, tid, it);
	mutex_exit(&p->p_lock);
}

void
clock_timer_init()
{
	clock_timer_cache = kmem_cache_create("timer_cache",
	    sizeof (itimer_t), 0, NULL, NULL, NULL, NULL, NULL, 0);
}

void
clock_add_backend(clockid_t clock, clock_backend_t *backend)
{
	ASSERT(clock < CLOCK_MAX);
	ASSERT(clock_backend[clock] == NULL);

	clock_backend[clock] = backend;
}

int
clock_settime(clockid_t clock, timespec_t *tp)
{
	timespec_t t;
	clock_backend_t *backend;
	int error;

	if ((backend = CLOCK_BACKEND(clock)) == NULL)
		return (set_errno(EINVAL));

	if (!suser(CRED()))
		return (set_errno(EPERM));

	if (get_udatamodel() == DATAMODEL_NATIVE) {
		if (copyin(tp, &t, sizeof (timespec_t)) != 0)
			return (set_errno(EFAULT));
	} else {
		timespec32_t t32;

		if (copyin(tp, &t32, sizeof (timespec32_t)) != 0)
			return (set_errno(EFAULT));

		TIMESPEC32_TO_TIMESPEC(&t, &t32);
	}

	if (itimerspecfix(&t))
		return (set_errno(EINVAL));

	error = backend->clk_clock_settime(&t);

	if (error)
		return (set_errno(error));

	return (0);
}

int
clock_gettime(clockid_t clock, timespec_t *tp)
{
	timespec_t t;
	clock_backend_t *backend;
	int error;

	if ((backend = CLOCK_BACKEND(clock)) == NULL)
		return (set_errno(EINVAL));

	error = backend->clk_clock_gettime(&t);

	if (error)
		return (set_errno(error));

	if (get_udatamodel() == DATAMODEL_NATIVE) {
		if (copyout(&t, tp, sizeof (timespec_t)) != 0)
			return (set_errno(EFAULT));
	} else {
		timespec32_t t32;

		if (TIMESPEC_OVERFLOW(&t))
			return (set_errno(EOVERFLOW));
		TIMESPEC_TO_TIMESPEC32(&t32, &t);

		if (copyout(&t32, tp, sizeof (timespec32_t)) != 0)
			return (set_errno(EFAULT));
	}

	return (0);
}

int
clock_getres(clockid_t clock, timespec_t *tp)
{
	timespec_t t;
	clock_backend_t *backend;
	int error;

	/*
	 * Strangely, the standard defines clock_getres() with a NULL tp
	 * to do nothing (regardless of the validity of the specified
	 * clock_id).  Go figure.
	 */
	if (tp == NULL)
		return (0);

	if ((backend = CLOCK_BACKEND(clock)) == NULL)
		return (set_errno(EINVAL));

	error = backend->clk_clock_getres(&t);

	if (error)
		return (set_errno(error));

	if (get_udatamodel() == DATAMODEL_NATIVE) {
		if (copyout(&t, tp, sizeof (timespec_t)) != 0)
			return (set_errno(EFAULT));
	} else {
		timespec32_t t32;

		if (TIMESPEC_OVERFLOW(&t))
			return (set_errno(EOVERFLOW));
		TIMESPEC_TO_TIMESPEC32(&t32, &t);

		if (copyout(&t32, tp, sizeof (timespec32_t)) != 0)
			return (set_errno(EFAULT));
	}

	return (0);
}

void
timer_signal(sigqueue_t *sigq)
{
	itimer_t *it = (itimer_t *)sigq->sq_backptr;

	/*
	 * Unfortunately, we can't assert that p_lock is held (there
	 * are some conditions during a fork or an exit when we can
	 * call siginfofree() without p_lock held).
	 */
	ASSERT(it->it_pending > 0);
	it->it_overrun = it->it_pending - 1;
	it->it_pending = 0;
}

/*
 * This routine is called from the clock backend.
 */
void
timer_fire(itimer_t *it)
{
	kthread_t *t;
	proc_t *p = it->it_proc;

	if (it->it_flags & IT_PERLWP) {
		/*
		 * It is safe to look at it_lwp even though we don't have
		 * this timer locked:  if IT_PERLWP is set, it_lwp is
		 * only written once.
		 */
		t = lwptot(it->it_lwp);
	} else {
		t = NULL;
	}

	mutex_enter(&p->p_lock);

	if (it->it_pending > 0) {
		if (it->it_pending < INT_MAX)
			it->it_pending++;
		goto out;
	}

	if (it->it_flags & IT_SIGNAL) {
		it->it_pending = 1;
		sigaddqa(p, t, it->it_sigq);
	}
out:
	mutex_exit(&p->p_lock);
}

int
timer_create(clockid_t clock, struct sigevent *evp, timer_t *tid)
{
	struct sigevent ev;
	proc_t *p = curproc;
	clock_backend_t *backend;
	itimer_t *it, **itp;
	sigqueue_t *sigq;
	cred_t *cr = CRED();
	int error = 0;
	timer_t i;

	if ((backend = CLOCK_BACKEND(clock)) == NULL)
		return (set_errno(EINVAL));

	if (evp != NULL) {
		/*
		 * short copyin() for binary compatibility
		 * fetch oldsigevent to determine how much to copy in.
		 */
		if (get_udatamodel() == DATAMODEL_NATIVE) {
			if (copyin(evp, &ev, sizeof (struct oldsigevent)))
				return (set_errno(EFAULT));
		} else {
			struct sigevent32 ev32;

			if (copyin(evp, &ev32, sizeof (struct oldsigevent32)))
				return (set_errno(EFAULT));
			ev.sigev_notify = ev32.sigev_notify;
			ev.sigev_signo = ev32.sigev_signo;
			/*
			 * See comment in sigqueue32() on handling of 32-bit
			 * sigvals in a 64-bit kernel.
			 */
			ev.sigev_value.sival_int = ev32.sigev_value.sival_int;
		}
		switch (ev.sigev_notify) {
		case SIGEV_NONE:
			break;

		case SIGEV_SIGNAL:
			if (ev.sigev_signo < 1 || ev.sigev_signo >= NSIG)
				return (set_errno(EINVAL));
			break;
		default:
			return (set_errno(EINVAL));
		}
	} else {
		/*
		 * Use the clock's default sigevent (this is a structure copy).
		 */
		ev = backend->clk_default;
	}

	/*
	 * We'll allocate our timer and sigqueue now, before we grab p_lock.
	 * If we can't find an empty slot, we'll free them before returning.
	 */
	it = kmem_cache_alloc(clock_timer_cache, KM_SLEEP);
	bzero(it, sizeof (itimer_t));
	sigq = kmem_zalloc(sizeof (sigqueue_t), KM_SLEEP);

	mutex_enter(&p->p_lock);

	/*
	 * If this is this process' first timer, we need to attempt to allocate
	 * an array of timerstr_t pointers.  We drop p_lock to perform the
	 * allocation; if we return to discover that p_itimer is non-NULL,
	 * we will free our allocation and drive on.
	 */
	if ((itp = p->p_itimer) == NULL) {
		mutex_exit(&p->p_lock);
		itp = kmem_zalloc(timer_max * sizeof (itimer_t *), KM_SLEEP);
		mutex_enter(&p->p_lock);

		if (p->p_itimer == NULL)
			p->p_itimer = itp;
		else {
			kmem_free(itp, timer_max * sizeof (itimer_t *));
			itp = p->p_itimer;
		}
	}

	for (i = 0; i < timer_max && itp[i] != NULL; i++)
		continue;

	if (i == timer_max) {
		/*
		 * We couldn't find a slot.  Drop p_lock, free the preallocated
		 * timer and sigqueue, and return an error.
		 */
		mutex_exit(&p->p_lock);
		kmem_cache_free(clock_timer_cache, it);
		kmem_free(sigq, sizeof (sigqueue_t));

		return (set_errno(EAGAIN));
	}

	ASSERT(i < timer_max && itp[i] == NULL);

	/*
	 * If we develop other notification mechanisms, this will need
	 * to call into (yet another) backend.
	 */
	sigq->sq_info.si_signo = ev.sigev_signo;
	sigq->sq_info.si_value = ev.sigev_value;
	sigq->sq_info.si_code = SI_TIMER;
	sigq->sq_info.si_pid = p->p_pid;
	sigq->sq_info.si_uid = cr->cr_ruid;
	sigq->sq_func = timer_signal;
	sigq->sq_next = NULL;
	sigq->sq_backptr = it;
	it->it_sigq = sigq;
	it->it_backend = backend;
	it->it_lock = ITLK_LOCKED;
	itp[i] = it;

	if (ev.sigev_notify == SIGEV_SIGNAL)
		it->it_flags |= IT_SIGNAL;

	mutex_exit(&p->p_lock);

	/*
	 * Call on the backend to verify the event argument (or return
	 * EINVAL if this clock type does not support timers).
	 */
	if ((error = backend->clk_timer_create(it, &ev)) != 0)
		goto err;

	it->it_lwp = ttolwp(curthread);
	it->it_proc = p;

	if (copyout(&i, tid, sizeof (timer_t)) != 0) {
		error = EFAULT;
		goto err;
	}

	/*
	 * If we're here, then we have successfully created the timer; we
	 * just need to release the timer and return.
	 */
	timer_release(p, it);

	return (0);

err:
	/*
	 * If we're here, an error has occurred late in the timer creation
	 * process.  We need to regrab p_lock, and delete the incipient timer.
	 * Since we never unlocked the timer (it was born locked), it's
	 * impossible for a removal to be pending.
	 */
	ASSERT(!(it->it_lock & ITLK_REMOVE));
	timer_delete_grabbed(p, i, it);

	return (set_errno(error));
}

int
timer_gettime(timer_t tid, itimerspec_t *val)
{
	proc_t *p = curproc;
	itimer_t *it;
	itimerspec_t when;
	int error;

	if ((it = timer_grab(p, tid)) == NULL)
		return (set_errno(EINVAL));

	error = it->it_backend->clk_timer_gettime(it, &when);

	timer_release(p, it);

	if (error == 0) {
		if (get_udatamodel() == DATAMODEL_NATIVE) {
			if (copyout(&when, val, sizeof (itimerspec_t)))
				error = EFAULT;
		} else {
			if (ITIMERSPEC_OVERFLOW(&when))
				error = EOVERFLOW;
			else {
				itimerspec32_t w32;

				ITIMERSPEC_TO_ITIMERSPEC32(&w32, &when)
				if (copyout(&w32, val, sizeof (itimerspec32_t)))
					error = EFAULT;
			}
		}
	}

	return (error ? set_errno(error) : 0);
}

int
timer_settime(timer_t tid, int flags, itimerspec_t *val, itimerspec_t *oval)
{
	itimerspec_t when;
	itimer_t *it;
	proc_t *p = curproc;
	int error;

	if (oval != NULL) {
		if ((error = timer_gettime(tid, oval)) != 0)
			return (error);
	}

	if (get_udatamodel() == DATAMODEL_NATIVE) {
		if (copyin(val, &when, sizeof (itimerspec_t)))
			return (set_errno(EFAULT));
	} else {
		itimerspec32_t w32;

		if (copyin(val, &w32, sizeof (itimerspec32_t)))
			return (set_errno(EFAULT));

		ITIMERSPEC32_TO_ITIMERSPEC(&when, &w32);
	}

	if (itimerspecfix(&when.it_value) ||
	    (itimerspecfix(&when.it_interval) &&
	    timerspecisset(&when.it_value))) {
		return (set_errno(EINVAL));
	}

	if ((it = timer_grab(p, tid)) == NULL)
		return (set_errno(EINVAL));

	it->it_itime = when;
	error = it->it_backend->clk_timer_settime(it, flags, &it->it_itime);

	timer_release(p, it);

	return (error ? set_errno(error) : 0);
}

int
timer_delete(timer_t tid)
{
	proc_t *p = curproc;
	itimer_t *it;

	if ((it = timer_grab(p, tid)) == NULL)
		return (set_errno(EINVAL));

	timer_delete_grabbed(p, tid, it);

	return (0);
}

int
timer_getoverrun(timer_t tid)
{
	int overrun;
	proc_t *p = curproc;
	itimer_t *it;

	if ((it = timer_grab(p, tid)) == NULL)
		return (set_errno(EINVAL));

	/*
	 * The it_overrun field is protected by p_lock; we need to acquire
	 * it before looking at the value.
	 */
	mutex_enter(&p->p_lock);
	overrun = it->it_overrun;
	mutex_exit(&p->p_lock);

	timer_release(p, it);

	return (overrun);
}

/*
 * Entered/exited with p_lock held, but will repeatedly drop and regrab
 * p_lock.
 */
void
timer_lwpexit(void)
{
	timer_t i;
	proc_t *p = curproc;
	klwp_t *lwp = ttolwp(curthread);
	itimer_t *it, **itp;

	ASSERT(MUTEX_HELD(&p->p_lock));

	if ((itp = p->p_itimer) == NULL)
		return;

	for (i = 0; i < timer_max; i++) {
		if ((it = itp[i]) == NULL)
			continue;

		timer_lock(p, it);

		if ((it->it_lock & ITLK_REMOVE) || it->it_lwp != lwp) {
			/*
			 * This timer is either being removed or it isn't
			 * associated with this lwp.
			 */
			timer_unlock(p, it);
			continue;
		}

		if (it->it_flags & IT_PERLWP) {
			/*
			 * If this is a per-LWP timer, we need to destroy it.
			 */
			timer_delete_locked(p, i, it);
			continue;
		}

		/*
		 * The LWP that created this timer is going away.  To the user,
		 * our behavior here is explicitly undefined.  We will simply
		 * null out the it_lwp field; if the LWP was bound to a CPU,
		 * the cyclic will stay bound to that CPU until the process
		 * exits.
		 */
		it->it_lwp = NULL;
		timer_unlock(p, it);
	}
}

/*
 * Called to notify of an LWP binding change.  Entered/exited with p_lock
 * held, but will repeatedly drop and regrab p_lock.
 */
void
timer_lwpbind()
{
	timer_t i;
	proc_t *p = curproc;
	klwp_t *lwp = ttolwp(curthread);
	itimer_t *it, **itp;

	ASSERT(MUTEX_HELD(&p->p_lock));

	if ((itp = p->p_itimer) == NULL)
		return;

	for (i = 0; i < timer_max; i++) {
		if ((it = itp[i]) == NULL)
			continue;

		timer_lock(p, it);

		if (!(it->it_lock & ITLK_REMOVE) && it->it_lwp == lwp) {
			/*
			 * Drop p_lock and jump into the backend.
			 */
			mutex_exit(&p->p_lock);
			it->it_backend->clk_timer_lwpbind(it);
			mutex_enter(&p->p_lock);
		}

		timer_unlock(p, it);
	}
}

/*
 * This function should only be called if p_itimer is non-NULL.
 */
void
timer_exit(void)
{
	timer_t i;
	proc_t *p = curproc;

	ASSERT(p->p_itimer != NULL);

	for (i = 0; i < timer_max; i++)
		(void) timer_delete(i);

	kmem_free(p->p_itimer, timer_max * sizeof (itimer_t *));
	p->p_itimer = NULL;
}

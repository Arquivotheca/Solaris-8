/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)sigwait.c	1.34	98/09/16 SMI"

#ifdef __STDC__
#pragma weak sigwait = _sigwait

#pragma	weak _ti_sigwait = _sigwait
#pragma weak _ti_sigtimedwait = __sigtimedwait
#endif /* __STDC__ */

#include "libthread.h"
#include <signal.h>
#include <errno.h>

/*
 * GLobal variables
 */
cond_t   _sigwait_cv = DEFAULTCV;


/*
 * Static functions
 */
static	int get_sig(sigset_t *smask, const sigset_t *rmask, int *sigp,
    siginfo_t *sip);

static	int cond_wait_sig(cond_t *cvp, sigset_t *set);
static	void dummy_hdlr();


#define	pending(s1, s2, s3)(\
	((s3)->__sigbits[0] = (s1)->__sigbits[0] & (s2)->__sigbits[0]) ||\
	    ((s3)->__sigbits[1] = (s1)->__sigbits[1] & (s2)->__sigbits[1]) ||\
	    ((s3)->__sigbits[2] = (s1)->__sigbits[2] & (s2)->__sigbits[2]) ||\
	    ((s3)->__sigbits[3] = (s1)->__sigbits[3] & (s2)->__sigbits[3]))

#define	TDIR 1
#define	PDIR 2

static struct timespec __zero_to = {0, 0};

/*
 * Return 1 if the signal was directed to this thread via thr_kill().
 * Return 2 if the signal was directed to the process.
 * Return 0 if no signal was found.
 * Return EINTR if signal not in signal set being waited for was found.
 * Return -1 on error.
 */
static int
get_sig(sigset_t *smask, const sigset_t *rmask, int *sigp, siginfo_t *sip)
{
	uthread_t *t = curthread;
	int sig = 0;
	int ret = 0;
	sigset_t found;
	sigset_t sigwsig;
	sigset_t sigs;
	int queued = 0;

	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(MUTEX_HELD(&_pmasklock));
	ASSERT(sigp != NULL);

	*sigp = 0;
	_sigemptyset(&found);
	maskreverse(smask, &sigs);
	if (pending(&t->t_psig, &sigs, &found)) {
		while (sig = _fsig(&found)) {
			if (ISIGNORED(sig)) {
				_sigdelset(&t->t_psig, sig);
				_sigdelset(&found, sig);
				t->t_pending = !sigisempty(&t->t_psig);
			}
			else
				break;
		}
		if (sig == 0)
			return (ret);
		ASSERT(sig != 0);
		if (_sigismember(rmask, sig)) {
			_sigdelset(&t->t_psig, sig);
			_sigdelset(&t->t_ssig, sig);
			t->t_pending = !sigisempty(&t->t_psig);
			*sigp = sig;
			if (sip != NULL) {
				sip->si_signo = sig;
				sip->si_code = SI_NOINFO;
			}
			ret = TDIR;
		} else
			ret = EINTR;
	} else if (pending(&_pmask, &sigs, &found)) {
		while (sig = _fsig(&found)) {
			if (ISIGNORED(sig)) {
				_sigdelset(&_pmask, sig);
				_sigdelset(&found, sig);
				dbg_delset(&_tpmask, sig);
				_lwp_sigredirect(0, sig, 0);
			}
			else
				break;
		}
		if (sig == 0)
			return (ret);
		ASSERT(sig != 0);
		if (_sigismember(rmask, sig)) {
			_sigemptyset(&sigwsig);
			_sigaddset(&sigwsig, sig);
			errno = 0;
			if (_lwp_sigtimedwait(&sigwsig, sip, &__zero_to,
				&queued) == -1) {
				/*
				 * The zero timeout above ensures a non-blocking
				 * call. The signal may not be found in the
				 * kernel - this is possible only if the current
				 * process is the child of a MT process which
				 * had pending signals at the time fork() or
				 * fork1() was called. The kernel clears all
				 * pending signals in the child of the fork.
				 * However, the user-level mirror of pending
				 * signals is not cleared at the time of the
				 * fork. It is simple for the kernel to clear
				 * all pending signals, since they are all at
				 * one place (p_notifsigs), but it is harder for
				 * the user level since the mirror is a union
				 * of _pmask and all threads' t_bsig masks.
				 * For the child of a fork1(), clearing the user
				 * mirror is much easier (just clear _pmask in
				 * _resetlib()), but for the child of a fork(),
				 * with cloned threads, this is much harder.
				 * The solution adopted here is a general
				 * solution for both fork() and fork1() - but
				 * more for the fork() case. The solution is to
				 * clear the user-level mirror lazily, as and
				 * when required. If errno == EAGAIN, this
				 * process must be the child of a fork in this
				 * situation. If so, do not set "ret" to -1,
				 * return zero in "ret" and *sigp, which causes
				 * the thread to go back to sleep in sigwait().
				 * This is like a spurious wake-up which lazily
				 * clears or corrects the pending signal state
				 * at user-level.
				 * Same comment applies in the t_bsig case below
				 */
				if (errno != EAGAIN)
					ret = -1;
			} else {
				*sigp = sig;
				ret = PDIR;
			}
			if (!queued || (errno == EAGAIN))
				_sigdelset(&_pmask, sig);
		} else
			ret = EINTR;
	} else if (pending(&t->t_bsig, &sigs, &found)) {
		while (sig = _fsig(&found)) {
			if (ISIGNORED(sig)) {
				_sigdelset(&t->t_bsig, sig);
				_sigdelset(&found, sig);
				t->t_bdirpend = !sigisempty(&t->t_bsig);
				_lwp_sigredirect(0, sig, 0);
			}
			else
				break;
		}
		if (sig == 0)
			return (ret);
		ASSERT(sig != 0);
		if (_sigismember(rmask, sig)) {
			_sigemptyset(&sigwsig);
			_sigaddset(&sigwsig, sig);
			errno = 0;
			if (_lwp_sigtimedwait(&sigwsig, sip, &__zero_to,
				&queued) == -1) {
				if (errno != EAGAIN)
					/*
					 * See big comment above about lazy
					 * clearing of pending signals.
					 * If errno == EAGAIN, this is OK since
					 * it occurs in the special scenario
					 * (child of a fork) described above.
					 * EAGAIN implies that the call to
					 * sigtimedwait() timed out. Since a
					 * zero timeout was supplied, this
					 * really implies that the signal was
					 * not found in the kernel.
					 */
					ret = -1;
			} else {
				*sigp = sig;
				ret = PDIR;
			}
			if (!queued || errno == EAGAIN) {
				_sigdelset(&t->t_bsig, sig);
				t->t_bdirpend = !sigisempty(&t->t_bsig);
			}
		} else
			ret = EINTR;
	}
	return (ret);
}

/*
 * Atomically wait, release lock and unblock signals in "set".
 * XXX: Reduce the number of arguments!
 */
static int
cond_timedwait_sig(cond_t *cvp, sigset_t *smask, const sigset_t *rmask,
    int *psigp, siginfo_t *info, int *dir, const struct timespec *to)
{
	uthread_t *t = curthread;
	struct timeval tv;
	int cv_timedout = 0;
	int ret;

	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(MUTEX_HELD(&_pmasklock));
	ASSERT(psigp != NULL);
	ASSERT(*psigp == 0);
	ASSERT(dir != NULL);

	if (to != NULL) {
		_lwp_mutex_unlock(&_pmasklock);
		_sched_unlock_nosig();
		tv.tv_sec = to->tv_sec;
		tv.tv_usec = to->tv_nsec/1000;
		_setcallout(&curthread->t_cv_callo, NULL, &tv, _setrun,
		    (uintptr_t)curthread);
		_sched_lock_nosig();
		_lwp_mutex_lock(&_pmasklock);
		ret = get_sig(smask, rmask, psigp, info);
		*dir = ret;
		if (*psigp != 0 || ret == -1 || ret == EINTR) {
			/*
			 * got a signal while establishing timeout.
			 */
			_lwp_mutex_unlock(&_pmasklock);
			_sched_unlock_nosig();
			_rmcallout(&curthread->t_cv_callo);
			_sched_lock_nosig();
			_lwp_mutex_lock(&_pmasklock);
			return (ret);
		} else if (ISTIMEDOUT(&curthread->t_cv_callo)) {
			return (ETIME);
		}
	}
	t->t_flag &= ~T_INTR;
	t->t_flag |= T_WAITCV;
	_t_block((caddr_t)cvp);
	cvp->cond_waiters = 1;
	_lwp_mutex_unlock(&_pmasklock);
	_sched_unlock_nosig();
	_thr_sigsetmask(SIG_SETMASK, smask, NULL);
	_cancelon();
	_swtch(0);
	_canceloff();
	if (to != NULL)
		cv_timedout = _rmcallout(&curthread->t_cv_callo);
	maskallsigs(&t->t_hold);
	_sched_lock_nosig();
	t->t_flag &= ~T_INTR;
	_lwp_mutex_lock(&_pmasklock);
	ret = get_sig(smask, rmask, psigp, info);
	*dir = ret;
	if (cv_timedout != 0)
		return (ETIME);
	else
		return (ret);
}

int
_sigwait(const sigset_t *set)
{
	return (__sigtimedwait(set, NULL, NULL));
}


#define	NANOSEC 1000000000

/*
 * This routine __sigtimedwait() interposes on __sigtimedwait() exported by
 * libc for librt. So the name should not be changed without also changing
 * libc and librt.
 */
int
__sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *to)
{
	uthread_t *t = curthread;
	sigset_t othmask;
	sigset_t sleepmask, newset;
	sigset_t lset;
	sigset_t *lsetp = &lset;
	int sig = 0;
	int zerotimeout = 0;
	int breakloop = 0;
	int dir = 0;
	int ret = 0;

	if (to != NULL) {
		if (to->tv_sec < 0 || to->tv_nsec < 0 ||
		    to->tv_nsec >= NANOSEC) {
			errno = EINVAL;
			return (-1);
		}
		if (to->tv_sec == 0 && to->tv_nsec == 0)
			zerotimeout = 1;
	}
	t->t_flag |= T_SIGWAIT;
	/*
	 * The following is to make sure there is nothing pending on the
	 * underlying LWP. Masking all signals ensures this.
	 */
	_thr_sigsetmask(SIG_SETMASK, &_allmasked, &othmask);
	sleepmask = othmask;
	lset = *set;
	/*
	 * Delete all the unmaskable signals from the requested signal set.
	 * These are not catchable either. The restriction of the caller not
	 * being able to wait for such signals is silently imposed by this
	 * implementation, similar to sigprocmask(2)/sigsuspend(2).
	 */
	sigdiffset(lsetp, &_cantmask);
	sigdiffset(&sleepmask, lsetp);
	maskreverse(&sleepmask, &newset);
	_sched_lock();
	_lwp_mutex_lock(&_pmasklock);
	ret = get_sig(&sleepmask, lsetp, &sig, info);
	if (sig == 0 && zerotimeout == 1) {
		_lwp_mutex_unlock(&_pmasklock);
		_sched_unlock();
		errno = EAGAIN;
		_thr_sigsetmask(SIG_SETMASK, &othmask, NULL);
		return (-1);
	}
	while (sig == 0 && breakloop == 0) {
		if ((ret = cond_timedwait_sig(&_sigwait_cv, &sleepmask,
		    lsetp, &sig, info, &dir, to)) == ETIME || ret == EINTR)
			breakloop = 1;
		else if (ret == -1) {
			_lwp_mutex_unlock(&_pmasklock);
			_sched_unlock_nosig();
			_thr_sigsetmask(SIG_SETMASK, &othmask, NULL);
			_sigon();
			return (-1);
		}
	}
	if (sig != 0) {
		ASSERT(_sigismember(lsetp, sig));
		_lwp_mutex_unlock(&_pmasklock);
		t->t_flag &= ~T_SIGWAIT;
		_sched_unlock_nosig();
		_thr_sigsetmask(SIG_SETMASK, &othmask, NULL);
		_sigon();
		return (sig);
	} else {
		ASSERT(breakloop != 0);
		_lwp_mutex_unlock(&_pmasklock);
		t->t_flag &= ~T_SIGWAIT;
		_sched_unlock_nosig();
		_thr_sigsetmask(SIG_SETMASK, &othmask, NULL);
		_sigon();
		if (ret == ETIME)
			errno = EAGAIN;
		else
			errno = ret;
		return (-1);
	}
}

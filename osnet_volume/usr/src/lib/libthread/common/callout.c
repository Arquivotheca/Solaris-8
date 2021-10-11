/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)callout.c	1.46	99/01/27 SMI"

#include "libthread.h"

/*
 * Global variables
 */
int	_timerset = 0; 	/* interval timer is set if timerset == 1 */
thread_t _co_tid = 0;	/* thread that does the callout processing */
int	_co_set = 0;	/* create only one thread to run callouts */
int	_calloutcnt = 0; /* number of pending callouts */
callo_t	*_calloutp = NULL; /* pointer to the callout queue */
mutex_t	_calloutlock = DEFAULTMUTEX; /* protects queue of callouts */

/*
 * Static variables
 */
static	lwp_sema_t _settimer;	/* signal co_timer() to set itimer */
static	sigset_t _alrmmask;	/* masks off SIGALRM */

/*
 * Static functions
 */
static	void _co_enable(void);
static	void _co_timerset(void *arg);
static	void _delcallout(callo_t *cop);
static	void _swapcallout(callo_t *cop1, callo_t *cop2);
static	void _setrealitimer(struct timeval *clocktime,
					struct timeval *tv);

/*
 * The callouts are linked together by a double linked list.
 * "calloutp points to the first element of this list. Elements
 * are put onto the list in time of day order.
 * If cop is already on the callout list (TIMER_ON), compute the
 * time remaining (in secs) after which the callout is supposed
 * to happen and delete the callout.
 * If time in tv is 0, simply return this time value, otherwise,
 * install the new callout and then return this time value.
 */
int
_setcallout(callo_t *cop, thread_t tid, const struct timeval *tv,
					void (*func)(), uintptr_t arg)
{
	callo_t *cp, *pcp;
	struct timeval clocktime;
	sigset_t set;
	struct itimerval it;
	int ret = 0;

	_lmutex_lock(&_calloutlock);
	if (!_co_set) {
		_co_set = 1;
		_sigemptyset(&_alrmmask);
		_sigaddset(&_alrmmask, SIGALRM);
		_lmutex_unlock(&_calloutlock);
		_co_enable();
		_lmutex_lock(&_calloutlock);
	}
	_gettimeofday(&clocktime, 0);
	if (cop->flag == CO_TIMER_ON) {
		/*
		 * compute time remaining until this callout was
		 * supposed to happen and later return this time.
		 */
		if (cop->time.tv_sec > clocktime.tv_sec) {
			ret = cop->time.tv_sec - clocktime.tv_sec;
		}
		_delcallout(cop);
	}
	cop->flag = CO_TIMER_OFF;
	if (tv->tv_sec == 0 && tv->tv_usec == 0) {
		cop->flag = CO_TIMEDOUT;
		_lmutex_unlock(&_calloutlock);
		return (ret);
	}
	clocktime.tv_sec += tv->tv_sec;
	clocktime.tv_usec += tv->tv_usec;
	if (clocktime.tv_usec >= 1000000) {
		clocktime.tv_usec -= 1000000;
		clocktime.tv_sec += 1;
	}
	_calloutcnt++;
	ASSERT(_calloutcnt > 0);
	cp = _calloutp;
	pcp = NULL;
	while (cp != NULL) {
		if (clocktime.tv_sec < cp->time.tv_sec || (clocktime.tv_sec ==
		    cp->time.tv_sec && clocktime.tv_usec < cp->time.tv_usec))
			break;
		pcp = cp;
		cp = cp->forw;
	}
	cop->flag = CO_TIMER_ON;
	cop->time = clocktime;
	cop->func = func;
	cop->arg = arg;
	cop->tid = tid;
	if (cp == _calloutp) {
		/* at the front of the list */
		cop->forw = cp;
		if (cp != NULL)
			cp->backw = cop;
		cop->backw = NULL;
		_calloutp = cop;
		_lmutex_unlock(&_calloutlock);
		_lwp_sema_post(&_settimer);
		return (ret);
	} else if (cp == NULL) {
		/* at the end of the list */
		cop->forw = NULL;
		pcp->forw = cop;
		cop->backw = pcp;
	} else {
		/* in the middle of the list */
		cp->backw->forw = cop;
		cop->forw = cp;
		cop->backw = cp->backw;
		cp->backw = cop;
	}
	_lmutex_unlock(&_calloutlock);
	return (ret);
}

/*
 * remove a thread from the list of pending callouts.
 */
int
_rmcallout(callo_t *cop)
{
	int timedout = 0;

	_lmutex_lock(&_calloutlock);
	if (cop->flag == CO_TIMER_ON)
		_delcallout(cop);
	else
		timedout = (cop->flag == CO_TIMEDOUT);
	cop->flag = CO_TIMER_OFF;
	_lmutex_unlock(&_calloutlock);
	return (timedout);
}

/*
 * return 1 when callout has already timed out. otherwise
 * return 0.
 */
static void
_delcallout(callo_t *cop)
{
	_calloutcnt--;
	if (cop == _calloutp) {
		/* at front of list */
		_calloutp = cop->forw;
		if (_calloutp != NULL)
			_calloutp->backw = NULL;
	} else if (cop->forw == NULL)
		/* at end of list */
		cop->backw->forw = NULL;
	else {
		/* in middle of the list */
		cop->backw->forw = cop->forw;
		cop->forw->backw = cop->backw;
	}
}

static void
_swapcallout(callo_t *cop1, callo_t *cop2)
{
	*cop2 = *cop1;
	if (cop1 == _calloutp) {
		_calloutp = cop2;
		if (cop1->forw)
			cop1->forw->backw = cop2;
	} else if (cop1->forw == NULL) {
		cop1->backw->forw = cop2;
	} else {
		cop1->forw->backw = cop2;
		cop1->backw->forw = cop2;
	}
	cop1->forw = cop1->backw = NULL;
}

/*
 * Note that _callin should be executed with all signals blocked. This is
 * because the callout thread executing this handler is a daemon thread
 * and should have all signals blocked at all times so as to not violate the
 * virtual process mask.
 */
void
_callin(int sig, siginfo_t *sip, ucontext_t *uap)
{
	struct timeval clocktime;
	struct itimerval it;
	callo_t *cop, co;
	callo_t placeholder;
	thread_t tid = _thr_self();
	int err;
	struct sigaction act;

	if (tid != _co_tid) {

		/*
		* Here, the async safe version of acquiring _sighandlerlock
		* should be used, since  SIGLWP is unmasked at this point
		* and can interrupt the critical section, causing a potential
		* deadlock on _sighandlerlock which is also acquired in the
		* main libthread signal handler for all signals.
		*/

		__sighandler_lock();
		act = __alarm_sigaction;
		__sighandler_unlock();

		if (act.sa_handler != SIG_DFL && act.sa_handler != SIG_IGN) {
			if (act.sa_handler == _callin)
				_panic("_callin: recursive");
			/*
			 * Emulate the SA_RESETHAND flag, if set, here.
			 */
			if (act.sa_flags & SA_RESETHAND)
				__sig_resethand(sig);
			/*
			 * The thread mask has been changed by sigacthandler().
			 * The old mask is stored in uap. So, before calling
			 * the user installed segv handler, establish the
			 * correct mask.
			 */
			sigorset(&act.sa_mask, &(uap->uc_sigmask));
			thr_sigsetmask(SIG_SETMASK, &act.sa_mask, NULL);
			(*act.sa_handler)(sig, sip, uap);
			/*
			 * old thread mask will be restored in sigacthandler
			 * which called _callin().
			 */
			return;
		} else if (act.sa_handler == SIG_DFL) {
			/* send SIGALRM to myself and blow myself away */
			__sigaction(SIGALRM, &act, NULL);
			if (_lwp_kill(curthread->t_lwpid, SIGALRM) < 0) {
				_panic("_callin: _lwp_kill");
			}
		}
		return;
	}
	_timerset--;
	_gettimeofday(&clocktime, 0);
	_lmutex_lock(&_calloutlock);
	cop = _calloutp;
	while (cop != NULL) {
		if (clocktime.tv_sec > cop->time.tv_sec ||
		    (clocktime.tv_sec == cop->time.tv_sec &&
		    clocktime.tv_usec >= cop->time.tv_usec)) {
			cop->flag = CO_TIMEDOUT;
			_swapcallout(cop, &placeholder);
			cop->running = 1;
			_lmutex_unlock(&_calloutlock);
			(*cop->func)(cop->arg);
			_lmutex_lock(&_calloutlock);
			cop->running = 0;
			_cond_signal(&cop->waiting);
			_delcallout(&placeholder);
			cop = placeholder.forw;
			continue;
		}
		break;
	}
	ASSERT((cop == NULL)?(_calloutp == NULL):1);
	if (cop)
		co = *cop;
	_lmutex_unlock(&_calloutlock);
	/*
	 * re-enable realtime interval timer for next callout.
	 */
	if (cop) {
		_setrealitimer(&clocktime, &co.time);
	}

}

/*
 * A bound thread is in charge of dispatching callouts by setting a real
 * time interval timer. The interval is set to the earliest arrival time of
 * a callout in the list of callouts.
 *
 * This routine assumes that it is not re-entrant with respect to SIGALRM.
 */
static void
_setrealitimer(struct timeval *clocktime, struct timeval *tv)
{
	struct itimerval it;
	int err;
	sigset_t otmask;
	ASSERT(ISBOUND(curthread));

#ifdef BUG_1156578
	if (clocktime->tv_usec > 1000000) {
		clocktime->tv_usec -= 1000000;
		clocktime->tv_sec++;
	}
#endif /* 1156578 */

	it.it_interval.tv_sec = it.it_interval.tv_usec = 0;
	it.it_value.tv_usec = tv->tv_usec - clocktime->tv_usec;
	it.it_value.tv_sec = tv->tv_sec - clocktime->tv_sec;
	_timerset++;
	if (it.it_value.tv_sec < 0 || (it.it_value.tv_sec == 0 &&
	    it.it_value.tv_usec <= 0)) {
		/* interval has expired before it could be set */
		ASSERT(curthread->t_tid == _co_tid);
		if (_calloutp != NULL) {
			/*
			 * Since this is called only from the bound daemon
			 * thread, use the LWP signal mask.
			 */
			__sigprocmask(SIG_BLOCK, &_alrmmask, &otmask);
			_callin(NULL, NULL, NULL);
			__sigprocmask(SIG_SETMASK, &otmask, NULL);
		}
	} else {
		/*
		 * calculate the elapsed time at which the real time interval
		 * timer should expire. It is calculated by subtracting
		 * the current clock time from the arrival time. The times
		 * must be converted to micro seconds.
		 */
		if (it.it_value.tv_sec > 0 && it.it_value.tv_usec < 0) {
			it.it_value.tv_sec--;
			/* 1000000 usec == 1 sec */
			it.it_value.tv_usec += 1000000;
		}
		ASSERT(it.it_value.tv_sec > 0 || it.it_value.tv_usec > 0);
		err = __setitimer(ITIMER_REAL, &it, NULL);
		if (err) printf("err = %d\n", err);
		ASSERT(err == 0);
	}
}

/*
 * co_timerset() represents a bound thread that is invisible to
 * the user. this thread is in charge of setting the the realtime
 * interval timer and running _callin() on those callouts that have
 * transpired.
 */
static struct sigaction __co_sigaction = {SA_RESTART, _callin, 0};

static void
_co_timerset(void *arg)
{
	struct timeval clocktime;
	callo_t cop;
	int calltimer;

	__sighandler_lock();
	if (__alarm_sigaction.sa_handler == SIG_DFL ||
	    __alarm_sigaction.sa_handler == SIG_IGN) {
		_sigfillset(&__co_sigaction.sa_mask);
		if (_setsighandler(SIGALRM, &__co_sigaction, NULL) == -1) {
			__sighandler_unlock();
			_panic("co_timerset, unable to install signal hndlr");
		}
	}
	__sighandler_unlock();
	maskallsigs(&curthread->t_hold);
	_sigdelset(&curthread->t_hold, SIGALRM);
	/*
	 * should call __sigprocmask() here - calling thr_sigsetmask() here
	 * could mean losing an undirected signal forever since thr_sigsetmask
	 * is now a fast call which will not set the lwp's mask and relies
	 * on subsequent unblocking to get any pending signals.
	 */
	__sigprocmask(SIG_BLOCK, &curthread->t_hold, NULL);
	while (1) {
		/*
		 * Push down the thread mask here to the LWP.
		 */
		__sigprocmask(SIG_UNBLOCK, &_alrmmask, NULL);
		_lwp_sema_wait(&_settimer);
		__sigprocmask(SIG_BLOCK, &_alrmmask, NULL);
		/*
		 * Block SIGALRM here on the LWP. Do not rely on
		 * thr_sigsetmask() - this is a thread which relies on
		 * synchronous delivery of SIGALRM to itself - so need
		 * to push down the mask everytime. Use __sigprocmask().
		 */
		calltimer = 0;
		_lmutex_lock(&_calloutlock);
		if (_calloutp != NULL) {
			cop = *_calloutp;
			calltimer = 1;
		}
		_lmutex_unlock(&_calloutlock);
		if (calltimer) {
			_gettimeofday(&clocktime, NULL);
			_setrealitimer(&clocktime, &(cop.time));
		}
	}
}

static void
_co_enable()
{
	int err, ix;
	uthread_t *t;

	_sigoff();
	err = _thr_create(NULL, 0, (void *(*)(void *))_co_timerset, NULL,
	    THR_SUSPENDED|THR_BOUND|THR_DAEMON, &_co_tid);
	if (err) {
		printf("co_enable, thr_create() returned error = %d\n",
		    errno);
		_panic("co_enable failed");
	}
	_lock_bucket((ix = HASH_TID(_co_tid)));
	t = _idtot(_co_tid);
	_unlock_bucket(ix);
	t->t_flag |= T_INTERNAL;
	t->t_stop = TSTP_INTERNAL;	/* for __thr_continue() */
	__thr_continue(_co_tid);
	_sched_lock();
	--_totalthreads;
	_sched_unlock();
	_sigon();
}

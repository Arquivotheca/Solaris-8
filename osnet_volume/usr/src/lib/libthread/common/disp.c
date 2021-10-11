/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)disp.c	1.119	99/12/03 SMI"

#include <stdio.h>

#ifdef DEBUGT
int _debugt = 1;
#endif

#include "libthread.h"
#include <tdb_agent.h>

#include <sys/reg.h>

/*
 * Global variables
 */
lwp_mutex_t _schedlock;		/* protects runqs and sleepqs */
				/* protected by sched_lock() */
dispq_t _dispq[DISPQ_SIZE];	/* the queue of runnable threads */
u_int 	_dqactmap[MAXRUNWORD];	/* bit map of priority queues */
int 	_nrunnable = 0;		/* number of threads on the run queue */
int 	_maxpriq = THREAD_MIN_PRIORITY-1; /* index of highest priority dispq */
dispq_t	_sdispq[DISPQ_SIZE];	/* the suspended queue of runnable threads */
u_int	_sdqactmap[MAXRUNWORD];	/* map of suspended runnable threads */
int	_srunnable = 0;		/* number of threads on suspended run queue */
int 	_smaxpriq = THREAD_MIN_PRIORITY-1; /* highest suspended runq */
int 	_minpri = THREAD_MIN_PRIORITY; /* value of lowest priority thread */
int 	_nthreads = 0;		/* number of unbound threads */
int 	_totalthreads = 0;	/* total number of threads */
int 	_userthreads = 0;	/* number of user created threads */
int 	_u2bzombies = 0;	/* u threads on their way 2b zombies */
int 	_d2bzombies = 0;	/* dameon ths on their way 2b zombies */
uthread_t *_nidle = NULL;	/* list of idling threads */
int 	_nagewakecnt = 0;	/* number of awakened aging threads */
int	_naging = 0;		/* number of aging threads running */
lwp_cond_t _aging;		/* condition on which threads age */
int 	_nlwps = 0;		/* number of lwps in the pool */
int 	_minlwps = 1;		/* min number of lwps in pool */
int 	_ndie = 0;		/* number of lwps to delete from pool */
int	_nidlecnt = 0;
int	_onprocq_size = 1;
int	_mypid;			/* my process-id */

/*
 * Static variables
 */
static	timestruc_t _idletime = {60*5, 0}; /* age for 5 minutes then destroy */

/*
 * Static functions
 */
static	void _park(uthread_t *t);
static	void _idle_deq(uthread_t *t);
static	uthread_t *_disp(void);
static	void _dispenq(uthread_t *, dispq_t *, int, u_int *);
static 	void _swap_dispqs(dispq_t *, u_int *, int *, int *,
			    dispq_t *, u_int *, int *, int *);
static	int _dispdeq(uthread_t *, dispq_t *, u_int *, int *, int *);
#ifdef DEBUG
int _onrunq(uthread_t *t);
static int _isrunqok(dispq_t *, u_int *, int);
#endif /* DEBUG */

/*
 * quick swtch(). switch to a new thread without saving the
 * current thread's state.
 */
void
_qswtch(void)
{
	struct thread *next;
	uthread_t *t = curthread;
	uintptr_t sp;
#ifdef DEBUG
	sigset_t lwpmask;
#endif

	/*
	 * While the LWP is switching, mark the thread to indicate that the
	 * LWP is switching threads. This flag is used to prevent SIGPROF
	 * processing while the LWP is switching: the global signal handler
	 * drops SIGPROF if it occurs on the LWP while it is switching (see
	 * sigacthandler() in sys/common/sigaction.c). SIGPROF should not be
	 * deferred while switching if it occurs after the switching LWP has
	 * checked for it, and is committed to switching to a new thread.
	 * Otherwise, signal deferral will cause the underlying LWP to have
	 * all signals blocked, when it switches to the new thread, delaying
	 * (possibly for a long time) all future receptions of signals by this
	 * thread.
	 *
	 * XXX: Eventually, a better fix should be adopted so that the LWP CAN
	 * defer SIGPROF during a thread switch, even *after* the LWP has
	 * committed to the new thread, and deliver the SIGPROF to the new
	 * thread instead - it should not matter which thread gets the SIGPROF
	 * as long as some thread gets it, since SIGPROF is really a per-LWP
	 * signal. Such a fix can also be used to solve the similar problem for
	 * SIGEMT, which is also a per-LWP signal like SIGPROF, but which can
	 * never be dropped in this manner. This fix may require introducing a
	 * LWP-specific data structure similar to struct thread, where this
	 * signal could be stored while it is deferred during a thread switch.
	 */

	t->t_ulflag |= T_SWITCHING;

	/*
	 * All subsequent occurrences of SIGPROF will be dropped, while the
	 * T_SWITCHING flag is set. However, here is a scenario which needs
	 * to be handled:
	 *
	 * If a SIGPROF occurs (just) before the above setting of the flag, it
	 * would be stored in curthread->t_sig and the underlying LWP's signal
	 * mask would be masking all signals. What should we do about the LWP's
	 * signal mask, and the deferred signal?
	 *
	 * Now, _qswtch() is called either by
	 *
	 *	   (A) a dying thread
	 *	OR (B) an idling LWP
	 *
	 * (A) If called from a dying thread, the LWP is committed to switching
	 * to a different thread: either to the idle thread, or a runnable
	 * thread picked from the dispatch queue, returned by _disp(). Since it
	 * is committed to switching, the deferred signal can be discarded,
	 * and the underlying LWP's signal mask can be cleared, since the dying
	 * thread is never going to call _sigon() to process the deferred signal
	 * and the new thread should not run on an LWP with all signals blocked.
	 * So, discard the deferred signal and clear the LWP mask
	 * without bothering to clean-up the stored siginfo, from the yielding
	 * thread, since it is dying. In effect, the SIGPROF is dropped.
	 *
	 * This is different from _swtch(), where, if the same scenario occurs
	 * (SIGPROF deferral occurs just before T_SWITCHING is set), the LWP
	 * will not switch (but park or return), because of the T_TEMPBOUND
	 * flag setting (set if a signal has been deferred in t_sig), in which
	 * case such mask clearing is not necessary. When the switching thread
	 * returns from _swtch(), the deferred signal will be processed when
	 * the thread makes its outermost call to _sigon().
	 *
	 * (B) If _qswtch() is called from an idling LWP, this scenario is
	 * impossible since SIGPROF is dropped on occurrence, for idling
	 * threads, in sigacthandler(). Hence nothing has to be done for
	 * calls to _qswtch() from idle threads.
	 *
	 * The following code checks for this scenario by inspecting t_sig, and
	 * clears the LWP mask if necessary.
	 */

	if (t->t_sig == SIGPROF) {
		ASSERT(!IDLETHREAD(t));
		ASSERT(__sigprocmask(SIG_BLOCK, NULL, &lwpmask) != -1 &&
		    !sigcmpset(&lwpmask, &_totalmasked));
		t->t_sig = 0;
		t->t_flag &= ~T_TEMPBOUND;
		__sigprocmask(SIG_SETMASK, &_null_sigset, NULL);
	}

	ASSERT(MUTEX_HELD(&_schedlock));

	ASSERT(LWPID(t) != 0);
	next = _disp();
	ASSERT(MUTEX_HELD(&_schedlock));
	if (IDLETHREAD(next)) {
		if (_ndie) {
			_ndie--;
			_sched_unlock_nosig();
			_lwp_exit();
		}
		if (t == next) {
			t->t_ulflag &= ~T_SWITCHING;
			return;
		}
		/*
		 * XXX Fix this. Cannot return if _qswtch() called from
		 * thr_exit()
		 */
		_thread_ret(next, _age);
		next->t_state = TS_DISP;
	}
	if (__tdb_stats_enabled)
		_tdb_update_stats();
	_sc_switch(next);
	_sched_unlock_nosig();
	_sigoff();
	sp = next->t_idle->t_sp;
	ASSERT(next->t_state == TS_DISP);
	ITRACE_0(UTR_FAC_TLIB_SWTCH, UTR_RESUME_START, "_resume start");
	_resume(next, (caddr_t)sp, 1);
	_panic("_qswtch: resume returned");
}

/*
 * when there are more lwps in the pool than there are
 * threads, the idling thread will hang around aging on
 * a timed condition wait. If the idling thread is awakened
 * due to the realtime alarm, then the idling thread and its
 * lwp are deallocated.
 */
void
_age(void)
{
	uthread_t *t = curthread;
	timestruc_t ts;
	int err = 0;

	/*
	ITRACE_1(TR_FAC_SYS_LWP, TR_SYS_LWP_CREATE_END2,
	    "lwp_create end2:lwpid 0x%x", lwp_self());
	TRACE_0(UTR_FAC_TRACE, UTR_THR_LWP_MAP, "dummy for thr_lwp mapping");
	*/
	ts.tv_nsec = 0;
	_sched_lock_nosig();
	/*
	 * Door server threads don't count as lwps available to run
	 * unbound threads until they're activated.
	 */
	if (t->t_flag & T_DOORSERVER) {
		_nlwps++;
		t->t_flag &= ~T_DOORSERVER;
	}
	while (1) {
		_naging++;
		while (!_nrunnable) {
			ts.tv_sec = time(NULL) + _idletime.tv_sec;
			err = _lwp_cond_timedwait(&_aging, &_schedlock, &ts);
			if (_nagewakecnt > 0) {
				_nagewakecnt--;
				continue;
			}
			ASSERT(err != EFAULT);

			if ((err == ETIME || err == EAGAIN || _ndie) &&
				(_naging > 1)) {
				_nlwps--;
				if (_ndie > 0)
					_ndie--;
				_naging--;
				_sched_unlock_nosig();
				ASSERT(ISBOUND(curthread));
				_thr_exit(0);
			}
		}
		_naging--;
		_qswtch();
	}
}


static void
_idle_deq(uthread_t *t)
{
	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(_nidlecnt > 0);

	t->t_flag &= ~T_IDLE;
	if (--_nidlecnt > 0) {
		t->t_ibackw->t_iforw = t->t_iforw;
		t->t_iforw->t_ibackw = t->t_ibackw;
		if (_nidle == t)
			_nidle = t->t_iforw;
	} else
		_nidle = NULL;
	t->t_iforw = t->t_ibackw = NULL;
}

static void
_idle_enq(uthread_t *t)
{
	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(!ISBOUND(t));
	ASSERT((t->t_iforw == t->t_ibackw) && t->t_iforw == NULL);

	/* temp trace point to benchmark lwp create */
	/*
	ITRACE_1(TR_FAC_SYS_LWP, TR_SYS_LWP_CREATE_END2,
	    "lwp_create end2:lwpid 0x%x", lwp_self());
	*/
	t->t_flag |= T_IDLE;
	_nidlecnt++;
	if (_nidle == NULL) {
		_nidle = t;
		t->t_iforw = t->t_ibackw = t;
	} else {
		_nidle->t_ibackw->t_iforw = t;
		t->t_iforw = _nidle;
		t->t_ibackw = _nidle->t_ibackw;
		_nidle->t_ibackw = t;
	}
}


void
_onproc_deq(uthread_t *t)
{

	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(_onprocq_size > 0);

	if (--_onprocq_size > 0) {
		t->t_backw->t_forw = t->t_forw;
		t->t_forw->t_backw = t->t_backw;
		if (_onprocq == t)
			_onprocq = t->t_forw;
	} else
		_onprocq = NULL;
	t->t_forw = t->t_backw = NULL;
}

void
_onproc_enq(uthread_t *t)
{

	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(!ISBOUND(t));
	ASSERT(t->t_link == NULL);
	ASSERT((t->t_forw == t->t_backw) && t->t_forw == NULL);
	ASSERT(t->t_state == TS_ONPROC || t->t_state == TS_DISP);

	_onprocq_size++;
	if (_onprocq == NULL) {
		_onprocq = t;
		t->t_forw = t->t_backw = t;
	} else {
		_onprocq->t_backw->t_forw = t;
		t->t_forw = _onprocq;
		t->t_backw = _onprocq->t_backw;
		_onprocq->t_backw = t;
	}
}

#ifdef DEBUG
int
_onprocq_consistent(void)
{
	uthread_t *x;
	int cnt, v;

	_sched_lock();
	x = _onprocq;
	if (x == NULL)
		cnt = 0;
	else {
		cnt = 1;
		while ((x = x->t_forw) != _onprocq)
			cnt++;
	}
	if (cnt == _onprocq_size)
		v = 1;
	else
		v = 0;
	_sched_unlock();
	return (v);
}
#endif

void
_swtch(int dontsave)
{
	struct thread *t = curthread;
	struct thread *next;
	uintptr_t sp;
	int ret;
#ifdef DEBUG
	sigset_t lwpmask;
#endif

	ITRACE_0(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_START, "_swtch start");
	/*
	 * It is impossible, given the signal delivery mechanism, for a thread
	 * to have received a signal while in the libthread critical section
	 * and to then switch. Hence the following ASSERT.
	 */
	_sched_lock();
	/*
	 * See Big Comment in _qswtch(), documenting the setting of the
	 * T_SWITCHING flag.
	 */
	t->t_ulflag |= T_SWITCHING;
again:
	ASSERT(t->t_state == TS_SLEEP || t->t_state == TS_DISP ||
	    t->t_state == TS_RUN || t->t_state == TS_ONPROC ||
	    t->t_state == TS_STOPPED);
	/*
	 * All calls to _swtch() are made with t_nosig at least 1. And the
	 * above call to _sched_lock() increments by another 1. Hence the
	 * following ASSERT.
	 */
	ASSERT(t->t_nosig >= 2);
	if (t->t_state == TS_SLEEP) {
		/*
		 * check t_ssig, T_BSSIG and t_sig. If any one of these is
		 * non-null, interrupt the thread going to sleep and
		 * return.
		 */
		if ((!sigisempty(&t->t_ssig)) ||
		    ((t->t_flag & T_BSSIG) | t->t_sig) ||
		    ((t->t_flag & T_SIGWAIT) &&
		    (t->t_pending != 0 || t->t_bdirpend != 0))) {
			ASSERT(sigisempty(&t->t_bsig) ||
			    (t->t_flag & T_BSSIG) ||
			    (t->t_flag & T_SIGWAIT));
			_unsleep(t);
			t->t_flag |= T_INTR;
			if (t->t_stop)
				_setrq(t);
			else {
				t->t_state = TS_ONPROC;
				if (!ISBOUND(t))
					_onproc_enq(t);
				_sched_unlock();
				ASSERT((t->t_sig != 0) ||
				    (__sigprocmask(
				    SIG_BLOCK, NULL, &lwpmask) != -1 &&
				    lwpmask.__sigbits[0] == 0 &&
				    lwpmask.__sigbits[1] == 0 &&
				    lwpmask.__sigbits[2] == 0 &&
				    lwpmask.__sigbits[3] == 0));
				t->t_ulflag &= ~T_SWITCHING;
				ITRACE_1(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_END,
				    "_swtch end:end %d", 2);
				return;
			}
		}
		if (ISBOUND(t) || ISTEMPBOUND(t) || ISTEMPRTBOUND(t)) {
			_park(t);
			/*
			* In the following, do not check the underlying
			* LWP mask if ISTEMPBOUND - this implies that
			* the thread is in the process of running a
			* user-signal-handler and so its LWP mask will
			* be cleared eventually before it switches to
			* another thread.
			*/
			ASSERT((t->t_sig != 0) ||
			    ISTEMPBOUND(t) ||
			    (t->t_tid == __dynamic_tid) ||
			    (t->t_tid == _co_tid) ||
			    (__sigprocmask(
			    SIG_BLOCK, NULL, &lwpmask) != -1 &&
			    lwpmask.__sigbits[0] == 0 &&
			    lwpmask.__sigbits[1] == 0 &&
			    lwpmask.__sigbits[2] == 0 &&
			    lwpmask.__sigbits[3] == 0));
			t->t_ulflag &= ~T_SWITCHING;
			ITRACE_1(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_END,
			    "_swtch end:end %d", 1);
			return;
		}
	} else if (t->t_stop && (t->t_state == TS_DISP ||
	    t->t_state == TS_ONPROC || ISBOUND(t) || ISTEMPBOUND(t) ||
	    ISTEMPRTBOUND(t))) {
		ASSERT(!ISBOUND(t) || t->t_state == TS_STOPPED);
		if (!ISBOUND(t) && ONPROCQ(t)) {
			ASSERT(t->t_state == TS_DISP);
			_onproc_deq(t);
		}
		t->t_state = TS_STOPPED;
		ASSERT((t->t_stop & TSTP_MUTATOR)? (t->t_mutator): 1);
		if (t->t_sig != 0 || ISTEMPBOUND(t) || (t->t_flag & T_BSSIG) ||
		    ISBOUND(t) || ISTEMPRTBOUND(t)) {
			_park(t);
			ASSERT(t->t_state == TS_DISP ||
			    t->t_state == TS_ONPROC);
			t->t_state = TS_ONPROC;
			/*
			 * For following ASSERT, see comment above under the
			 * preceding call to _park().
			 */
			ASSERT((t->t_sig != 0) ||
			    ISTEMPBOUND(t) ||
			    (__sigprocmask(SIG_BLOCK, NULL, &lwpmask) != -1 &&
			    lwpmask.__sigbits[0] == 0 &&
			    lwpmask.__sigbits[1] == 0 &&
			    lwpmask.__sigbits[2] == 0 &&
			    lwpmask.__sigbits[3] == 0));
			t->t_ulflag &= ~T_SWITCHING;
			ITRACE_1(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_END,
			    "_swtch end:end %d", 8);
			return;
		}
	} else {
		if (t->t_state == TS_ONPROC) {
			ASSERT((_suspendingallmutators | _suspendedallmutators)
			    ? (!t->t_mutator ||
			    !(t->t_mutator ^ t->t_mutatormask))
			    : 1);
			ASSERT(ISBOUND(t) || ONPROCQ(t));
			_sched_unlock();
			ASSERT((t->t_sig != 0) ||
			    (t->t_tid == __dynamic_tid) ||
			    (t->t_tid == _co_tid) ||
			    (__sigprocmask(SIG_BLOCK, NULL, &lwpmask) != -1 &&
			    lwpmask.__sigbits[0] == 0 &&
			    lwpmask.__sigbits[1] == 0 &&
			    lwpmask.__sigbits[2] == 0 &&
			    lwpmask.__sigbits[3] == 0));
			t->t_ulflag &= ~T_SWITCHING;
			ITRACE_1(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_END,
			    "_swtch end:end %d", 3);
			return;
		}
		if (t->t_state == TS_DISP) {
			ASSERT(ONPROCQ(t));
			ASSERT(!ISBOUND(t));
			ASSERT((_suspendingallmutators | _suspendedallmutators)
			    ? (!t->t_mutator ||
			    !(t->t_mutator ^ t->t_mutatormask))
			    : 1);
			t->t_state = TS_ONPROC;
			_sched_unlock();
			if (__td_event_report(t, TD_READY)) {
				t->t_td_evbuf.eventnum = TD_READY;
				tdb_event_ready();
			}
			ASSERT((t->t_sig != 0) ||
			    (__sigprocmask(
			    SIG_BLOCK, NULL, &lwpmask) != -1 &&
			    lwpmask.__sigbits[0] == 0 &&
			    lwpmask.__sigbits[1] == 0 &&
			    lwpmask.__sigbits[2] == 0 &&
			    lwpmask.__sigbits[3] == 0));
			t->t_ulflag &= ~T_SWITCHING;
			ITRACE_1(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_END,
			    "_swtch end:end %d", 4);
			return;
		} else {
			ASSERT(t->t_state == TS_RUN ||
			    t->t_state == TS_STOPPED);
			/*
			* Again, as in the TS_SLEEP case, check if t_ssig,
			* T_BSSIG or t_sig is non-NULL. Since this implies
			* that the thread cannot switch, since it is in the
			* run state, simply put it back to the ONPROC state
			* and return on the same LWP after deleting it from
			* the run queue.
			*/
			if (t->t_state == TS_RUN && (!sigisempty(&t->t_ssig) ||
			    ((t->t_flag & T_BSSIG) || ISTEMPBOUND(t) ||
			    ISTEMPRTBOUND(t)))) {
				if (_suspendingallmutators |
				    _suspendedallmutators) {
					if (_srqdeq(t))
						_panic("_swtch: not on srunq");
					t->t_stop |= TSTP_MUTATOR;
				} else if (_rqdeq(t))
					_panic("_swtch: not on runq");
				t->t_state = TS_ONPROC;
				if (!ISBOUND(t))
					_onproc_enq(t);
				_sched_unlock();
				ASSERT((t->t_sig != 0) ||
				    (__sigprocmask(
				    SIG_BLOCK, NULL, &lwpmask) != -1 &&
				    lwpmask.__sigbits[0] == 0 &&
				    lwpmask.__sigbits[1] == 0 &&
				    lwpmask.__sigbits[2] == 0 &&
				    lwpmask.__sigbits[3] == 0));
				t->t_ulflag &= ~T_SWITCHING;
				ITRACE_1(UTR_FAC_TLIB_SWTCH,
				    UTR_SWTCH_END,
				    "_swtch end:end %d", 9);
				return;
			}
		}
	}
	ASSERT(t->t_state == TS_SLEEP || t->t_state == TS_RUN ||
	    t->t_state == TS_STOPPED);
	/*
	 * thread's preemption flag is cleared since thread is really
	 * surrendering the "processor" to another thread.
	 */
	t->t_preempt = 0;
	t->t_flag &= ~T_PREEMPT;
	t->t_flag |= T_OFFPROC;
	next = _disp();

	if (IDLETHREAD(next)) {
		ASSERT(!ONPROCQ(t));
		t->t_flag &= ~T_OFFPROC;
		_idle_enq(t);
		ITRACE_1(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_END,
		    "_swtch end:end %d", 5);
		_park(t);
		_sched_lock();
		if (ON_IDLE_Q(t))
			_idle_deq(t);
		ITRACE_0(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_START, "_swtch start");
		goto again;
	}
	ASSERT(next->t_link == NULL);
	ASSERT(next->t_state == TS_DISP);
	ASSERT((next->t_flag & T_OFFPROC) == 0);
	ASSERT(t->t_nosig > 0);
	if (next != t) {
		ASSERT((t->t_stop & TSTP_MUTATOR)? (t->t_mutator): 1);
		sp = next->t_idle->t_sp;
		t->t_fp = _manifest_thread_state();
		if ((_suspendingallmutators | _suspendedallmutators) &&
		    t->t_mutator) {
			t->t_stop &= ~TSTP_MUTATOR;
			if (t->t_stop & TSTP_ALLMUTATORS) {
				t->t_stop &= ~TSTP_ALLMUTATORS;
				_samcnt--;
				if (!_samcnt)
					_lwp_cond_signal(&_samcv);
			}
			if (t->t_state == TS_STOPPED) {
				_setsrq(t);
				t->t_ssflg = 1;
				_lwp_cond_broadcast(&t->t_suspndcv);
			}
			ASSERT(t->t_state != TS_STOPPED);
		} else {
			if (t->t_stop)
				_lwp_cond_broadcast(&t->t_suspndcv);
		}
		_sc_switch(next);
		_sched_unlock_nosig();
		ASSERT((t->t_sig != 0) ||
		    (__sigprocmask(SIG_BLOCK, NULL, &lwpmask) != -1 &&
		    lwpmask.__sigbits[0] == 0 &&
		    lwpmask.__sigbits[1] == 0 &&
		    lwpmask.__sigbits[2] == 0 &&
		    lwpmask.__sigbits[3] == 0));
		ITRACE_1(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_END,
		    "_swtch end:end %d", 6);
		ITRACE_0(UTR_FAC_TLIB_SWTCH, UTR_RESUME_START, "_resume start");
		if (__tdb_stats_enabled)
			_tdb_update_stats();
		_resume(next, (caddr_t)sp, dontsave);
		ASSERT((curthread->t_sig != 0) ||
		    (__sigprocmask(SIG_BLOCK, NULL, &lwpmask) != -1 &&
		    lwpmask.__sigbits[0] == 0 &&
		    lwpmask.__sigbits[1] == 0 &&
		    lwpmask.__sigbits[2] == 0 &&
		    lwpmask.__sigbits[3] == 0));

	} else {
		ASSERT((t->t_mutator ^ t->t_mutatormask) ?
		    !(_suspendingallmutators | _suspendedallmutators) : 1);
		t->t_state = TS_ONPROC;
		ASSERT(ONPROCQ(t));
		_sched_unlock();
		ASSERT((t->t_sig != 0) ||
		    (__sigprocmask(SIG_BLOCK, NULL, &lwpmask) != -1 &&
		    lwpmask.__sigbits[0] == 0 &&
		    lwpmask.__sigbits[1] == 0 &&
		    lwpmask.__sigbits[2] == 0 &&
		    lwpmask.__sigbits[3] == 0));
		t->t_ulflag &= ~T_SWITCHING;
		ITRACE_1(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_END,
		    "_swtch end:end %d", 7);
	}
	ASSERT(MUTEX_HELD(&curthread->t_lock));
	ASSERT(curthread->t_state == TS_ONPROC);
}

/*
 * Initialize the dispatcher queues.
 */
void
_dispinit(void)
{
	int i;

	for (i = 0; i < DISPQ_SIZE; i++)
		_dispq[i].dq_first = _dispq[i].dq_last = NULL;
}

/*
 * Park a thread inside the kernel. The thread is bound to its
 * LWP until it is awakened. A park thread may wake up due to
 * signals. The caller must verify on return from this call whether
 * or not the thread was unparked.
 */
static void
_park(uthread_t *t)
{
	int err = 0;

	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(ISBOUND(t) || ISTEMPRTBOUND(t) || OFFPROCQ(t));
	ASSERT(t->t_nosig > 0);

	ITRACE_0(UTR_FAC_TLIB_SWTCH, UTR_PARK_START, "_park start");
	t->t_flag |= T_PARKED;
	t->t_fp = _manifest_thread_state();
	if ((_suspendingallmutators | _suspendedallmutators) &&
	    t->t_mutator) {
		if (t->t_stop & TSTP_ALLMUTATORS) {
			t->t_stop &= ~TSTP_ALLMUTATORS;
			_samcnt--;
			if (!_samcnt)
				_lwp_cond_signal(&_samcv);
		}
	}
	if (t->t_stop)
		_lwp_cond_broadcast(&t->t_suspndcv);
	while (ISPARKED(t)) {
		_sched_unlock_nosig();
		/*
		 * interrupt the parking thread if signaled while
		 * asleep or when _lwp_sema_wait() returns EINTR.
		 */
		if (err == EINTR) {
			_sched_lock_nosig();
			if (t->t_state == TS_SLEEP) {
				t->t_flag |= T_INTR;
				err = EINTR;
				_unsleep(t);
				_setrq(t);
			}
			_sched_unlock_nosig();
		}
		err = _lwp_sema_wait(&t->t_park);
		_sched_lock_nosig();
	}
	ASSERT(!ISPARKED(t));
	_sched_unlock();
	ITRACE_0(UTR_FAC_TLIB_SWTCH, UTR_PARK_END, "_park end");
}

/*
 * Wake up a parked thread.
 */
void
_unpark(uthread_t *t)
{
	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(ISPARKED(t));
	/*
	 * The thread can either be bound, or idle, or stuck to this LWP and
	 * hence parked because of pending signal action.
	 */
	ASSERT(ISBOUND(t) || ON_IDLE_Q(t) || ISTEMPBOUND(t) ||
	    ISTEMPRTBOUND(t) || t->t_sig != 0 || ((t->t_flag & T_BSSIG) != 0));
	ITRACE_1(UTR_FAC_TLIB_SWTCH, UTR_UNPARK_START,
	    "_unpark start:thread ptr = 0x%p", t);
	if (ON_IDLE_Q(t))
		_idle_deq(t);
	t->t_flag &= ~T_PARKED;
	_lwp_sema_post(&t->t_park);
	ITRACE_0(UTR_FAC_TLIB_SWTCH, UTR_UNPARK_END, "_unpark end");
}

#ifdef DEBUG
static int
_isrunqok(dispq_t *dq, u_int *dqactmap, int nrunnable)
{
	int i, runqcnt = 0;
	uthread_t *t;

	for (i = 0; i < DISPQ_SIZE; i++) {
		if (dq[i].dq_first) {
			ASSERT((dqactmap[i/BPW] & (1<<(i & (BPW-1)))));
			for (t = dq[i].dq_first; t != NULL; t = t->t_link)
				runqcnt++;
		}
	}
	ASSERT(runqcnt == nrunnable);
	return (0);
}

/*
 * Purely for assertion purposes. Asserts that the thread
 * is on the run queue. Returns 0 on failure, 1 on success.
 */
int
_onrunq(uthread_t *t)
{
	int qx;
	uthread_t *qt;

	ASSERT(MUTEX_HELD(&_schedlock));
	HASH_PRIORITY(DISP_PRIO(t), qx);
	if (t->t_link == NULL) {
		if (_dispq[qx].dq_first == t || _dispq[qx].dq_last == t)
			return (1); /* success */
		return (0);
	}
	for (qt = _dispq[qx].dq_first; qt != NULL; qt = qt->t_link) {
		if (qt == t)
			return (1); /* success */
	}
	return (0); /* failure */
}

#endif /* DEBUG */

static void
_dispenq(uthread_t *t, dispq_t *dq, int qx, u_int *dqactmap)
{
	ASSERT(MUTEX_HELD(&_schedlock));
	if (dq->dq_last == NULL) {
		/* queue empty */
		dq->dq_first = dq->dq_last = t;
		dqactmap[(qx/BPW)] |= 1<<(qx & (BPW-1));
	} else {
		/* add to end of q */
		if (qx < DISPQ_SIZE) {
			dq->dq_last->t_link = t;
			dq->dq_last = t;
		}
#ifdef OUT
		else {
			/*
			 * priorities outside normal range map to last
			 * priority bucket and are inserted in priority
			 * order.
			 */
			register struct thread *prev, *next;

			prev = (struct thread *)dq;
			for (next = dq->dq_first; next != NULL &&
				next->t_pri >= tpri; next = next->t_link)
						prev = next;
			if (prev == (struct thread *)dq) {
				t->t_link = dq->dq_first;
				dq->dq_first = t;
			} else {
				t->t_link = prev->t_link;
				prev->t_link = t;
			}
		}
#endif
	}
}

void
_setrq(uthread_t *t)
{
	uthread_t **pt, *nt;
	int preemptflag = 0;
	int qx;

	ITRACE_4(UTR_FAC_TLIB_DISP, UTR_SETRQ_START,
	    "_setrq start:tid 0x%x pri %d usropts 0x%x _maxpriq %d",
	    t->t_tid, t->t_pri, t->t_usropts, _maxpriq);
	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(t->t_link == NULL && t->t_wchan == NULL);
	ASSERT(!_onrunq(t));

	if ((_suspendingallmutators | _suspendedallmutators) &&
	    (t->t_mutator ^ t->t_mutatormask)) {
		/*
		 * when all mutators are being suspended, any
		 * mutator thread that is made runnable, because
		 * it was awakened, or preempted, should be
		 * moved to the suspended runq. if the mutator
		 * is parked or is bound, then it is either
		 * asleep or suspended, and should transition
		 * to suspended if it is asleep..
		 */
		ASSERT(t->t_mutator);
		if (ISPARKED(t) || ISBOUND(t)) {
			ASSERT(t->t_state == TS_SLEEP ||
			    t->t_state == TS_STOPPED);
			ASSERT(t->t_stop & TSTP_MUTATOR);
			if (t->t_state == TS_SLEEP)
				t->t_state = TS_STOPPED;
		} else {
			ASSERT(t->t_state == TS_SLEEP ||
			    t->t_state == TS_ONPROC ||
			    t->t_state == TS_STOPPED);
			t->t_stop &= ~TSTP_MUTATOR;
			_setsrq(t);
		}
		return;
	}
	if (t->t_stop) {
		t->t_state = TS_STOPPED;
		/*
		 * If this is a bound thread, it will be unparked,
		 * if necessay, via the eventual call to thr_continue().
		 */
		return;
	}
	if (ISPARKED(t)) {
		if (ISBOUND(t) || ISTEMPRTBOUND(t) || ISTEMPBOUND(t)) {
			if (__td_event_report(t, TD_READY)) {
				t->t_td_evbuf.eventnum = TD_READY;
				tdb_event_ready();
			}
			t->t_state = TS_ONPROC;
		} else {
			ASSERT(OFFPROCQ(t));
			t->t_state = TS_DISP;
			_onproc_enq(t);
		}
		_unpark(t);
		ITRACE_4(UTR_FAC_TLIB_DISP, UTR_SETRQ_END,
		    "_setrq end:tid 0x%x pri %d usropts 0x%x _maxpriq %d",
		    t->t_tid, t->t_pri, t->t_usropts, _maxpriq);
		return;
	} else if (ISBOUND(t) || ISTEMPRTBOUND(t) || ISTEMPBOUND(t)) {
		t->t_state = TS_ONPROC;
		if (__td_event_report(t, TD_READY)) {
			t->t_td_evbuf.eventnum = TD_READY;
			tdb_event_ready();
		}
		return;
	}
	ASSERT((t->t_forw == t->t_backw) && t->t_forw == NULL);
	t->t_state = TS_RUN;
	if (__td_event_report(t, TD_READY)) {
		t->t_td_evbuf.eventnum = TD_READY;
		tdb_event_ready();
	}
	HASH_PRIORITY(DISP_PRIO(t), qx);
	if (qx > _maxpriq) {
		_maxpriq = qx;
		if (_maxpriq > _minpri)
			preemptflag = 1;
	}
	_dispenq(t, &_dispq[qx], qx, _dqactmap);
	_nrunnable++;
	ASSERT(_isrunqok(_dispq, _dqactmap, _nrunnable) == 0);
	if (_nidle) {
		_unpark(_nidle);
	} else if (_nagewakecnt < _naging) {
		_nagewakecnt++;
		_lwp_cond_signal(&_aging);
	} else {
		if (!_sigwaitingset)
			_sigwaiting_enabled();
		if (preemptflag)
			_preempt(NULL, DISP_PRIO(t));
	}
	ITRACE_4(UTR_FAC_TLIB_DISP, UTR_SETRQ_END,
	    "_setrq end:tid 0x%x pri %d usropts 0x%x _maxpriq %d",
	    t->t_tid, t->t_pri, t->t_usropts, _maxpriq);
}

void
_setsrq(uthread_t *t)
{
	int qx;

	ASSERT(MUTEX_HELD(&_schedlock));
	t->t_state = TS_RUN;
	HASH_PRIORITY(DISP_PRIO(t), qx);
	if (qx > _smaxpriq)
		_smaxpriq = qx;
	_srunnable++;
	_dispenq(t, &_sdispq[qx], qx, _sdqactmap);
	ASSERT(_isrunqok(_sdispq, _sdqactmap, _srunnable) == 0);
}

/*
 * swap two disp queues and their associated dispatcher state.
 * the dq* parameters are swapped with the sdq* parameters.
 */
static void
_swap_dispqs(dispq_t *dq, u_int *dqactmap, int *maxpriq, int *nrunnable,
	    dispq_t *sdq, u_int *sdqactmap, int *smaxpriq, int *snrunnable)
{
	int bitmap;
	int runword;
	int qx;
	int hb;
	int omaxpriq;
#ifdef DEBUG
	int oruncnt = *nrunnable;
#endif

	ASSERT(_isrunqok(sdq, sdqactmap, *snrunnable) == 0);
	ASSERT(_isrunqok(dq, dqactmap, *nrunnable) == 0);
	if (*smaxpriq < *maxpriq)
		*smaxpriq = *maxpriq;
	omaxpriq = *maxpriq;
	ASSERT(omaxpriq <= THREAD_MAX_PRIORITY);
	*maxpriq = -1;
	*snrunnable += *nrunnable;
	*nrunnable = 0;
	for (runword = omaxpriq/BPW; runword >= 0; runword--) {
		if (!dqactmap[runword])
			continue;
		sdqactmap[runword] |= dqactmap[runword];
		dqactmap[runword] = 0;
		bitmap = sdqactmap[runword];
		ASSERT(bitmap != 0);
		while (bitmap) {
			hb = _hibit(bitmap) - 1;
			qx = hb + BPW * runword;
			/*
			 * if dq is not empty, appended it to sdq,
			 * and if sdq is empty, move list from dq
			 * to sdq.
			 */
			if (dq[qx].dq_first) {
				ASSERT(dq[qx].dq_last->t_link == NULL);
				if (sdq[qx].dq_first) {
					ASSERT(sdq[qx].dq_last->t_link == NULL);
					sdq[qx].dq_last->t_link =
					    dq[qx].dq_first;
				} else
					sdq[qx].dq_first = dq[qx].dq_first;
				sdq[qx].dq_last = dq[qx].dq_last;
				dq[qx].dq_first = NULL;
				dq[qx].dq_last = NULL;
			}
			bitmap &= ~(1<<hb);
		}
	}
	ASSERT(_isrunqok(sdq, sdqactmap, *snrunnable) == 0);
}

void
_suspend_rq()
{
	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(_isrunqok(_dispq, _dqactmap, _nrunnable) == 0);
	if (_nrunnable)
		_swap_dispqs(_dispq, _dqactmap, &_maxpriq, &_nrunnable,
		    _sdispq, _sdqactmap, &_smaxpriq, &_srunnable);
}

void
_resume_rq()
{
	int cnt;

	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(_isrunqok(_sdispq, _sdqactmap, _srunnable) == 0);
	if (_srunnable) {
		cnt = _srunnable;
		_swap_dispqs(_sdispq, _sdqactmap, &_smaxpriq, &_srunnable,
		    _dispq, _dqactmap, &_maxpriq, &_nrunnable);
		while (cnt > 0 && _nidle) {
			_unpark(_nidle);
			cnt--;
		}
		while (cnt > 0 && (_nagewakecnt < _naging)) {
			_nagewakecnt++;
			_lwp_cond_signal(&_aging);
		}
	}
}

/*
 * Remove a thread from its dispatch queue. Returns FAILURE (int value 1)
 * when thread wasn't dispatchable.
 */
static int
_dispdeq(uthread_t *t, dispq_t *dispq, u_int *dqactmap, int *nrunnable,
	    int *maxpriq)
{
	dispq_t *dq;
	uthread_t **prev, *pt, *tx;
	int runword, bitmap, hb, qx;

	ASSERT(MUTEX_HELD(&_schedlock));

	if (t->t_state != TS_RUN)
		return (1);
	HASH_PRIORITY(DISP_PRIO(t), qx);
	dq = &dispq[qx];
	if (dq->dq_last == dq->dq_first) {
		/*
		 * There is only one thread on this priority run
		 * queue. If this thread's priority is equal to
		 * _maxpriq then _maxpriq must be re-adjusted.
		 */
		ASSERT(t == dq->dq_last);
		dq->dq_last = dq->dq_first = NULL;
		t->t_link = NULL;
		dqactmap[(qx/BPW)] &= ~(1 << (qx & (BPW-1)));
		if (--(*nrunnable) == 0)
			*maxpriq = _minpri-1;
		else {
			if (DISP_PRIO(t) == *maxpriq) {
				runword = DISP_PRIO(t)/BPW;
				for (; runword >= 0; runword--) {
					if (bitmap = dqactmap[runword]) {
						hb = _hibit(bitmap) - 1;
						*maxpriq = hb + BPW * runword;
						break;
					}
				}
			}
		}

		ASSERT(_isrunqok(dispq, dqactmap, *nrunnable) == 0);
		return (0);
	}
	prev = &dq->dq_first;
	pt = NULL;
	while (tx = *prev) {
		if (tx == t) {
			if ((*prev = tx->t_link) == NULL)
				dq->dq_last = pt;
			tx->t_link = NULL;
			--(*nrunnable);
			ASSERT(_isrunqok(dispq, dqactmap, *nrunnable) == 0);
			return (0);
		}
		pt = tx;
		prev = &tx->t_link;
	}
	return (1);
}

/*
 * remove a thread from a run queue.
 */
int
_rqdeq(uthread_t *t)
{
	return (_dispdeq(t, _dispq, _dqactmap, &_nrunnable, &_maxpriq));
}

/*
 * remove a thread from the suspended run queue.
 */
int
_srqdeq(uthread_t *t)
{
	return (_dispdeq(t, _sdispq, _sdqactmap, &_srunnable, &_smaxpriq));
}

/*
 * Return the highest priority runnable thread which needs an lwp.
 */
static uthread_t *
_disp(void)
{
	register uthread_t *t = curthread, **prev, *prev_t;
	register dispq_t *dq;
	register int runword, bitmap, qx, hb;

	ITRACE_0(UTR_FAC_TLIB_DISP, UTR_DISP_START, "_disp_start");
	ASSERT(MUTEX_HELD(&_schedlock));

	ASSERT(_isrunqok(_dispq, _dqactmap, _nrunnable) == 0);

	if (!_nrunnable)	/* if run q empty */
		goto idle;
	/*
	 * Find the runnable thread with the highest priority.
	 * _dqactmap[] is a bit map of priority queues. This
	 * loop looks through this map for the highest priority
	 * queue with runnable threads.
	 */
	for (runword = _maxpriq/BPW; runword >= 0; runword--) {
		do {
			bitmap = _dqactmap[runword];
			if ((hb = _hibit(bitmap) - 1) >= 0) {
				qx = hb + BPW * runword;
				dq = &_dispq[qx];
				t = dq->dq_first;
				dq->dq_first = t->t_link;
				t->t_link = NULL;
				if (--_nrunnable == 0) {
					dq->dq_last = NULL;
					_dqactmap[runword] &= ~(1<<hb);
					_maxpriq = _minpri-1;
					/* exit the while */
					hb = -1;
				} else if (dq->dq_first == NULL) {
					dq->dq_last = NULL;
					_dqactmap[runword] &= ~(1<<hb);
					if (qx == _maxpriq) {
						/* re-adjust _maxpriq */
						do {
							bitmap =
							    _dqactmap[runword];
							hb =
							    _hibit(bitmap) - 1;
							if (hb >= 0)
								break;
						} while (runword--);
						if (hb < 0)
							_maxpriq = _minpri-1;
						else
							_maxpriq = hb + BPW *
							    runword;
					}
				}
				ASSERT(t->t_state == TS_RUN);
				t->t_state = TS_DISP;
				t->t_savedstate = NULL;
				t->t_ssflg = 0;
				_onproc_enq(t);
				if (t->t_flag & T_OFFPROC) {
					t->t_flag &= ~T_OFFPROC;
					t->t_lwpid = curthread->t_lwpid;
					ASSERT(LWPID(t) != 0);
					t->t_idle = curthread->t_idle;
					goto out;
				}
				if (ISPARKED(t))
					_unpark(t);
			}
		} while (hb >= 0);
	}
idle:
	t = curthread->t_idle;
out:
	ITRACE_0(UTR_FAC_TLIB_DISP, UTR_DISP_END, "_disp_end");
	return (t);
}

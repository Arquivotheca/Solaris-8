/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)preempt.c	1.43	99/10/25 SMI"

#include "libthread.h"
#include "tdb_agent.h"

static void
check_for_psigs(void)
{
	uthread_t *t = curthread;
	sigset_t ppending;

	ASSERT(t->t_nosig > 0);
	/*
	 * On coming back from _swtch() in _dopreempt() below, which is after
	 * preemption or suspension has occurred, or after clearing preemption
	 * on current thread, the preempted or suspended thread needs to check
	 * for signals that could have been put into _pmask but not delivered
	 * to this thread, even though its mask was not masking them, because
	 * it was in the process of being preempted or suspended.
	 *
	 * No need to hold schedlock or _pmasklock here. First just do
	 * a quick check on whether we need to really do the signal check.
	 * If we do, *then* grab the required locks and do the work - this
	 * is to speed up the most common case (i.e. ppending is empty).
	 */
	_sigmaskset(&_pmask, &t->t_hold, &ppending);

	if (!sigisempty(&ppending)) {
		_sigemptyset(&ppending);
		_lwp_mutex_lock(&_pmasklock);
		_sigunblock(&_pmask, &t->t_hold, &ppending);
		_lwp_mutex_unlock(&_pmasklock);
		if (!sigisempty(&ppending)) {
			_sched_lock_nosig();
			sigorset(&t->t_bsig, &ppending);
			t->t_bdirpend = 1;
			_sched_unlock_nosig();
		}
	}

}

void
_dopreempt(ucontext_t *uc)
{

	thstate_t ostate;
	uthread_t *t = curthread;
	sigset_t ppending;

	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(t->t_state != TS_SLEEP);
	ASSERT(!ISBOUND(t) || (ISBOUND(t) && (t->t_stop != 0 ||
	    t->t_state == TS_ONPROC)));
	ASSERT(t->t_nosig > 0);

	if (ONPROCQ(t))
		_onproc_deq(t);
	if (!t->t_stop && !ISBOUND(t)) {
		if (DISP_PRIO(t) >= _maxpriq) {
			t->t_flag &= ~T_PREEMPT;
			t->t_preempt = 0;
			ASSERT(t->t_state == TS_ONPROC ||
			    t->t_state == TS_DISP);
			_onproc_enq(t);
			_sched_unlock_nosig();
			check_for_psigs();
			return;
		}
		t->t_savedstate = uc;
		_setrq(t);
		/*
		 * set the thread's preempt flag in case _swtch() returns
		 * without surrendering the "processor". t->t_preempt is
		 * cleared when _swtch() has switched to a different thread.
		 */
		t->t_preempt = 1;
	}
	ASSERT(!(t->t_flag & T_IDLE));
	_sched_unlock_nosig();
	ASSERT(t->t_nosig > 0);

	/*
	 * Unmask all signals from the underlying LWP if about
	 * to switch from the handler - since while in the
	 * handler, all signals are blocked on the LWP.
	 */
	if (!ISBOUND(t) && (t->t_flag & T_INSIGLWP))  {
		__sigprocmask(SIG_SETMASK, &_allunmasked, NULL);
	}
	if (__td_event_report(t, TD_PREEMPT)) {
		t->t_td_evbuf.eventnum = TD_PREEMPT;
		tdb_event_preempt();
	}
	_swtch(0);
	check_for_psigs();
}

void
_preempt(uthread_t *t, pri_t pri)
{
	ASSERT(MUTEX_HELD(&_schedlock));
	t = _choose_thread(pri);
	if (t) {
		t->t_flag |= T_PREEMPT;
		ASSERT(!(t->t_flag & T_IDLE));
		if (t == curthread)
			t->t_preempt = 1;
		else if (t->t_state == TS_ONPROC) {
			if (_lwp_kill(LWPID(t), SIGLWP) < 0)
				_panic("preempt: lwp_kill failed");
		}
	}
}

/*
 * Find the lowest priority running thread. If the lowest
 * priority running thread has a priority lower than "pri", return this
 * thread. If not, return NULL.
 */
struct thread *
_choose_thread(pri_t pri)
{
	uthread_t *lowt;
	uthread_t *t = _onprocq;
	int lowpri;

	ASSERT(MUTEX_HELD(&_schedlock));

	if (_onprocq == NULL)
		return (NULL);

	lowpri = THREAD_MAX_PRIORITY + 1;

	do {
		if (t->t_state != TS_ONPROC && t->t_state != TS_DISP)
			_panic("_choose_thread");
		if (DISP_PRIO(t) < lowpri) {
			/* is thread preemptable? */
			if (!(t->t_flag & T_DONTPREEMPT) && !PREEMPTED(t)) {
				lowpri = DISP_PRIO(t);
				lowt = t;
			}
		}
		t = t->t_forw;
	} while (t != _onprocq);
	if (pri > lowpri) {
		if (lowpri == -1)
			_panic("_choose_thread: found idling thread");
		return (lowt);
	}
	else
		return (NULL);
}

void
_preempt_off(void)
{
	if (!ISBOUND(curthread)) {
		_sched_lock();
		curthread->t_flag |= T_DONTPREEMPT;
		_sched_unlock();
	}
}

void
_preempt_on(void)
{
	extern int _maxpriq;

	if (!ISBOUND(curthread)) {
		_sched_lock();
		curthread->t_flag &= ~(T_DONTPREEMPT);
		if (_maxpriq > DISP_PRIO(curthread)) {
			_dopreempt(NULL);
			_sigon();
		} else
			_sched_unlock();
	}
}

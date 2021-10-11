/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)timer.c	1.9	94/07/15	SMI"


#include "libthread.h"

static	void _t_setitimer(int arg);
static	void _t_alarm(int arg);

int
_thr_alarm(int s)
{
	thread_t tid = curthread->t_tid;
	struct timeval tv;

	if (ISBOUND(curthread)) {
		return (alarm(s));
	}
	tv.tv_sec = s;
	tv.tv_usec = 0;
	return (_setcallout(&curthread->t_alrm_callo,
						tid, &tv, _t_alarm, tid));
}

int
_thr_setitimer(int which, struct itimerval *v, struct itimerval *ov)
{
	struct timeval clocktime;
	struct timeval deltv;
	thread_t tid = curthread->t_tid;

	if (ISBOUND(curthread))
		return (setitimer(which, v, ov));
	if (ov != NULL)
		*ov = curthread->t_realitimer;
	if (v != NULL) {
		if (v->it_value.tv_sec == 0 && v->it_value.tv_usec == 0)
			return (_rmcallout(&curthread->t_alrm_callo));
		curthread->t_realitimer = *v;
		if (curthread->t_alrm_callo.flag == CO_TIMER_ON &&
		    v->it_interval.tv_sec == 0 &&
		    v->it_interval.tv_usec == 0)
			return (1);
		return (_setcallout(&curthread->t_alrm_callo,
		    tid, &v->it_value, _t_setitimer, tid));
	}
}

static void
_t_setitimer(int arg)
{
	thread_t tid = (thread_t)arg;
	uthread_t *t;
	struct itimerval *itv;
	struct timeval interval;
	int ix;

	if (_thr_kill(tid, SIGALRM) == ESRCH) {
		/* if thr_kill() is done to a non-existent tid */
		return;
	}
	_lmutex_lock(&_calloutlock);
	_lock_bucket((ix = HASH_TID(tid)));
	if ((t = THREAD(tid)) == (uthread_t *)-1) {
		/* if thread has disappeared by this time */
		_unlock_bucket(ix);
		return;
	}
	itv = &t->t_realitimer;
	interval = itv->it_interval;
	_lmutex_unlock(&_calloutlock);
	if (interval.tv_sec > 0 || interval.tv_usec > 0)
		_setcallout(&t->t_alrm_callo, tid, &itv->it_interval,
		    _t_setitimer, tid);
	_unlock_bucket(ix);
}

static void
_t_alarm(int arg)
{
	_thr_kill((thread_t)arg, SIGALRM);
}

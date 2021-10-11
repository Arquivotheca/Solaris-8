/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)cancel.c	1.29	98/01/30 SMI"

#ifdef __STDC__

#pragma weak pthread_cancel = _pthread_cancel
#pragma weak pthread_testcancel = _pthread_testcancel
#pragma weak pthread_setcanceltype = _pthread_setcanceltype
#pragma weak pthread_setcancelstate = _pthread_setcancelstate

#pragma weak _ti_pthread_cancel = _pthread_cancel
#pragma weak _ti_pthread_testcancel = _pthread_testcancel
#pragma weak _ti_pthread_setcanceltype = _pthread_setcanceltype
#pragma weak _ti_pthread_setcancelstate = _pthread_setcancelstate

#pragma weak _ti__pthread_cleanup_push = __pthread_cleanup_push
#pragma weak _ti__pthread_cleanup_pop = __pthread_cleanup_pop
#endif /* __STDC__ */

#include "libthread.h"

/*
 * Thread is existing due to the cancellation.  This sets the
 * cancel pending bit so that _pthread_exit (_thr_exit)
 * knows when to call the cleanup callback.
 */
#define	CANCEL_EXIT() t->t_can_pending = TC_PENDING; \
			_pthread_exit((void *)PTHREAD_CANCELED)

/*
 * _ex_unwind is supported by libC, but if libC is not loaded
 * we would like to call local version of _ex_unwind which does
 * exactly same except calling destructor. By making weak, it
 * allows us to check whether symbol is loaded or not.
 */
#pragma weak _ex_unwind
extern	void	_ex_unwind(void (*func)(void *), void *arg);

/*
 * Cleanup handler related data
 * This structure is exported as _cleanup_t in pthread.h.
 * We export only the size of this structure. so check
 * _cleanup_t in pthread.h before making a change here.
 */
typedef struct __cleanup {
	_cleanup_t *next;		/* pointer to next handler */
	caddr_t	fp;			/* current frame pointer */
	void	(*func)(void *);	/* cleanup handler address */
	void	*arg;			/* handler's argument */
} __cleanup_t;

/*
 * POSIX.1c
 * pthread_cancel: tries to cancel the targeted thread.
 * if the target thread has already exited/killed no
 * action is taken. If the target thread is self, then
 * treat this call as thr_exit (may be disputed?).
 * Else, send signal if thread needs to be canceled or
 * mark it pending if thread is not yet cancelable.
 */

int
_pthread_cancel(thread_t tid)
{

	uthread_t *t;
	int ix;
	int rc = 0;
	lwpid_t lwpid;
	int pending;


	if (tid == (thread_t)0)
		return (ESRCH);
	_lock_bucket((ix = HASH_TID(tid)));
	if ((t = THREAD(tid)) == (uthread_t *)-1 ||
	    t->t_flag & T_INTERNAL) {
		_unlock_bucket(ix);
		return (ESRCH);
	}

	/*
	 * if the request is to cancel self and cancellation state is enabled
	 * and type is ASYNCHRONOUS treat this as _thr_exit.
	 */
	if ((t == curthread) && CANCELENABLE(t) && CANCELASYNC(t)) {
		_unlock_bucket(ix);
		CANCEL_EXIT();
		/* should never return here */
	}

	_sched_lock();
	if (t->t_state == TS_ZOMB) {
		_sched_unlock();
		_unlock_bucket(ix);
		return (ESRCH);
	}
	_sched_unlock();

	pending = (int)(t->t_can_pending);
	t->t_can_pending = TC_PENDING;
	_flush_store();
	/* cancellation already pending */
	if (!pending) {
		if (CANCELENABLE(t) && (CANCELABLE(t) || CANCELASYNC(t))) {
			_sched_lock();
			/* internal version of thr_kill */
			rc = _thrp_kill_unlocked(t, ix, SIGCANCEL, &lwpid);
			if (rc == 0 && lwpid != NULL) {
				rc = _lwp_kill(lwpid, SIGCANCEL);
			}
			_sched_unlock();
		}
	}
	_unlock_bucket(ix);
	return (rc);

}

/*
 * POSIX.1c
 * pthread_setcancelstate: sets the state ENABLED or DISABLED
 * If the state is being set as ENABLED, then it becomes
 * the cancellation point only if the type of cancellation is
 * ASYNCHRONOUS and a cancel request is pending.
 * Disabling cancellation is not a cancellation point.
 */

int
_pthread_setcancelstate(int state, int *oldstate)
{

	uthread_t *t = curthread;
	int pstate = t->t_can_state;

	if (state == PTHREAD_CANCEL_ENABLE) {
		t->t_can_state = TC_ENABLE;
		_flush_store();
		/*
		 * if this thread has been requested to be canceled
		 * and is in async mode.
		 */
		if (CANCELASYNC(t) && CANCELPENDING(t)) {
			CANCEL_EXIT();
			/* should never return here */
		}
	} else if (state == PTHREAD_CANCEL_DISABLE) {
		t->t_can_state = TC_DISABLE;
		_flush_store();
		/*
		 * if this thread has been requested to be canceled
		 * and is in async mode and was enabled.
		 */
		if (pstate == TC_ENABLE &&
		    CANCELASYNC(t) && CANCELPENDING(t)) {
			CANCEL_EXIT();
			/* should never return here */
		}
	} else
		return (EINVAL);

	if (oldstate != NULL) {
		if (pstate == TC_ENABLE)
			*oldstate = PTHREAD_CANCEL_ENABLE;
		else
			*oldstate = PTHREAD_CANCEL_DISABLE;
	}
	return (0);
}

/*
 * POSIX.1c
 * pthread_setcanceltype: sets the type DEFERRED or ASYNCHRONOUS
 * If the type is being set as ASYNC, then it becomes
 * the cancellation point if there is a cancellation pending.
 */

int
_pthread_setcanceltype(int type, int *oldtype)
{
	uthread_t *t = curthread;
	int ptype = t->t_can_type;


	if (type == PTHREAD_CANCEL_ASYNCHRONOUS) {
		t->t_can_type = TC_ASYNCHRONOUS;
		_flush_store();
		/*
		 * if this thread has been requested to be canceled
		 * and is in enabled.
		 */
		if (CANCELPENDING(t) && CANCELENABLE(t)) {
			CANCEL_EXIT();
			/* should never return here */
		}
	} else if (type == PTHREAD_CANCEL_DEFERRED) {
		t->t_can_type = TC_DEFERRED;
		_flush_store();
		/*
		 * if this thread has been requested to be canceled
		 * and is in enabled and was in async mode.
		 */
		if (ptype == TC_ASYNCHRONOUS &&
		    CANCELPENDING(t) && CANCELENABLE(t)) {
			CANCEL_EXIT();
			/* should never return here */
		}
	} else
		return (EINVAL);

	if (oldtype != NULL) {
		if (ptype == TC_ASYNCHRONOUS)
			*oldtype = PTHREAD_CANCEL_ASYNCHRONOUS;
		else
			*oldtype = PTHREAD_CANCEL_DEFERRED;
	}
	return (0);
}


/*
 * POSIX.1c
 * pthread_testcancel: tests for any cancellation pending
 * if the cancellation is enabled and is pending, act on
 * it by calling thr_exit. thr_exit takes care of calling
 * cleanup handlers.
 */

void
_pthread_testcancel(void)
{

	uthread_t *t = curthread;

	if (CANCELENABLE(t) && CANCELPENDING(t)) {
		CANCEL_EXIT();
	}
}

/*
 * For deferred mode, this routine makes a thread cancelable.
 * It is called from the functions which want to be cancellation
 * point and are about to block, such as cond_wait().
 */

void
_cancelon()
{
	uthread_t *t = curthread;

	if (CANCELENABLE(t)) {
		t->t_cancelable = TC_CANCELABLE;
		_flush_store();
		if (CANCELPENDING(t)) {
			_sched_lock_nosig();
			if (t->t_state == TS_SLEEP) {
				_unsleep(t);
				t->t_state = TS_ONPROC;
				if (!ISBOUND(t))
					_onproc_enq(t);
			}
			_sched_unlock_nosig();
			_sigon();
			CANCEL_EXIT();
		}
	}
}

/*
 * This routines turns cancelability off. It is called from
 * function which are cancellation points such as cond_wait().
 * _cancelon() and _canceloff() are usually called around swtch()
 * which blocks the thread.
 */

void
_canceloff()
{
	uthread_t *t = curthread;
	int cancelable;

	if (CANCELENABLE(t)) {
		cancelable =  t->t_cancelable;
		t->t_cancelable = ~TC_CANCELABLE;
		_flush_store();
		/*
		 * cancel is taken if there is a cancel pending
		 * and we are not waking up consuming a wakeup due to
		 * condtion signal or semaphore.
		 * cancelability is turned off in _t_release() when the
		 * thread is woken up due to condtion signal or semaphore.
		 */
		if (CANCELPENDING(t) && (cancelable == TC_CANCELABLE)) {
			_sigon();
			CANCEL_EXIT();
		}
	}
}

/*
 * __pthread_cleanup_push: called by macro in pthread.h which defines
 * POSIX.1c pthread_cleanup_push(). Macro in pthread.h allocates the
 * cleanup struct and calls this routine to push the handler off the
 * curthread's struct.
 */

void
__pthread_cleanup_push(void (*routine)(void *), void *args, caddr_t fp,
						_cleanup_t *clnup_info)
{
	uthread_t *t = curthread;
	__cleanup_t *infop = (__cleanup_t *)clnup_info;

	infop->func = routine;
	infop->arg = args;
	infop->fp = fp;
	infop->next = t->t_clnup_hdr;
	t->t_clnup_hdr = clnup_info;
}

/*
 * __pthread_cleanup_pop: called by macro in pthread.h which defines
 * POSIX.1c pthread_cleanup_pop(). It calls this routine to pop the
 * handler off the curthread's struct execute it if necessary.
 */

void
__pthread_cleanup_pop(int ex, _cleanup_t *clnup_info)
{
	uthread_t *t = curthread;
	__cleanup_t *infop = (__cleanup_t *)(t->t_clnup_hdr);

	t->t_clnup_hdr = infop->next;

	if (ex)
		(*infop->func)(infop->arg);
}


/*
 * _t_cancel(fp):calls cleanup handlers if there are any in
 *		 frame (fp), and calls _ex_unwind() to call
 *		 destructors if libC has been linked.
 *
 * Control comes here from _tcancel_all. Logically:
 *
 *	_tcancel_all: first arg = current fp;
 *	    jump _t_cancel;
 *
 *
 * XXX:
 * we could have called _t_cancel(_getfp) from _thr_exit()
 * but _ex_unwind() also calls _t_cancel() and it does after
 * poping out the two frames. If _ex_unwind() passes current
 * fp, then it will be invalid. For caller of _tcancel_all()
 * it looks like as if it is calling _t_cancel(fp).
 *
 * Another way was to find caller's fp in _t_cancel() itself,
 * but for sparc, register windows need to be flushed which
 * can be avoided this way.
 *
 * _t_cancel will eventually call _thrp_exit().
 * It never returns from _t_cancel().
 *
 */

void
_t_cancel(void *fp)
{
	__cleanup_t *head;
	void (* fptr)();

	if (fp == NULL) {
		_thrp_exit();
	}

	while ((head = (__cleanup_t *)(curthread->t_clnup_hdr)) != NULL) {
		if (fp == head->fp) {
			curthread->t_clnup_hdr = head->next;
			/* execute the cleanup handler */
			_ex_clnup_handler(head->arg, head->func,
			    _tcancel_all);
		} else
			break;
	}

	/* see comment above declaration of _ex_unwind */
	if ((fptr = _ex_unwind) != 0 && (curthread->t_flag & T_EXUNWIND))
		/* libC is loaded and thread is canceled, call libC version */
		(* fptr)(_tcancel_all, 0);
	else if (head != NULL)
		/* libC not present, call local version */
		_ex_unwind_local(_tcancel_all, 0);
	else
		/* libC not present and no cleanup handlers, exit here */
		_thrp_exit();

	/* never returns here */
	head = (__cleanup_t *)(curthread->t_clnup_hdr);
}

/*
 * _sigcancel: SIGCANCEL handler
 */
void
_sigcancel(int sig, siginfo_t *sip, ucontext_t *uap)
{
	uthread_t *t = curthread;

	ASSERT(_sigismember(&curthread->t_hold, SIGCANCEL));
	/*
	 * we are canceling the thread, if this thread has been sent
	 * SIGCANCEL from _pthread_cancel() it is meant to be killed.
	 * Re-check the canceltype and state just in case target thread
	 * decided to change type or state before signal arrived.
	 */

	if (CANCELENABLE(t) &&
	    (CANCELABLE(t) || CANCELASYNC(t) || ISBOUND(t))) {
		CANCEL_EXIT();
	}
}

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cancel.c	1.2	99/11/02 SMI"

#include "liblwp.h"

static void do_sigcancel(void);

/*
 * _ex_unwind is supported by libC, but if libC is not loaded
 * we would like to call local version of _ex_unwind which does
 * exactly same except calling destructor. By making weak, it
 * allows us to check whether symbol is loaded or not.
 */
#pragma weak _ex_unwind
extern	void	_ex_unwind(void (*func)(void *), void *arg);
#pragma unknown_control_flow(_ex_unwind)

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
 * pthread_cancel: tries to cancel the targeted thread.
 * If the target thread has already exited no action is taken.
 * Else send SIGCANCEL to request the other thread to cancel itself.
 */
#pragma weak pthread_cancel = _pthread_cancel
#pragma weak _liblwp_pthread_cancel = _pthread_cancel
int
_pthread_cancel(thread_t tid)
{
	ulwp_t *ulwp;
	int error = 0;

	if ((ulwp = find_lwp(tid)) == NULL)
		return (ESRCH);

	if (ulwp->ul_cancel_pending) {	/* don't kill more than once */
		ulwp_unlock(ulwp);
	} else if (ulwp == curthread) {	/* unlock self before cancelling */
		ulwp_unlock(ulwp);
		do_sigcancel();
	} else {		/* request the other thread to cancel self */
		error = __lwp_kill(tid, SIGCANCEL);
		ulwp_unlock(ulwp);
	}

	return (error);
}

/*
 * pthread_setcancelstate: sets the state ENABLED or DISABLED
 * If the state is being set as ENABLED, then it becomes
 * a cancellation point only if the type of cancellation is
 * ASYNCHRONOUS and a cancel request is pending.
 * Disabling cancellation is not a cancellation point.
 */
#pragma weak pthread_setcancelstate = _pthread_setcancelstate
#pragma weak _liblwp_pthread_setcancelstate = _pthread_setcancelstate
int
_pthread_setcancelstate(int state, int *oldstate)
{
	ulwp_t *self = curthread;
	int was_disabled;

	/*
	 * Call enter_critical() to defer SIGCANCEL until exit_critical().
	 * We do this because curthread->ul_cancel_pending is set in the
	 * SIGCANCEL handler and we must be async-signal safe here.
	 */
	enter_critical();

	was_disabled = self->ul_cancel_disabled;
	switch (state) {
	case PTHREAD_CANCEL_ENABLE:
		self->ul_cancel_disabled = 0;
		break;
	case PTHREAD_CANCEL_DISABLE:
		self->ul_cancel_disabled = 1;
		break;
	default:
		exit_critical();
		return (EINVAL);
	}

	/*
	 * If this thread has been requested to be canceled and
	 * is in async mode and is or was enabled, then exit.
	 */
	if ((!self->ul_cancel_disabled || !was_disabled) &&
	    self->ul_cancel_async && self->ul_cancel_pending) {
		exit_critical();
		_pthread_exit(PTHREAD_CANCELED);
	}

	exit_critical();

	if (oldstate != NULL) {
		if (was_disabled)
			*oldstate = PTHREAD_CANCEL_DISABLE;
		else
			*oldstate = PTHREAD_CANCEL_ENABLE;
	}
	return (0);
}

/*
 * pthread_setcanceltype: sets the type DEFERRED or ASYNCHRONOUS
 * If the type is being set as ASYNC, then it becomes
 * a cancellation point if there is a cancellation pending.
 */
#pragma weak pthread_setcanceltype = _pthread_setcanceltype
#pragma weak _liblwp_pthread_setcanceltype = _pthread_setcanceltype
int
_pthread_setcanceltype(int type, int *oldtype)
{
	ulwp_t *self = curthread;
	int was_async;

	/*
	 * Call enter_critical() to defer SIGCANCEL until exit_critical().
	 * We do this because curthread->ul_cancel_pending is set in the
	 * SIGCANCEL handler and we must be async-signal safe here.
	 */
	enter_critical();

	was_async = self->ul_cancel_async;
	switch (type) {
	case PTHREAD_CANCEL_ASYNCHRONOUS:
		self->ul_cancel_async = 1;
		break;
	case PTHREAD_CANCEL_DEFERRED:
		self->ul_cancel_async = 0;
		break;
	default:
		exit_critical();
		return (EINVAL);
	}
	self->ul_save_async = self->ul_cancel_async;

	/*
	 * If this thread has been requested to be canceled and
	 * is in enabled mode and is or was in async mode, exit.
	 */
	if ((self->ul_cancel_async || was_async) &&
	    self->ul_cancel_pending && !self->ul_cancel_disabled) {
		exit_critical();
		_pthread_exit(PTHREAD_CANCELED);
	}

	exit_critical();

	if (oldtype != NULL) {
		if (was_async)
			*oldtype = PTHREAD_CANCEL_ASYNCHRONOUS;
		else
			*oldtype = PTHREAD_CANCEL_DEFERRED;
	}
	return (0);
}

/*
 * pthread_testcancel: tests for any cancellation pending
 * if the cancellation is enabled and is pending, act on
 * it by calling thr_exit. thr_exit takes care of calling
 * cleanup handlers.
 */
#pragma weak pthread_testcancel = _pthread_testcancel
#pragma weak _liblwp_pthread_testcancel = _pthread_testcancel
void
_pthread_testcancel(void)
{
	ulwp_t *self = curthread;

	if (self->ul_cancel_pending && !self->ul_cancel_disabled)
		_pthread_exit(PTHREAD_CANCELED);
}

/*
 * For deferred mode, this routine makes a thread cancelable.
 * It is called from the functions which want to be cancellation
 * points and are about to block, such as cond_wait().
 */
void
_cancelon()
{
	ulwp_t *self = curthread;

	ASSERT(!(self->ul_cancelable && self->ul_cancel_disabled));
	if (!self->ul_cancel_disabled) {
		ASSERT(self->ul_cancelable >= 0);
		self->ul_cancelable++;
		if (self->ul_cancel_pending)
			_pthread_exit(PTHREAD_CANCELED);
	}
}

/*
 * This routine turns cancelability off and possible calls pthread_exit().
 * It is called from functions which are cancellation points, like cond_wait().
 */
void
_canceloff()
{
	ulwp_t *self = curthread;

	ASSERT(!(self->ul_cancelable && self->ul_cancel_disabled));
	if (!self->ul_cancel_disabled) {
		if (self->ul_cancel_pending)
			_pthread_exit(PTHREAD_CANCELED);
		self->ul_cancelable--;
		ASSERT(self->ul_cancelable >= 0);
	}
}

/*
 * Same as _canceloff() but don't actually cancel the thread.
 * This is used by cond_wait() and sema_wait() when they don't get EINTR.
 */
void
_canceloff_nocancel()
{
	ulwp_t *self = curthread;

	ASSERT(!(self->ul_cancelable && self->ul_cancel_disabled));
	if (!self->ul_cancel_disabled) {
		self->ul_cancelable--;
		ASSERT(self->ul_cancelable >= 0);
	}
}

/*
 * __pthread_cleanup_push: called by macro in pthread.h which defines
 * POSIX.1c pthread_cleanup_push(). Macro in pthread.h allocates the
 * cleanup struct and calls this routine to push the handler off the
 * curthread's struct.
 */
#pragma weak _liblwp__pthread_cleanup_push = __pthread_cleanup_push
void
__pthread_cleanup_push(void (*routine)(void *),
	void *args, caddr_t fp, _cleanup_t *clnup_info)
{
	ulwp_t *self = curthread;
	__cleanup_t *infop = (__cleanup_t *)clnup_info;

	infop->func = routine;
	infop->arg = args;
	infop->fp = fp;
	infop->next = self->ul_clnup_hdr;
	self->ul_clnup_hdr = clnup_info;
}

/*
 * __pthread_cleanup_pop: called by macro in pthread.h which defines
 * POSIX.1c pthread_cleanup_pop(). It calls this routine to pop the
 * handler off the curthread's struct and execute it if necessary.
 */
#pragma weak _liblwp__pthread_cleanup_pop = __pthread_cleanup_pop
/* ARGSUSED1 */
void
__pthread_cleanup_pop(int ex, _cleanup_t *clnup_info)
{
	ulwp_t *self = curthread;
	__cleanup_t *infop = (__cleanup_t *)(self->ul_clnup_hdr);

	self->ul_clnup_hdr = infop->next;
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
 * We could have called _t_cancel(_getfp) from _thr_exit()
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
	ulwp_t *self = curthread;
	__cleanup_t *head;
	void (* fptr)();

	if (fp == NULL) {
		_thrp_exit();
		panic("_t_cancel(): _thrp_exit() returned");
	}

	if ((head = (__cleanup_t *)(self->ul_clnup_hdr)) != NULL &&
	    fp == head->fp) {
		self->ul_clnup_hdr = head->next;
		/* execute the cleanup handler */
		_ex_clnup_handler(head->arg, head->func, _tcancel_all);
		panic("_t_cancel(): _ex_clnup_handler() returned");
	}

	/* see comment above declaration of _ex_unwind */
	if ((fptr = _ex_unwind) != 0 && self->ul_unwind) {
		/* libC is loaded and thread is canceled, call libC version */
		(* fptr)(_tcancel_all, 0);
		panic("_t_cancel(): _ex_unwind() returned");
	} else if (head != NULL) {
		/* libC not present, call local version */
		_ex_unwind_local(_tcancel_all, 0);
		panic("_t_cancel(): _ex_unwind_local() returned");
	} else {
		/* libC not present and no cleanup handlers, exit here */
		_thrp_exit();
		panic("_t_cancel(): _thrp_exit() returned");
	}
	/* never returns here */
}

/*
 * _sigcancel: SIGCANCEL handler
 */
/* ARGSUSED2 */
void
_sigcancel(int sig, siginfo_t *sip, void *uap)
{
	/*
	 * If this thread has been sent SIGCANCEL from
	 * _pthread_cancel() it is being asked to exit.
	 */
	if (sig == SIGCANCEL && sip != NULL && sip->si_code == SI_LWP)
		do_sigcancel();
}

/*
 * Common code for cancelling self in _sigcancel() and pthread_cancel().
 * If the thread is at a cancellation point (ul_cancelable) then just
 * return and let _canceloff() do the exit, else exit immediately if
 * async mode is in effect.
 */
static void
do_sigcancel()
{
	ulwp_t *self = curthread;

	ASSERT(self->ul_critical == 0);
	self->ul_cancel_pending = 1;
	if (self->ul_cancel_async &&
	    !self->ul_cancel_disabled &&
	    !self->ul_cancelable)
		_pthread_exit(PTHREAD_CANCELED);
}

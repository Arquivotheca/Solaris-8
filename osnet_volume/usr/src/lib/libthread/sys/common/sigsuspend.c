/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)sigsuspend.c	1.3	96/11/01	SMI"

#ifdef __STDC__
#pragma weak sigsuspend = _sigsuspend

#pragma weak _ti_sigsuspend = _sigsuspend
#endif /* __STDC__ */

#include "libthread.h"
#include <signal.h>
#include <errno.h>

int
_sigsuspend(const sigset_t *set)
{
	sigset_t emptyset, oset;
	int ret;

	_sigemptyset(&emptyset);
	/*
	 * The order in which the thread/lwp masks are set/reset in the
	 * following code is crucial. It preserves all the invariants in
	 * thread/lwp masking. The order of the mask resetting after return
	 * from __sigsuspend_cancel() is esp. crucial. If the
	 * __sigprocmask(,&emptyset,) is done after the thr_sigsetmask(,&oset,),
	 * then this lwp could get a signal while the thread was masking it!
	 * This is because on return from __sigsuspend_cancel(), the thread has
	 * a less restrictive mask than the LWP, as just before this call.
	 * This could result in signals directed to this thread but pending on
	 * the LWP. If this occurs before the call, __sigsuspend() does the
	 * right thing - it will take these pending signals since it opens up
	 * the LWP mask. However, after the call, with such pending signals, if
	 * the call to thr_sigsetmask(,&oset,) makes the thread mask more
	 * restrictive, and *then* the LWP mask is cleared, the pending signals
	 * are sent up, even though the thread is masking it, violating a
	 * fundamental invariant.
	 *
	 * XXX: However, this means that there is a window, after the LWP
	 * has cleared its signal mask to all empty, but before the thread
	 * has restored its mask, that a signal could come in and jump into
	 * the handler outside the call to __sigsuspend_trap(). This
	 * compromises the semantics of sigsuspend() somewhat. Currently,
	 * for single threaded processes, the semantic is that only
	 * one such signal can come in - however, it has not been spelled
	 * out anywhere in the man pages or any standard. So, it should be
	 * OK to change the behavior for MT.
	 *
	 * XXX: Another point: while executing the signal handler, the thread
	 * does not have its old mask restored. It has the mask passed to
	 * sigsuspend(). Again, this is not specified in the man pages, nor
	 * in any standards document. So, it should be OK to not have the
	 * mask be restored while executing the signal handler that interrupted
	 * sigsuspend().
	 * From the man page for sigsuspend(2):
	 * "If the action is to execute a signal catching function, sigsuspend()
	 *  returns  after  the  signal  catching function returns.  On return,
	 *  the signal mask is restored to the set that existed before the call
	 *  to sigsuspend()."
	 * Note that the above says that the mask is restored on return from
	 * the call to sigsuspend(), which is *after* running the handler. While
	 * the handler is running the mask may or may not be restored - this is
	 * unspecified.
	 */
	thr_sigsetmask(SIG_SETMASK, NULL, &oset);
	__sigprocmask(SIG_SETMASK, &oset, NULL);
	thr_sigsetmask(SIG_SETMASK, set, NULL);
	_cancelon();
	ret = (*__sigsuspend_trap)(set);
	_canceloff();
	__sigprocmask(SIG_SETMASK, &emptyset, NULL);
	thr_sigsetmask(SIG_SETMASK, &oset, NULL);
	return (ret);
}

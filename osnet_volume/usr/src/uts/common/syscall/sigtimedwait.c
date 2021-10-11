/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)sigtimedwait.c	1.26	99/06/11 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/bitmap.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/fault.h>
#include <sys/procset.h>
#include <sys/signal.h>
#include <sys/siginfo.h>
#include <sys/time.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/condvar_impl.h>
#include <sys/model.h>

#define	sigandset(x, y, z) (\
	(x)->__sigbits[0] = (y)->__sigbits[0] & (z)->__sigbits[0]; \
	(x)->__sigbits[1] = (y)->__sigbits[1] & (z)->__sigbits[1];

#define	getnotifysigs(rs, ns, ts)	(\
	(rs)->__sigbits[0] = ~((ns)->__sigbits[0]) & (ts)->__sigbits[0], \
	(rs)->__sigbits[1] =  ~((ns)->__sigbits[1]) & (ts)->__sigbits[1])

#define	maskreverse(s1, s2) {\
	int i; \
	for (i = 0; i < 2; i++)\
		(s2)->__sigbits[i] =  ~(s1)->__sigbits[i]; \
}

extern  void    bsigaddqa(proc_t *, kthread_t *, sigqueue_t *);

static int
copyout_siginfo(model_t datamodel, k_siginfo_t *ksip, void *uaddr)
{
	if (datamodel == DATAMODEL_NATIVE) {
		if (copyout(ksip, uaddr, sizeof (*ksip)))
			return (set_errno(EFAULT));
	}
#ifdef _SYSCALL32_IMPL
	else {
		siginfo32_t si32;

		siginfo_kto32(ksip, &si32);
		if (copyout(&si32, uaddr, sizeof (si32)))
			return (set_errno(EFAULT));
	}
#endif
	return (ksip->si_signo);
}

/*
 * Wait until a signal within a specified set is posted or until the
 * time interval 'timeout' if specified.  The signal is caught but
 * not delivered. The value of the signal is returned to the caller.
 */
int
sigtimedwait(sigset_t *setp, siginfo_t *siginfop, timespec_t *timeoutp)
{
	return (csigtimedwait(setp, siginfop, timeoutp, NULL));
}

int
csigtimedwait(sigset_t *setp, siginfo_t *siginfop, timespec_t *timeoutp,
	int *queued)
{
	sigset_t set;
	k_sigset_t kset;
	k_sigset_t oldmask;
	proc_t *p = curproc;
	kthread_t *t = curthread;
	klwp_t *lwp = ttolwp(curthread);
	timespec_t sig_timeout, now;
	clock_t sig_time = 0;
	int ret = 0;
	clock_t time_left;
	k_siginfo_t info, *infop;
	int sig;
	kthread_t *ast;
	sigqueue_t *qp = NULL;
	model_t datamodel = get_udatamodel();
	int q = 0;

	if (t == p->p_aslwptp)
		return (set_errno(EINVAL));
	if (copyin(setp, &set, sizeof (set)))
		return (set_errno(EFAULT));
	sigutok(&set, &kset);
	sigdiffset(&kset, &cantmask);
	if (sigisempty(&kset))
		return (set_errno(EINVAL));
	if (timeoutp) {
		if (datamodel == DATAMODEL_NATIVE) {
			if (copyin(timeoutp, &sig_timeout,
			    sizeof (sig_timeout)))
				return (set_errno(EFAULT));
		} else {
			timespec32_t timeout32;

			if (copyin(timeoutp, &timeout32, sizeof (timeout32)))
				return (set_errno(EFAULT));
			TIMESPEC32_TO_TIMESPEC(&sig_timeout, &timeout32)
		}

		if (itimerspecfix(&sig_timeout))
			return (set_errno(EINVAL));
		/*
		 * Convert the timespec value into lbolt equivalent
		 */
		if (timerspecisset(&sig_timeout)) {
			gethrestime(&now);
			timespecadd(&sig_timeout, &now);
			sig_time = timespectohz(&sig_timeout, now);
			sig_time += lbolt;
		}
	}
	mutex_enter(&p->p_lock);
	/*
	 * set the thread's signal mask to unmask
	 * those signals in the specified set.
	 */
	oldmask = t->t_hold;
	if ((ast = p->p_aslwptp) != NULL) {
		/*
		 * This is a multi-threaded process linked with -l[p]thread.
		 * Hence, this system call "sigtimedwait()" is called only
		 * in a very special scenario: when the threads library knows
		 * for sure that there is a signal in the kernel that needs
		 * to be extracted. The threads library has its own version
		 * of sigtimedwait(), and makes this system call only in a
		 * non-blocking manner, in this special case. When it does
		 * so, the LWP mask, in t_hold, is not important - it can
		 * and *should* be set to be the reverse of the signals of
		 * interest being "extracted". Otherwise, some other signal
		 * that was unmasked in the LWP mask could be returned, instead
		 * of the signal that was requested in this system call - this
		 * would result in the kernel's sigtimedwait() returning EINTR
		 * instead, and this would result in this signal being lost.
		 * NOTE: the LWP mask, for a multi-threaded process is typically
		 * not used - the real mask is the user-level thread mask.
		 */
		maskreverse(&kset, &t->t_hold);
		sig = fsig(&p->p_notifsigs, t);
		if (sig > 0) {
			ret = sig;
			sigdelset(&p->p_notifsigs, sig);
			if (sigdeq(p, ast, sig, &qp) == 1) {
				if ((sig >= _SIGRTMIN) && (sig <= _SIGRTMAX) &&
					queued) {

					q = 1;
					sigdelset(&ast->t_sig, sig);
					sigaddset(&p->p_notifsigs, sig);
				} else {
					cv_signal(&p->p_notifcv);
				}
			} else if (sigismember(&ast->t_sig, sig))
				cv_signal(&p->p_notifcv);
			/*
			 * The following is a special case for handling
			 * debugger interaction. We re-direct the signal to
			 * the calling thread by calling bsigaddqa() and adding
			 * it to t_sig. This signal will then be noticed inside
			 * issig_forreal() in the call to cv_timedwait_sig()
			 * below.
			 * This will ensure normal behavior with respect to
			 * the debugger, i.e. if this process is being
			 * debugged, control will be transferred back to
			 * debugger. The only exception to this case is
			 * when this thread is the aslwp. That is trapped
			 * earlier - an error is returned if this thread
			 * is the aslwp.
			 */
			if (qp != NULL)
				bsigaddqa(p, t, qp);
			sigaddset(&t->t_sig, sig);
		}
	} else {
		sigdiffset(&t->t_hold, &kset);
	}
	if (timeoutp) {
		while ((time_left = cv_timedwait_sig(&u.u_cv, &p->p_lock,
		    sig_time)) > 0)
			continue;
	} else {
		while (time_left = cv_wait_sig_swap(&u.u_cv, &p->p_lock))
			continue;
	}

	/*
	 * restore thread's signal mask to its previous value.
	 */
	t->t_hold = oldmask;
	aston(t);		/* so post_syscall sees new t_hold mask */

	mutex_exit(&p->p_lock);
	if (time_left == -1)
		return (set_errno(EAGAIN));	/* timer expired */
	/*
	 * Don't bother with signal if it is not in request set.
	 */
	if (lwp->lwp_cursig == 0 || !sigismember(&kset, lwp->lwp_cursig)) {
		/*
		 * lwp_cursig is zero if pokelwps() awakened cv_wait_sig().
		 * this happens if some LWP in this process has forked or
		 * exited.
		 */
		return (set_errno(EINTR));
	}

	if (lwp->lwp_curinfo)
		infop = &lwp->lwp_curinfo->sq_info;
	else {
		infop = &info;
		bzero(infop, sizeof (info));
		infop->si_signo = lwp->lwp_cursig;
		infop->si_code = SI_NOINFO;
	}
	ret = lwp->lwp_cursig;
	lwp->lwp_cursig = 0;

	if (siginfop)
		ret = copyout_siginfo(datamodel, infop, siginfop);
	if (lwp->lwp_curinfo) {
		siginfofree(lwp->lwp_curinfo);
		lwp->lwp_curinfo = NULL;
	}
	if ((sig >= _SIGRTMIN) && (sig <= _SIGRTMAX) && queued) {
		(void) copyout(&q, queued, sizeof (int));
	}
	return (ret);
}

static int
fsignum(k_sigset_t *s)
{
	int i;

	for (i = 0; i < sizeof (*s) / sizeof (s->__sigbits[0]); i++) {
		if (s->__sigbits[i])
			return ((i * sizeof (s->__sigbits[0]) * NBBY) +
			    lowbit(s->__sigbits[i]));
	}
	return (0);
}

/*
 * Wait for notification from the kernel of a signal that has been sent to the
 * process. The signal is not "delivered", due to changes in issig_forreal()
 * to not deliver it to the aslwp. Since this function can be called only by
 * the aslwp, the issig_forreal() changes for the aslwp should be OK.
 * The routine may return -1 and sets errno to EINTR if the wait was interrupted
 * by a fork() from some other LWP in the process. Or the EXITLWPS p_flag was
 * set. Otherwise, on success, it will return the signal number for the
 * signal whose notification has arrived at the process.
 */
int
signotifywait()
{
	k_sigset_t oldmask;
	proc_t *p = curproc;
	k_sigset_t notifysigs;
	kthread_t *t = curthread;
	int sig = 0;

	/*
	 * Only the aslwp can make this system call.
	 */
	if (curthread != p->p_aslwptp)
		return (set_errno(EINVAL));
	mutex_enter(&p->p_lock);
	/*
	 * set the thread's signal mask to unmask all signals.
	 */
	oldmask = t->t_hold;
	sigemptyset(&t->t_hold);
	for (;;) {
		getnotifysigs(&notifysigs, &p->p_notifsigs, &t->t_sig);
		if (!sigisempty(&notifysigs))
			break;
		else {
			/*
			 * Mask all signals in notification set, so that, in
			 * case the same signal number appears in both the
			 * notification set and t_sig, it does not cause the
			 * aslwp to spin in the following loop:
				do {
					getnotifysigs();
					if (sigisempty())
						cv_wait_sig_swap();
				} while (1);
			 * leaving the signal in t_sig and not blocked in t_hold
			 * will force a wake-up of cv_wait_sig_swap(). The aslwp
			 * will then call getnotifysigs() again, to return an
			 * empty set again and so call cv_wait_sig_swap...
			 */
			t->t_hold = p->p_notifsigs;
			if (cv_wait_sig_swap(&p->p_notifcv, &p->p_lock) == 0) {
				/*
				 * cv_wait() above was interrupted by a
				 * significant event such as EXITLWPS being set
				 * in p_flag, etc. The return code of 0 from
				 * cv_wait() above indicates that such an event
				 * occurred. Return from signotifywait() in this
				 * case.
				 */
				t->t_hold = oldmask;
				mutex_exit(&p->p_lock);
				return (set_errno(EINTR));
			}
			/*
			 * Do not need to restore t_hold after the wake-up
			 * since the next change to t_hold is an assignment.
			 */
		}
	}
	ASSERT(!sigisempty(&notifysigs));
	sig = fsignum(&notifysigs);
	ASSERT(sig > 0);
	sigdelset(&t->t_sig, sig);
	sigaddset(&p->p_notifsigs, sig);
	t->t_hold = oldmask;
	mutex_exit(&p->p_lock);
	return (sig);
}

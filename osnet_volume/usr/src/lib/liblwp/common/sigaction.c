/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sigaction.c	1.2	99/11/15 SMI"

#include "liblwp.h"
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <siginfo.h>
#include <ucontext.h>

/* protects _siguhandler[], _sa_mask[], and _sa_flags[] */
mutex_t	sig_lock[NSIG];
mutex_t	def_sig_lock = DEFAULTMUTEX;

/* this lives in libc */
extern void (*_siguhandler[NSIG])(int, siginfo_t *, void *);
sigset_t _sa_mask[NSIG];	/* sa_mask values from sigaction() */
int	_sa_flags[NSIG];	/* sa_flags values from sigaction() */

sigset_t fillset;

/* this is the actual system call trap in libc */
extern int __sigaction(int, const struct sigaction *, struct sigaction *);

void
sigorset(sigset_t *s1, const sigset_t *s2)
{
	s1->__sigbits[0] |= s2->__sigbits[0];
	s1->__sigbits[1] |= s2->__sigbits[1];
	s1->__sigbits[2] |= s2->__sigbits[2];
	s1->__sigbits[3] |= s2->__sigbits[3];
}

extern	void	call_user_handler(int, siginfo_t *, ucontext_t *);
#pragma unknown_control_flow(call_user_handler)

/*
 * Common code for calling the user-specified signal handler.
 */
void
call_user_handler(int sig, siginfo_t *sip, ucontext_t *uap)
{
	ulwp_t *self = curthread;
	void (*handler)(int, siginfo_t *, void *);
	sigset_t sigmask;
	int flags;

	if (__td_event_report(self, TD_CATCHSIG)) {
		self->ul_td_evbuf.eventnum = TD_CATCHSIG;
		self->ul_td_evbuf.eventdata = (void *)sig;
		tdb_event_catchsig();
	}

	/*
	 * Get a self-consistent set of handler, mask, and flags.
	 */
	lmutex_lock(&sig_lock[sig]);
	handler = _siguhandler[sig];
	sigmask = _sa_mask[sig];
	flags = _sa_flags[sig];
	lmutex_unlock(&sig_lock[sig]);

	if (!(flags & SA_SIGINFO) || sip == NULL || sip->si_signo == 0)
		sip = NULL;

	/*
	 * Set the proper signal mask and call the user's signal handler.
	 * (We overrode the user-requested signal mask with
	 * fillset so we currently have all signals blocked.)
	 */
	sigorset(&sigmask, &uap->uc_sigmask);	/* mask at previous level */
	if (!(flags & SA_NODEFER))
		(void) _sigaddset(&sigmask, sig);	/* current signal */
	(void) _sigprocmask(SIG_SETMASK, &sigmask, NULL);
	__sighndlr(sig, sip, uap, handler);

#if defined(sparc) || defined(__sparc)
	/*
	 * If this is a floating point exception and the queue
	 * is non-empty, pop the top entry from the queue.  This
	 * is to maintain expected behavior.
	 */
	if (sig == SIGFPE && uap->uc_mcontext.fpregs.fpu_qcnt) {
		fpregset_t *fp = &uap->uc_mcontext.fpregs;

		if (--fp->fpu_qcnt > 0) {
			unsigned char i;
			struct fq *fqp;

			fqp = fp->fpu_q;
			for (i = 0; i < fp->fpu_qcnt; i++)
				fqp[i] = fqp[i+1];
		}
	}
#endif	/* sparc */

	(void) _setcontext(uap);
	panic("call_user_handler(): _setcontext() returned");
}

/*
 * take_deferred_signal() is called when ul_critical becomes zero
 * and a deferred signal has been recorded on the current thread.
 * We are out of the critical region and are ready to take a signal.
 */
void
take_deferred_signal(int sig)
{
	ulwp_t *self = curthread;
	siginfo_t siginfo;
	ucontext_t uc;
	volatile int returning;

	ASSERT(self->ul_critical == 0);
	ASSERT(self->ul_cursig == 0);

	returning = 0;
	(void) _getcontext(&uc);
	/*
	 * If the application signal handler calls setcontext() on
	 * the ucontext we give it, it returns here, then we return.
	 */
	if (returning)
		return;
	returning = 1;

	siginfo = self->ul_siginfo;
	uc.uc_sigmask = self->ul_prevmask;

	call_user_handler(sig, &siginfo, &uc);
	panic("take_deferred_signal(): call_user_handler() returned");
}

void
sigacthandler(int sig, siginfo_t *sip, ucontext_t *uap)
{
	ulwp_t *self = curthread;

	/*
	 * Do this in case we took a signal while in a cancellable system call.
	 * It does no harm if we were not in such a system call.
	 */
	self->ul_validregs = 0;
	if (sig != SIGCANCEL)
		self->ul_cancel_async = self->ul_save_async;

	/*
	 * If we are not in a critical region, take the signal now.
	 */
	if (self->ul_critical == 0) {
		call_user_handler(sig, sip, uap);
		panic("sigacthandler(): call_user_handler() returned");
	}

	/*
	 * We are in a critical region; defer this signal.  When we
	 * emerge from the region we will call take_deferred_signal().
	 */
	ASSERT(self->ul_cursig == 0);
	self->ul_cursig = (char)sig;
	if (sip != NULL)
		self->ul_siginfo = *sip;
	else
		self->ul_siginfo.si_signo = 0;
	self->ul_prevmask = uap->uc_sigmask;

	/*
	 * Return to the previous context with all signals blocked.
	 * We will restore the signal mask in take_deferred_signal().
	 */
	uap->uc_sigmask = fillset;
	(void) _setcontext(uap);
	panic("sigacthandler(): _setcontext() returned");
}

int
sigaction_internal(
	int sig, const struct sigaction *nact, struct sigaction *oact)
{
	struct sigaction tact;
	struct sigaction *tactp = NULL;
	void		(*ohandler)(int, siginfo_t *, void *);
	sigset_t	osa_mask;
	int		osa_flags;
	int		rv;

	if (sig <= 0 || sig >= NSIG) {
		errno = EINVAL;
		return (-1);
	}

	lmutex_lock(&sig_lock[sig]);

	ohandler = _siguhandler[sig];
	osa_mask = _sa_mask[sig];
	osa_flags = _sa_flags[sig];

	if (nact != NULL) {
		tact = *nact;
		tactp = &tact;

		/*
		 * To be compatible with the behavior of SunOS 4.x:
		 * If the new signal handler is SIG_IGN or SIG_DFL,
		 * do not change the signal's entry in the handler array.
		 * This allows a child of vfork(2) to set signal handlers
		 * to SIG_IGN or SIG_DFL without affecting the parent.
		 */
		if (tactp->sa_handler != SIG_DFL &&
		    tactp->sa_handler != SIG_IGN) {
			_siguhandler[sig] = tactp->sa_handler;
			_sa_mask[sig] = tactp->sa_mask;
			_sa_flags[sig] = tactp->sa_flags;
			tactp->sa_handler = sigacthandler;
			tactp->sa_mask = fillset;
			tactp->sa_flags &= ~SA_NODEFER;
		}
	}

	if ((rv = __sigaction(sig, tactp, oact)) == -1) {
		_siguhandler[sig] = ohandler;
		_sa_mask[sig] = osa_mask;
		_sa_flags[sig] = osa_flags;
	} else if (oact != NULL &&
	    oact->sa_handler != SIG_DFL && oact->sa_handler != SIG_IGN) {
		oact->sa_sigaction = ohandler;
		oact->sa_mask = osa_mask;
		oact->sa_flags = osa_flags;
	}

	lmutex_unlock(&sig_lock[sig]);
	return (rv);
}

/*
 * Delete the libthread-reserved signals from the set.
 */
void
delete_reserved_signals(sigset_t *set)
{
	(void) _sigdelset(set, SIGWAITING);
	(void) _sigdelset(set, SIGLWP);
	(void) _sigdelset(set, SIGCANCEL);
}

#pragma weak	sigaction		= _sigaction
#pragma weak	_liblwp_sigaction	= _sigaction
int
_sigaction(int sig, const struct sigaction *nact, struct sigaction *oact)
{
	struct sigaction tact;
	struct sigaction *tactp = NULL;

	if (nact != NULL) {
		switch (sig) {
		case SIGWAITING:
		case SIGLWP:
		case SIGCANCEL:
			errno = EINVAL;
			return (-1);
		}
		tact = *nact;
		tactp = &tact;
		delete_reserved_signals(&tact.sa_mask);
	}
	return (sigaction_internal(sig, tactp, oact));
}

/*
 * Called at library initialization to take over signal handling.
 * This is necessary if we are dlopen()ed afer signal handlers
 * have already been established.
 */
void
signal_init()
{
	int sig;
	struct sigaction act;

	(void) _sigfillset(&fillset);
	(void) _sigdelset(&fillset, SIGKILL);
	(void) _sigdelset(&fillset, SIGSTOP);

	sig_lock[0] = def_sig_lock;
	for (sig = 1; sig < NSIG; sig++) {
		sig_lock[sig] = def_sig_lock;
		if (__sigaction(sig, NULL, &act) == 0 &&
		    act.sa_sigaction != SIG_DFL &&
		    act.sa_sigaction != SIG_IGN) {
			_sa_mask[sig] = act.sa_mask;
			_sa_flags[sig] = act.sa_flags;
			act.sa_handler = sigacthandler;
			act.sa_mask = fillset;
			act.sa_flags &= ~SA_NODEFER;
			(void) __sigaction(sig, &act, NULL);
		}
	}
}

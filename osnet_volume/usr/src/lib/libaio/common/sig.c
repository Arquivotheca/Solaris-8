/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sig.c	1.28	99/11/08 SMI"

#include <signal.h>
#include <siginfo.h>
#include <ucontext.h>
#include <sys/reg.h>
#include <sys/param.h>
#include <wait.h>
#include <errno.h>
#include <sys/asm_linkage.h>
#include <sys/lwp.h>
#include <sys/t_lock.h>
#include <errno.h>
#include <sys/synch32.h>
#include <string.h>
#include <unistd.h>
#include "libaio.h"

lwp_mutex_t __sigio_pendinglock;	/* protects __sigio_pending */
int __sigio_pending = 0;		/* count of pending SIGIO signals */
int _sigio_enabled;			/* set if SIGIO has a signal handler */
static struct sigaction sigioact;
static void aiosigiohndlr(int, siginfo_t *, void *);
void aiosigcancelhndlr(int, siginfo_t *, void *);
int _aiosigaction(int, const struct sigaction *, struct sigaction *);
sigset_t __sigiomask;
struct sigaction  sigcanact;

#ifdef __STDC__
#pragma	weak sigaction = _aiosigaction
#endif

int
_aio_create_worker(aio_req_t *rp, int mode)
{
	struct aio_worker *aiowp, **workers, **nextworker;
	int *aio_workerscnt;
	caddr_t stk = 0;
	ucontext_t uc;
	int stksize;
	void (*func)(void *);

	sigset_t *set = &_worker_set;

	if (_aio_alloc_stack(__aiostksz, &stk) == 0) {
		return (-1);
	}

	/*
	 * Put the new worker lwp in the right queue.
	 */

	switch (mode) {
	case AIOWRITE:
		workers = &__workers_wr;
		nextworker = &__nextworker_wr;
		aio_workerscnt = &__wr_workerscnt;
		func = _aio_do_request;
		break;
	case AIOREAD:
		workers = &__workers_rd;
		nextworker = &__nextworker_rd;
		aio_workerscnt = &__rd_workerscnt;
		func = _aio_do_request;
		break;
	case AIOSIGEV:
		workers = &__workers_si;
		nextworker = &__nextworker_si;
		func = _aio_send_sigev;
		aio_workerscnt = &__si_workerscnt;
	}

	stksize = __aiostksz - (int32_t)sizeof (struct aio_worker);
	/*LINTED*/
	aiowp = (struct aio_worker *)(stk + stksize);
	/* initialize worker's private data */
	(void) memset(aiowp, 0, sizeof (aiowp));

	if (rp) {
		rp->req_state = AIO_REQ_QUEUED;
		rp->req_worker = aiowp;
		aiowp->work_head1 = rp;
		aiowp->work_tail1 = rp;
		aiowp->work_next1 = rp;
		aiowp->work_cnt1 = 1;
	}

	aiowp->work_stk = stk;
	stksize -= sizeof (double);
	(void) memset(&uc, 0, sizeof (ucontext_t));
	(void) memcpy(&uc.uc_sigmask, set, sizeof (sigset_t));
	_lwp_makecontext(&uc, func, aiowp, aiowp, stk, stksize);
	if (_lwp_create(&uc, NULL, &aiowp->work_lid)) {
		_aio_free_stack(__aiostksz, stk);
		return (-1);
	}

	_lwp_mutex_lock(&__aio_mutex);
	(*aio_workerscnt)++;
	if (*workers == NULL) {
		aiowp->work_forw = aiowp;
		aiowp->work_backw = aiowp;
		*nextworker = aiowp;
		*workers = aiowp;
	} else {
		aiowp->work_backw = (*workers)->work_backw;
		aiowp->work_forw = (*workers);
		(*workers)->work_backw->work_forw = aiowp;
		(*workers)->work_backw = aiowp;
	}
	_aio_worker_cnt++;
	_lwp_mutex_unlock(&__aio_mutex);

	return (0);
}

void
_aio_cancel_on(struct aio_worker *aiowp)
{
	aiowp->work_cancel_flg = 1;
}

void
_aio_cancel_off(struct aio_worker *aiowp)
{
	aiowp->work_cancel_flg = 0;
}

/*
 * resend a SIGIO signal that was sent while the
 * __aio_mutex was locked.
 */
void
__aiosendsig(void)
{
	(void) sigprocmask(SIG_BLOCK, &__sigiomask, NULL);

	_lwp_mutex_lock(&__sigio_pendinglock);
	__sigio_pending = 0;
	_lwp_mutex_unlock(&__sigio_pendinglock);

	(void) sigprocmask(SIG_UNBLOCK, &__sigiomask, NULL);

	(void) kill(__pid, SIGIO);
}

/*
 * this is the low-level handler for SIGIO. the application
 * handler will not be called if the signal is being blocked.
 */
static void
aiosigiohndlr(int sig, siginfo_t *sip, void *uap)
{
	/*
	 * SIGIO signal is being blocked if either _sigio_masked
	 * or sigio_maskedcnt is set or if both these variables
	 * are clear and the _aio_mutex is locked. the last
	 * condition can only happen when _aio_mutex is being
	 * unlocked. this is a very small window where the mask
	 * is clear and the lock is about to be unlocked, however,
	 * it`s still set and so the signal should be defered.
	 */
	if ((__sigio_masked | __sigio_maskedcnt) ||
	    (MUTEX_HELD(&__aio_mutex))) {
		/*
		 * aio_lock() is supposed to be non re-entrant with
		 * respect to SIGIO signals. if a SIGIO signal
		 * interrupts a region of code locked by _aio_mutex
		 * the SIGIO signal should be deferred until this
		 * mutex is unlocked. a flag is set, sigio_pending,
		 * to indicate that a SIGIO signal is pending and
		 * should be resent to the process via a kill().
		 */
		_lwp_mutex_lock(&__sigio_pendinglock);
		__sigio_pending = 1;
		_lwp_mutex_unlock(&__sigio_pendinglock);
	} else {
		/*
		 * call the real handler.
		 */
		(sigioact.sa_sigaction)(sig, sip, uap);
	}
}

void
aiosigcancelhndlr(int sig, siginfo_t *sip, void *uap)
{
	struct aio_worker *aiowp;
	struct sigaction act;

	if (sip != NULL && sip->si_code == SI_LWP) {
		aiowp = (struct aio_worker *)_lwp_getprivate();
		ASSERT(aiowp != NULL);
		if (aiowp->work_cancel_flg)
			longjmp(aiowp->work_jmp_buf, 1);
	} else if (sigcanact.sa_handler == SIG_DFL) {
		act.sa_handler = SIG_DFL;
		_sigaction(SIGAIOCANCEL, &act, NULL);
		kill(getpid(), sig);
	} else if (sigcanact.sa_handler != SIG_IGN) {
		(sigcanact.sa_sigaction)(sig, sip, uap);
	}
}

int
_aiosigaction(int sig, const struct sigaction *nact, struct sigaction *oact)
{
	struct sigaction tact;
	struct sigaction *tactp;
	struct sigaction oldact;

	if (sig <= 0 || sig >= NSIG) {
		errno = EINVAL;
		return (-1);
	}

	/*
	 * Only interpose on SIGIO when it is given a dispostion other than
	 * SIG_IGN, or SIG_DFL. Because SIGAIOCANCEL is SIGPROF, this
	 * signal always should be interposed on, so that SIGPROF can
	 * also be used by the application for profiling.
	 */
	if (nact &&
	    (((sig == SIGIO) && (nact->sa_handler != SIG_DFL &&
	    nact->sa_handler != SIG_IGN)) ||
	    (sig == SIGAIOCANCEL))) {
		tactp = &tact;
		*tactp = *nact;
		if (sig == SIGIO) {
			_sigio_enabled = 1;
			*(&oldact) = *(&sigioact);
			*(&sigioact) = *tactp;
			tactp->sa_sigaction = aiosigiohndlr;
			sigaddset(&tactp->sa_mask, SIGIO);
			if (_sigaction(sig, tactp, oact) == -1) {
				*(&sigioact) = *(&oldact);
				return (-1);
			}
		} else {
			*(&oldact) = *(&sigcanact);
			*(&sigcanact) = *tactp;
			tactp->sa_sigaction = aiosigcancelhndlr;
			tactp->sa_flags |= SA_SIGINFO;
			sigaddset(&tactp->sa_mask, SIGAIOCANCEL);
			if (_sigaction(sig, tactp, oact) == -1) {
				*(&sigcanact) = *(&oldact);
				return (-1);
			}
		}
		if (oact)
			*oact = *(&oldact);
		return (0);
	}

	if (oact && (sig == SIGIO || sig == SIGAIOCANCEL)) {
		if (sig == SIGIO)
			*oact = *(&sigioact);
		else
			*oact = *(&sigcanact);
		return (0);
	} else
		return (_sigaction(sig, nact, oact));
}

/*
 * Check for valid signal number as per SVID.
 */
#define	CHECK_SIG(s, code) \
	if ((s) <= 0 || (s) >= NSIG || (s) == SIGKILL || (s) == SIGSTOP) { \
		errno = EINVAL; \
		return (code); \
	}

/*
 * Equivalent to stopdefault set in the kernel implementation (sig.c).
 */
#define	STOPDEFAULT(s) \
	((s) == SIGSTOP || (s) == SIGTSTP || (s) == SIGTTOU || (s) == SIGTTIN)


/*
 * SVr3.x signal compatibility routines. They are now
 * implemented as library routines instead of system
 * calls.
 */

void(*
signal(int sig, void(*func)(int)))(int)
{
	struct sigaction nact;
	struct sigaction oact;

	CHECK_SIG(sig, SIG_ERR);

	nact.sa_handler = func;
	nact.sa_flags = SA_RESETHAND|SA_NODEFER;
	(void) _sigemptyset(&nact.sa_mask);

	/*
	 * Pay special attention if sig is SIGCHLD and
	 * the disposition is SIG_IGN, per sysV signal man page.
	 */
	if (sig == SIGCHLD) {
		nact.sa_flags |= SA_NOCLDSTOP;
		if (func == SIG_IGN)
			nact.sa_flags |= SA_NOCLDWAIT;
	}

	if (STOPDEFAULT(sig))
		nact.sa_flags |= SA_RESTART;

	if (_aiosigaction(sig, &nact, &oact) < 0)
		return (SIG_ERR);

	return (oact.sa_handler);
}

int
sigignore(int sig)
{
	struct sigaction act;
	sigset_t set;

	CHECK_SIG(sig, -1);

	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	(void) _sigemptyset(&act.sa_mask);

	/*
	 * Pay special attention if sig is SIGCHLD and
	 * the disposition is SIG_IGN, per sysV signal man page.
	 */
	if (sig == SIGCHLD) {
		act.sa_flags |= SA_NOCLDSTOP;
		act.sa_flags |= SA_NOCLDWAIT;
	}

	if (STOPDEFAULT(sig))
		act.sa_flags |= SA_RESTART;

	if (_aiosigaction(sig, &act, (struct sigaction *)0) < 0)
		return (-1);

	(void) _sigemptyset(&set);
	if (_sigaddset(&set, sig) < 0)
		return (-1);
	return (_sigprocmask(SIG_UNBLOCK, &set, (sigset_t *)0));
}

void(*
sigset(int sig, void(*func)(int)))(int)
{
	struct sigaction nact;
	struct sigaction oact;
	sigset_t nset;
	sigset_t oset;
	int code;

	CHECK_SIG(sig, SIG_ERR);

	(void) _sigemptyset(&nset);
	if (_sigaddset(&nset, sig) < 0)
		return (SIG_ERR);

	if (func == SIG_HOLD) {
		if (_sigprocmask(SIG_BLOCK, &nset, &oset) < 0)
			return (SIG_ERR);
		if (_aiosigaction(sig, (struct sigaction *)0, &oact) < 0)
			return (SIG_ERR);
	} else {
		nact.sa_handler = func;
		nact.sa_flags = 0;
		(void) _sigemptyset(&nact.sa_mask);
		/*
		 * Pay special attention if sig is SIGCHLD and
		 * the disposition is SIG_IGN, per sysV signal man page.
		 */
		if (sig == SIGCHLD) {
			nact.sa_flags |= SA_NOCLDSTOP;
			if (func == SIG_IGN)
				nact.sa_flags |= SA_NOCLDWAIT;
		}

		if (STOPDEFAULT(sig))
			nact.sa_flags |= SA_RESTART;

		if (_aiosigaction(sig, &nact, &oact) < 0)
			return (SIG_ERR);

		if (_sigprocmask(SIG_UNBLOCK, &nset, &oset) < 0)
			return (SIG_ERR);
	}

	if ((code = _sigismember(&oset, sig)) < 0)
		return (SIG_ERR);
	else if (code == 1)
		return (SIG_HOLD);

	return (oact.sa_handler);
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *	Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)sigaction.c	1.18	98/01/30 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include <signal.h>
#include <errno.h>
#include <siginfo.h>
#include <ucontext.h>
#include "libc.h"

void (*_siguhandler[NSIG])(int, siginfo_t *, void *) = { 0 };

extern int __sigaction(int, const struct sigaction *, struct sigaction *);

static void
sigacthandler(int sig, siginfo_t *sip, void *state)
{
	ucontext_t	*uap = state;

	(*_siguhandler[sig])(sig, sip, uap);

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

	(void) setcontext(uap);
}

int
_libc_sigaction(int sig, const struct sigaction *nact, struct sigaction *oact)
{
	struct sigaction	tact;
	struct sigaction	*tactp;
	void			(*ohandler)(int, siginfo_t *, void *);

	if (sig <= 0 || sig >= NSIG) {
		errno = EINVAL;
		return (-1);
	}

	ohandler = _siguhandler[sig];

	if ((tactp = (struct sigaction *)nact) != NULL) {
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
			tactp->sa_handler = sigacthandler;
		}
	}

	if (__sigaction(sig, tactp, oact) == -1) {
		_siguhandler[sig] =
			(void (*) (int, siginfo_t *, void *)) ohandler;
		return (-1);
	}

	if (oact && oact->sa_handler != SIG_DFL && oact->sa_handler != SIG_IGN)
		oact->sa_sigaction = ohandler;

	return (0);
}

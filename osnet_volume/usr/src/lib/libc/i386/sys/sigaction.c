/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sigaction.c	1.5	96/10/15 SMI"

#include "synonyms.h"
#include <signal.h>
#include <errno.h>
#include <siginfo.h>
#include <ucontext.h>

void (*_siguhandler[NSIG])() = { 0 };

static void
sigacthandler(sig, sip, uap)
	int sig;
	siginfo_t *sip;
	ucontext_t *uap;
{
	(*_siguhandler[sig])(sig, sip, uap);
	setcontext(uap);
}

_libc_sigaction(sig, nact, oact)
	int sig;
	const struct sigaction *nact;
	struct sigaction *oact;
{
	struct sigaction tact;
	register struct sigaction *tactp;
	void (*ohandler)();

	if (sig <= 0 || sig >= NSIG) {
		errno = EINVAL;
		return (-1);
	}

	ohandler = _siguhandler[sig];

	if (tactp = (struct sigaction *)nact) {
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
		_siguhandler[sig] = ohandler;
		return (-1);
	}

	if (oact && oact->sa_handler != SIG_DFL && oact->sa_handler != SIG_IGN)
		oact->sa_handler = ohandler;

	return (0);
}

/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)exit.c	1.15	99/10/22 SMI"

/*
 * Utility functions
 */
#include	<unistd.h>
#include	<signal.h>
#include	<locale.h>
#include	<errno.h>
#include	<string.h>
#include	"msg.h"
#include	"_libld.h"
#include	"_ld.h"

/*
 * Exit after cleaning up.
 */
void
ldexit()
{
	/*
	 * If we have created an output file remove it.
	 */
	if (Ofl.ofl_fd > 0)
		(void) unlink(Ofl.ofl_name);

	/*
	 * Inform any support library that the link-edit has failed.
	 */
	lds_atexit(EXIT_FAILURE);

	exit(EXIT_FAILURE);
	/* NOTREACHED */
}

/*
 * Establish the signals we're interested in, and the handlers that need to be
 * reinstalled should any of these signals occur.
 */
typedef struct {
	int	signo;
	void (*	defhdl)();
} Signals;

Signals signals[] = {	{ SIGHUP,	SIG_DFL },
			{ SIGINT,	SIG_IGN },
			{ SIGQUIT,	SIG_DFL },
			{ SIGBUS,	SIG_DFL },
			{ SIGTERM,	SIG_IGN },
			{ 0,		0 } };

/*
 * Define our signal handler.
 */
static void
/* ARGSUSED2 */
handler(int sig, siginfo_t * sip, void * utp)
{
	struct sigaction	nact;
	Signals *		sigs;

	/*
	 * Reset all ignore handlers regardless of how we got here.
	 */
	nact.sa_handler = SIG_IGN;
	nact.sa_flags = 0;
	(void) sigemptyset(&nact.sa_mask);

	for (sigs = signals; sigs->signo; sigs++) {
		if (sigs->defhdl == SIG_IGN)
			(void) sigaction(sigs->signo, &nact, NULL);
	}

	/*
	 * The model for creating an output file is to ftruncate() it to the
	 * required size and mmap() a mapping into which the new contents are
	 * written.  Neither of these operations guarantee that the required
	 * disk blocks exist, and should we run out of disk space a bus error
	 * is generated.
	 * Other situations have been reported to result in ld catching a bus
	 * error (one instance was a stale NFS handle from an unstable server).
	 * Thus we catch all bus errors and hope we can decode a better error.
	 */
	if ((sig == SIGBUS) && sip && Ofl.ofl_name) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_FIL_INTERRUPT), Ofl.ofl_name,
		    strerror(sip->si_errno));
	}
	ldexit();
}


/*
 * Establish a signal handler for all signals we're interested in.
 */
void
init()
{
	struct sigaction	nact, oact;
	Signals *		sigs;

	/*
	 * For each signal we're interested in set up a signal handler that
	 * insures we clean up any output file we're in the middle of creating.
	 */
	nact.sa_sigaction = handler;
	(void) sigemptyset(&nact.sa_mask);

	for (sigs = signals; sigs->signo; sigs++) {
		if ((sigaction(sigs->signo, NULL, &oact) == 0) &&
		    (oact.sa_handler != SIG_IGN)) {
			nact.sa_flags = SA_SIGINFO;
			if (sigs->defhdl == SIG_DFL)
				nact.sa_flags |= (SA_RESETHAND | SA_NODEFER);
			(void) sigaction(sigs->signo, &nact, NULL);
		}
	}
}

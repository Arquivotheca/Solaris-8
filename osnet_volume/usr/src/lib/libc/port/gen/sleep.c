/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident  "@(#)sleep.c 1.19     96/12/06 SMI"        /* SVr4.0 2.12.1.4      */

/*LINTLIBRARY*/

/*
 * Suspend the process for `sleep_tm' seconds - using alarm/pause
 * system calls.  If caller had an alarm already set to go off `n'
 * seconds from now, then Case 1: (sleep_tm >= n) sleep for n, and
 * cause the callers previously specified alarm interrupt routine
 * to be executed, then return the value (sleep_tm - n) to the caller
 * as the unslept amount of time, Case 2: (sleep_tm < n) sleep for
 * sleep_tm, after which, reset alarm to go off when it would have
 * anyway.  In case process is aroused during sleep by any caught
 * signal, then reset any prior alarm, as above, and return to the
 * caller the (unsigned) quantity of (requested) seconds unslept.
 *
 * For SVR4 sleep uses the new sigaction system call which restores
 * the proper flavor of the old SIGALRM signal handler, i.e. whether
 * it was set via signal() or sigset()
 */


#ifndef lint
#include "synonyms.h"
#endif

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include "libc.h"


static void awake(int); 

/* ARGSUSED */
static void
awake(sig)
	int sig;
{
}

unsigned int
_libc_sleep(unsigned int sleep_tm)
{
	int  alrm_flg;
	unsigned unslept, alrm_tm, left_ovr;


/* variables for the sigaction version */
	struct sigaction nact;
	struct sigaction oact;
	sigset_t alrm_mask;
	sigset_t nset;
	sigset_t oset;

	if (sleep_tm == 0)
		return (0);

	alrm_tm = alarm(0);			/* prev. alarm time */
	nact.sa_handler = awake;
	nact.sa_flags = 0;
	(void) sigemptyset(&nact.sa_mask);
	(void) sigaction(SIGALRM, &nact, &oact);

	alrm_flg = 0;
	left_ovr = 0;

	if (alrm_tm != 0) {	/* skip all this ifno prev. alarm */
		if (alrm_tm > sleep_tm) {
			alrm_tm -= sleep_tm;
			++alrm_flg;
		} else {
			left_ovr = sleep_tm - alrm_tm;
			sleep_tm = alrm_tm;
			alrm_tm = 0;
			--alrm_flg;
			(void) sigaction(SIGALRM, &oact, (struct sigaction *)0);
		}
	}

	(void) sigemptyset(&alrm_mask);
	(void) sigaddset(&alrm_mask, SIGALRM);
	(void) sigprocmask(SIG_BLOCK, &alrm_mask, &oset);
	nset = oset;
	(void) sigdelset(&nset, SIGALRM);
	(void) alarm(sleep_tm);
	(void) sigsuspend(&nset);
	unslept = alarm(0);
	if (!sigismember(&oset, SIGALRM))
		(void) sigprocmask(SIG_UNBLOCK, &alrm_mask, (sigset_t *)0);
	if (alrm_flg >= 0)
		(void) sigaction(SIGALRM, &oact, (struct sigaction *)0);

	if (alrm_flg > 0 || (alrm_flg < 0 && unslept != 0))
		(void) alarm(alrm_tm + unslept);
	return (left_ovr + unslept);
}

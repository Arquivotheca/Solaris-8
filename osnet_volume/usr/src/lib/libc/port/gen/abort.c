/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)abort.c	1.15	99/05/04 SMI"

/*LINTLIBRARY*/

#ifndef lint
#include "synonyms.h"
#endif
#include "file64.h"
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "stdiom.h"

static int pass = 0;	/* counts how many times abort has been called */

/*
 * abort() - terminate current process with dump via SIGABRT
 */
void
abort(void)
{
	sigset_t	set;
	struct sigaction	act;

	if (!sigaction(SIGABRT, NULL, &act) &&
	    act.sa_handler != SIG_DFL && act.sa_handler != SIG_IGN) {
		/*
		 * User handler is installed, invokes user handler before
		 * taking default action.
		 *
		 * Send SIGABRT, unblock SIGABRT if blocked.
		 * If there is pending signal SIGABRT, we only need to unblock
		 * SIGABRT.
		 */
		if (!sigprocmask(SIG_SETMASK, NULL, &set) &&
		    sigismember(&set, SIGABRT)) {
			if (!sigpending(&set) && !sigismember(&set, SIGABRT))
				(void) raise(SIGABRT);
			(void) sigrelse(SIGABRT);
		} else
			(void) raise(SIGABRT);
	}

	if (++pass == 1)
		_cleanup();

	for (;;) {
		(void) signal(SIGABRT, SIG_DFL);
		(void) sigrelse(SIGABRT);
		(void) raise(SIGABRT);
	}
}

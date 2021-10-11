/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)siginterrupt.c	1.3	96/12/02 SMI"

/*LINTLIBRARY*/

#pragma weak siginterrupt = _siginterrupt

#include "synonyms.h"
#include <sys/types.h>
#include <errno.h>
#include <signal.h>

int
siginterrupt(int sig, int flag)
{
	struct sigaction act;

	/*
	 * Check for valid signal number
	 */
	if (sig <= 0 || sig >= NSIG) {
		errno = EINVAL;
		return (-1);
	}

	(void) sigaction(sig, NULL, &act);
	if (flag)
		act.sa_flags &= ~SA_RESTART;
	else
		act.sa_flags |= SA_RESTART;
	return (sigaction(sig, &act, NULL));
}

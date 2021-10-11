/*
 * Copyright (c) 1995-1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident   "@(#)posix_sigwait.c 1.4     97/08/08 SMI"

/*LINTLIBRARY*/

#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include "libc.h"

/*
 * POSIX.1c version of the sigwait.
 * User gets it via static sigwait from header file.
 */
int
__posix_sigwait(const sigset_t *setp, int *signo)
{
	int nerrno = 0;
	int oerrno = errno;

	errno = 0;
	if ((*signo = _sigwait((sigset_t *)setp)) == -1) {
		if (errno == 0)
			errno = EINVAL;
		else
			nerrno = errno;
	}
	errno = oerrno;
	return (nerrno);
}

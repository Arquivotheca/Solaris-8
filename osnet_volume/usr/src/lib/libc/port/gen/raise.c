/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)raise.c	1.12	98/11/06 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <thread.h>
#include <errno.h>
#include "mtlib.h"
#include "libc.h"

int
raise(int sig)
{
	int error;

	if (!__threaded)
		return (kill(getpid(), sig));
	if ((error = _thr_kill(_thr_self(), sig)) != 0) {
		errno = error;
		return (-1);
	}
	return (0);
}

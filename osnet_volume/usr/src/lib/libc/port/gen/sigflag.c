/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sigflag.c	1.8	96/12/03 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

/* change state of signal flag */

#pragma weak sigflag = _sigflag

#include "synonyms.h"
#include <sys/types.h>
#include <signal.h>

int
sigflag(int sig, int on, int flag)
{
	struct sigaction sa;
	int v;

	if ((v = sigaction(sig, 0, &sa)) < 0)
		return (v);
	if (on)
		sa.sa_flags |= flag;
	else
		sa.sa_flags &= ~flag;
	return (sigaction(sig, &sa, 0));
}

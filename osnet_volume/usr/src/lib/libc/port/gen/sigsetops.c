/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sigsetops.c	1.12	99/05/04 SMI"
					/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

/*
 * POSIX signal manipulation functions.
 */
#pragma weak sigfillset = _sigfillset
#pragma weak sigemptyset = _sigemptyset
#pragma weak sigaddset = _sigaddset
#pragma weak sigdelset = _sigdelset
#pragma weak sigismember = _sigismember

#include "synonyms.h"
#include <sys/types.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <errno.h>
#include "libc.h"

#define	SIGSETSIZE	4
#define	MAXBITNO (NBPW*8)

static sigset_t sigs;
static int sigsinit;

#define	sigword(n) ((n-1)/MAXBITNO)
#define	bitmask(n) (1L<<((n-1)%MAXBITNO))

static int
sigvalid(int sig)
{
	if (sig <= 0 || sig > (MAXBITNO * SIGSETSIZE))
		return (0);

	if (!sigsinit) {
		(void) __sigfillset(&sigs);
		sigsinit++;
	}

	return ((sigs.__sigbits[sigword(sig)] & bitmask(sig)) != 0);
}

int
sigfillset(sigset_t *set)
{
	if (!sigsinit) {
		(void) __sigfillset(&sigs);
		sigsinit++;
	}

	*set = sigs;
	return (0);
}

int
sigemptyset(sigset_t *set)
{
	set->__sigbits[0] = 0;
	set->__sigbits[1] = 0;
	set->__sigbits[2] = 0;
	set->__sigbits[3] = 0;
	return (0);
}

int
sigaddset(sigset_t *set, int sig)
{
	if (!sigvalid(sig)) {
		errno = EINVAL;
		return (-1);
	}
	set->__sigbits[sigword(sig)] |= bitmask(sig);
	return (0);
}

int
sigdelset(sigset_t *set, int sig)
{
	if (!sigvalid(sig)) {
		errno = EINVAL;
		return (-1);
	}
	set->__sigbits[sigword(sig)] &= ~bitmask(sig);
	return (0);
}

int
sigismember(const sigset_t *set, int sig)
{
	if (!sigvalid(sig)) {
		errno = EINVAL;
		return (-1);
	}
	return ((set->__sigbits[sigword(sig)] & bitmask(sig)) != 0);
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)clearerr.c	1.12	97/12/02 SMI"	/* SVr4.0 1.3 */

/*LINTLIBRARY*/

#ifdef _REENTRANT
#pragma weak clearerr_unlocked = _clearerr_unlocked
#endif

#include "synonyms.h"
#include "file64.h"
#include <mtlib.h>
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include <sys/types.h>
#include "stdiom.h"

#undef clearerr

#ifdef _REENTRANT
#undef _clearerr_unlocked

void
clearerr(FILE *iop)
{
	rmutex_t *lk;

	FLOCKFILE(lk, iop);
	iop->_flag &= ~(_IOERR | _IOEOF);
	FUNLOCKFILE(lk);
}
#else
#define	_clearerr_unlocked clearerr
#endif _REENTRANT

void
_clearerr_unlocked(FILE *iop)
{
	iop->_flag &= ~(_IOERR | _IOEOF);
}

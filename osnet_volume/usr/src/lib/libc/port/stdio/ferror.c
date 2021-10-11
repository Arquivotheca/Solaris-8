/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ferror.c	1.13	97/12/02 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#ifdef _REENTRANT
#pragma weak ferror_unlocked = _ferror_unlocked
#endif

#include "synonyms.h"
#include "file64.h"
#include "mtlib.h"
#include <sys/types.h>
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include "stdiom.h"

#undef ferror

#ifdef _REENTRANT
#undef ferror_unlocked

int
ferror(FILE *iop)
{
	FLOCKRETURN(iop, iop->_flag & _IOERR)
}
#else
#define	_ferror_unlocked ferror
#endif _REENTRANT

int
_ferror_unlocked(FILE *iop)
{
	return (iop->_flag & _IOERR);
}

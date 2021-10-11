/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)fileno.c	1.15	98/07/13 SMI"	/* SVr4.0 1.6 */

/*LINTLIBRARY*/

#ifdef _REENTRANT
#pragma weak fileno_unlocked = _fileno_unlocked
#endif
#pragma weak fileno = _fileno

#include "synonyms.h"
#include "file64.h"
#include "mtlib.h"
#include <sys/types.h>
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include "stdiom.h"

#ifdef _REENTRANT
#undef _fileno_unlocked

int
_fileno(FILE *iop)
{
	FLOCKRETURN(iop, iop->_file)
}
#else
#define	_fileno_unlocked _fileno
#endif _REENTRANT


int
_fileno_unlocked(FILE *iop)
{
	return (iop->_file);
}

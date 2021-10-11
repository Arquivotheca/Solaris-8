/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)feof.c	1.13	97/12/02 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#ifdef _REENTRANT
#pragma weak feof_unlocked = _feof_unlocked
#endif

#include "synonyms.h"
#include <mtlib.h>
#include "file64.h"
#include <sys/types.h>
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include "stdiom.h"

#undef feof

#ifdef _REENTRANT
#undef _feof_unlocked

int
feof(FILE *iop)
{
	FLOCKRETURN(iop, iop->_flag & _IOEOF)
}
#else
#define	_feof_unlocked feof
#endif _REENTRANT


int
_feof_unlocked(FILE *iop)
{
	return (iop->_flag & _IOEOF);
}

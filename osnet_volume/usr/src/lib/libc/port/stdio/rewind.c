/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rewind.c	1.19	97/12/06 SMI"	/* SVr4.0 1.7	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include "file64.h"
#include "mtlib.h"
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <synch.h>
#include <thread.h>
#include "stdiom.h"
#include "libc.h"


void
rewind(FILE *iop)
{
	rmutex_t *lk;

	FLOCKFILE(lk, iop);
	_rewind_unlocked(iop);
	FUNLOCKFILE(lk);
}

void
_rewind_unlocked(FILE *iop)
{
	(void) _fflush_u(iop);
	(void) lseek64(FILENO(iop), 0, SEEK_SET);
	iop->_cnt = 0;
	iop->_ptr = iop->_base;
	iop->_flag &= ~(_IOERR | _IOEOF);
	if (iop->_flag & _IORW)
		iop->_flag &= ~(_IOREAD | _IOWRT);
}

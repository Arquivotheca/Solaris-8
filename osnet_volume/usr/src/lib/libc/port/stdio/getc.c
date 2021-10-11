/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getc.c	1.17	99/11/03 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#pragma weak getc_unlocked = _getc_unlocked

#include "synonyms.h"
#include "file64.h"
#include "mtlib.h"
#include <sys/types.h>
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include "stdiom.h"
#include "mse.h"

#undef getc

#undef _getc_unlocked

int
getc(FILE *iop)
{
	rmutex_t *lk;
	int c;

	FLOCKFILE(lk, iop);

	_SET_ORIENTATION_BYTE(iop);

	c = (--iop->_cnt < 0) ? _filbuf(iop) : *iop->_ptr++;
	FUNLOCKFILE(lk);
	return (c);
}


int
_getc_unlocked(FILE *iop)
{
	return ((--iop->_cnt < 0) ? _filbuf(iop) : *iop->_ptr++);
}

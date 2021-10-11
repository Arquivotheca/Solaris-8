/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)putc.c	1.17	99/11/03 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#pragma weak putc_unlocked = _putc_unlocked

#include "synonyms.h"
#include "file64.h"
#include "mtlib.h"
#include <sys/types.h>
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include "stdiom.h"
#include "mse.h"

#undef putc

#undef putc_unlocked

int
putc(int ch, FILE *iop)
{
	rmutex_t *lk;
	int ret;

	FLOCKFILE(lk, iop);

	_SET_ORIENTATION_BYTE(iop);

	if (--iop->_cnt < 0)
		ret = _flsbuf((unsigned char) ch, iop);
	else {
		(*iop->_ptr++) = (unsigned char)ch;
		ret = (unsigned char)ch;
	}
	FUNLOCKFILE(lk);
	return (ret);
}


int
_putc_unlocked(int ch, FILE *iop)
{
	if (--iop->_cnt < 0)
		return (_flsbuf((unsigned char) ch, iop));
	else
		return (*iop->_ptr++ = (unsigned char)ch);
}

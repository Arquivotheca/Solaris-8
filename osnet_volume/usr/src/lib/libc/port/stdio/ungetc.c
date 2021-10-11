/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ungetc.c	1.11	97/12/02 SMI"	/* SVr4.0 2.11	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include "shlib.h"
#include "file64.h"
#include "mtlib.h"
#include <sys/types.h>
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include "stdiom.h"

int _ungetc_unlocked(int c, FILE *iop);

#ifdef _REENTRANT
int
ungetc(int c, FILE *iop)
{
	FLOCKRETURN(iop, _ungetc_unlocked(c, iop))
}
#else
#define	_ungetc_unlocked ungetc
#endif
/*
 * Called internally by the library (instead of the safe "ungetc") when
 * iop->_lock is already held at a higher level - required since we do not
 * have recursive locks.
 */
int
_ungetc_unlocked(int c, FILE *iop)
{
	if (c == EOF)
		return (EOF);
	if (iop->_ptr <= iop->_base) {
		if (iop->_base == 0) {
			if (_findbuf(iop) == 0)
				return (EOF);
		} else if (iop->_ptr <= iop->_base - PUSHBACK)
			return (EOF);
	}
	if ((iop->_flag & _IOREAD) == 0) /* basically a no-op on write stream */
		++iop->_ptr;
	if (*--iop->_ptr != (unsigned char) c)
		*iop->_ptr = (unsigned char) c;  /* was *--iop->_ptr = c; */
	++iop->_cnt;
	iop->_flag &= ~_IOEOF;
	return (c);
}

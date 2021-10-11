/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_wrtchk.c	1.10	97/12/02 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include "shlib.h"
#include "file64.h"
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include "stdiom.h"

#undef _wrtchk

/* check permissions, correct for read & write changes */
int
_wrtchk(FILE *iop)
{
	if ((iop->_flag & (_IOWRT | _IOEOF)) != _IOWRT) {
		if (!(iop->_flag & (_IOWRT | _IORW))) {
			iop->_flag |= _IOERR;
			errno = EBADF;
			return (EOF); /* stream is not writeable */
		}
		iop->_flag = (iop->_flag & ~_IOEOF) | _IOWRT;
	}

	/* if first I/O to the stream get a buffer */
	if (iop->_base == NULL && _findbuf(iop) == NULL)
		return (EOF);
	else if ((iop->_ptr == iop->_base) &&
	    !(iop->_flag & (_IOLBF | _IONBF))) {
		iop->_cnt = _bufend(iop) - iop->_ptr;
	}
	return (0);
}

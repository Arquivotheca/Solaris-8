/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)fseek.c	1.19	97/12/02 SMI"	/* SVr4.0 1.15 */

/*LINTLIBRARY*/
/*
 * Seek for standard library.  Coordinates with buffering.
 */

#pragma weak fseek = _fseek

#include "synonyms.h"
#include "shlib.h"
#include "file64.h"
#include "mtlib.h"
#include <stdio.h>
#include <errno.h>
#include <thread.h>
#include <synch.h>
#include <unistd.h>
#include <sys/types.h>
#include "stdiom.h"

int
fseek(FILE *iop, long offset, int ptrname)
{
	off_t	p;
#ifdef _REENTRANT
	rmutex_t *lk;
#endif _REENTRANT

	FLOCKFILE(lk, iop);
	iop->_flag &= ~_IOEOF;

	if (!(iop->_flag & _IOREAD) && !(iop->_flag & (_IOWRT | _IORW))) {
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (-1);
	}

	if (iop->_flag & _IOREAD) {
		if (ptrname == 1 && iop->_base && !(iop->_flag&_IONBF)) {
			offset -= iop->_cnt;
		}
	} else if (iop->_flag & (_IOWRT | _IORW)) {
		if (_fflush_u(iop) == EOF) {
			FUNLOCKFILE(lk);
			return (-1);
		}
	}
	iop->_cnt = 0;
	iop->_ptr = iop->_base;
	if (iop->_flag & _IORW) {
		iop->_flag &= ~(_IOREAD | _IOWRT);
	}
	p = lseek(FILENO(iop), (off_t)offset, ptrname);
	FUNLOCKFILE(lk);
	return ((p == (off_t)-1) ? -1: 0);
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#pragma	ident	"@(#)ftell.c	1.20	97/12/02 SMI"	/* SVr4.0 1.13 */

/*LINTLIBRARY*/
/*
 * Return file offset.
 * Coordinates with buffering.
 */
#pragma weak ftell = _ftell

#include "synonyms.h"
#include "file64.h"
#include "mtlib.h"
#include <fcntl.h>
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <limits.h>
#include <sys/types.h>
#include "stdiom.h"

long
ftell(FILE *iop)
{
	ptrdiff_t adjust;
	off64_t	tres;
#ifdef _REENTRANT
	rmutex_t *lk;
#endif _REENTRANT

	FLOCKFILE(lk, iop);
	if (iop->_cnt < 0)
		iop->_cnt = 0;
	if (iop->_flag & _IOREAD)
		adjust = (ptrdiff_t) -iop->_cnt;
	else if (iop->_flag & (_IOWRT | _IORW)) {
		adjust = 0;
		if (((iop->_flag & (_IOWRT | _IONBF)) == _IOWRT) &&
						(iop->_base != 0))
			adjust = iop->_ptr - iop->_base;
		else if ((iop->_flag & _IORW) && (iop->_base != 0))
			adjust = (ptrdiff_t) -iop->_cnt;
	} else {
		errno = EBADF;	/* file descriptor refers to no open file */
		FUNLOCKFILE(lk);
		return (EOF);
	}

	tres = lseek64(FILENO(iop), 0, SEEK_CUR);
	if (tres >= 0)
		tres += adjust;

	if (tres > LONG_MAX) {
		errno = EOVERFLOW;
		FUNLOCKFILE(lk);
		return (EOF);
	}

	FUNLOCKFILE(lk);
	return ((long) tres);
}

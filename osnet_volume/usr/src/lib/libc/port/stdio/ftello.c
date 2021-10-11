/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#pragma ident	"@(#)ftello.c	1.8	97/12/02 SMI"	/* SVr4.0 1.13	*/

/*LINTLIBRARY*/

/*
 * Return file offset.
 * Coordinates with buffering.
 */

#include <sys/feature_tests.h>

#if !defined(_LP64)
#pragma weak ftello64 = _ftello64
#endif
#pragma weak ftello = _ftello

#include "synonyms.h"
#include "file64.h"
#include "mtlib.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <stddef.h>
#include "stdiom.h"

#if !defined(_LP64)

off64_t
ftello64(FILE *iop)
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
	} else {
		errno = EBADF;	/* file descriptor refers to no open file */
		FUNLOCKFILE(lk);
		return ((off64_t) EOF);
	}
	tres = lseek64(FILENO(iop), 0, SEEK_CUR);
	if (tres >= 0)
		tres += (off64_t)adjust;
	FUNLOCKFILE(lk);
	return (tres);
}

#endif	/* _LP64 */

off_t
ftello(FILE *iop)
{
	return ((off_t)ftell(iop));
}

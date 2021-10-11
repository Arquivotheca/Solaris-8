/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)fputs.c	1.18	99/11/03 SMI"	/* SVr4.0 3.19 */

/*LINTLIBRARY*/
/*
 * Ptr args aren't checked for NULL because the program would be a
 * catastrophic mess anyway.  Better to abort than just to return NULL.
 */
#include "synonyms.h"
#include "shlib.h"
#include "file64.h"
#include "mtlib.h"
#include <stdio.h>
#include <string.h>
#include <thread.h>
#include <synch.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include "stdiom.h"
#include "mse.h"

int
fputs(const char *ptr, FILE *iop)
{
	ssize_t ndone = 0L, n;
	unsigned char *cptr, *bufend;
	char *p;
	rmutex_t *lk;

	FLOCKFILE(lk, iop);

	_SET_ORIENTATION_BYTE(iop);

	if (_WRTCHK(iop)) {
		FUNLOCKFILE(lk);
		return (EOF);
	}
	bufend = _bufend(iop);

	if ((iop->_flag & _IONBF) == 0) {
		for (; ; ptr += n) {
			while ((n = bufend - (cptr = iop->_ptr)) <= 0) {
				/* full buf */
				if (_xflsbuf(iop) == EOF) {
					FUNLOCKFILE(lk);
					return (EOF);
				}
			}
			if ((p = memccpy((char *) cptr, ptr, '\0',
			    (size_t)n)) != 0)
				n = (p - (char *) cptr) - 1;
			iop->_cnt -= n;
			iop->_ptr += n;
			if (_needsync(iop, bufend))
				_bufsync(iop, bufend);
			ndone += n;
			if (p != 0) {
				/* done; flush buffer if line-buffered */
				if (iop->_flag & _IOLBF)
					if (_xflsbuf(iop) == EOF) {
						FUNLOCKFILE(lk);
						return (EOF);
					}
				FUNLOCKFILE(lk);
				if (ndone <= INT_MAX)
					return ((int)ndone);
				else
					return (EOF);
			}
		}
	}
	else
	{
		/* write out to an unbuffered file */
		size_t cnt = strlen(ptr);
		ssize_t num_wrote;
		ssize_t count = (ssize_t)cnt;

		while ((num_wrote = write(iop->_file, ptr,
			(size_t)count)) != count) {
				if (num_wrote <= 0) {
					iop->_flag |= _IOERR;
					FUNLOCKFILE(lk);
					return (EOF);
				}
				count -= num_wrote;
				ptr += num_wrote;
		}
		FUNLOCKFILE(lk);
		if (cnt <= INT_MAX)
			return ((int)cnt);
		else
			return (EOF);
	}
}

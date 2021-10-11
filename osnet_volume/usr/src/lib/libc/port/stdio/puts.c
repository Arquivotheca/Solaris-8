/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)puts.c	1.15	99/11/03 SMI"	/* SVr4.0 3.13 */

/*LINTLIBRARY*/
#include "synonyms.h"
#include "shlib.h"
#include "file64.h"
#include "mtlib.h"
#include <sys/types.h>
#include <stdio.h>
#include <memory.h>
#include <thread.h>
#include <synch.h>
#include <limits.h>
#include "stdiom.h"
#include "mse.h"

int
puts(const char *ptr)
{
	char *p;
	ssize_t ndone = 0L, n;
	unsigned char *cptr, *bufend;
	rmutex_t *lk;

	FLOCKFILE(lk, stdout);

	_SET_ORIENTATION_BYTE(stdout);

	if (_WRTCHK(stdout)) {
		FUNLOCKFILE(lk);
		return (EOF);
	}

	bufend = _bufend(stdout);

	for (; ; ptr += n) {
		while ((n = bufend - (cptr = stdout->_ptr)) <= 0) /* full buf */
		{
			if (_xflsbuf(stdout) == EOF) {
				FUNLOCKFILE(lk);
				return (EOF);
			}
		}
		if ((p = memccpy((char *) cptr, ptr, '\0', (size_t)n)) != 0)
			n = p - (char *) cptr;
		stdout->_cnt -= n;
		stdout->_ptr += n;
		if (_needsync(stdout, bufend))
			_bufsync(stdout, bufend);
		ndone += n;
		if (p != 0) {
			stdout->_ptr[-1] = '\n'; /* overwrite '\0' with '\n' */
			if (stdout->_flag & (_IONBF | _IOLBF)) /* flush line */
			{
				if (_xflsbuf(stdout) == EOF) {
					FUNLOCKFILE(lk);
					return (EOF);
				}
			}
			FUNLOCKFILE(lk);
			if (ndone <= INT_MAX)
				return ((int)ndone);
			else
				return (EOF);
		}
	}
}

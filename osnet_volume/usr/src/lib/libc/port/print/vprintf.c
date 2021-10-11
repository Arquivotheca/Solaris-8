/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vprintf.c	1.19	99/11/03 SMI"	/* SVr4.0 1.7.1.4 */

/*LINTLIBRARY*/

#include "synonyms.h"
#include <mtlib.h>
#include <stdarg.h>
#include <errno.h>
#include <thread.h>
#include <values.h>
#include <synch.h>
#include <sys/types.h>
#include "print.h"
#include "mse.h"

/*VARARGS1*/

int
vprintf(const char *format, va_list ap)
{
	ssize_t count;
	rmutex_t *lk;

	/* Use F*LOCKFILE() macros because vprintf() is not async-safe. */
	FLOCKFILE(lk, stdout);

	_SET_ORIENTATION_BYTE(stdout);

	if (!(stdout->_flag & _IOWRT)) {
		/* if no write flag */
		if (stdout->_flag & _IORW) {
			/* if ok, cause read-write */
			stdout->_flag |= _IOWRT;
		} else {
			/* else error */
			FUNLOCKFILE(lk);
			errno = EBADF;
			return (EOF);
		}
	}
	count = _doprnt(format, ap, stdout);

	/* check for error or EOF */
	if (FERROR(stdout) || count == EOF) {
		FUNLOCKFILE(lk);
		return (EOF);
	}

	FUNLOCKFILE(lk);

	/* check for overflow */
	if ((size_t)count > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	} else
		return ((int)count);
}

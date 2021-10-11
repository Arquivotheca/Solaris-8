/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fprintf.c	1.24	99/11/03 SMI"	/* SVr4.0 1.14	*/

/*LINTLIBRARY*/

/* This function should not be defined weak, but there might be */
/* some program or libraries that may be interposing on this */
#pragma weak fprintf = _fprintf

#include "synonyms.h"
#include "shlib.h"
#include <mtlib.h>
#include <thread.h>
#include <synch.h>
#include <stdarg.h>
#include <values.h>
#include <errno.h>
#include "print.h"
#include <sys/types.h>
#include "mse.h"

/*VARARGS2*/
int
fprintf(FILE *iop, const char *format, ...)
{
	ssize_t count;
	rmutex_t *lk;
	va_list ap;

	va_start(ap, format);

	/* Use F*LOCKFILE() macros because fprintf() is not async-safe. */
	FLOCKFILE(lk, iop);

	_SET_ORIENTATION_BYTE(iop);

	if (!(iop->_flag & _IOWRT)) {
		/* if no write flag */
		if (iop->_flag & _IORW) {
			/* if ok, cause read-write */
			iop->_flag |= _IOWRT;
		} else {
			/* else error */
			FUNLOCKFILE(lk);
			errno = EBADF;
			return (EOF);
		}
	}
	count = _doprnt(format, ap, iop);

	/* return EOF on error or EOF */
	if (FERROR(iop) || count == EOF) {
		FUNLOCKFILE(lk);
		return (EOF);
	}

	FUNLOCKFILE(lk);

	/* error on overflow */
	if ((size_t)count > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	}
	return ((int)count);
}

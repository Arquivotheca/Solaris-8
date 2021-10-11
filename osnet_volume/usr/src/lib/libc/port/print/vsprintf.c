/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vsprintf.c	1.14	98/02/24 SMI"	/* SVr4.0 1.6.1.4 */

/*LINTLIBRARY*/
#include "synonyms.h"
#include <mtlib.h>
#include <stdarg.h>
#include <values.h>
#include <errno.h>
#include <synch.h>
#include <thread.h>
#include <sys/types.h>
#include "print.h"

/*VARARGS2*/
int
vsprintf(char *string, const char *format, va_list ap)
{
	ssize_t count;
	FILE siop;

	siop._cnt = MAXINT;
	siop._base = siop._ptr = (unsigned char *)string;
	siop._flag = _IOREAD; /* distinguish dummy file descriptor */
	count = _doprnt(format, ap, &siop);
	*siop._ptr = '\0'; /* plant terminating null character */

	if (count == EOF) {
		return (EOF);
	}
	/* check for overflow */
	if ((size_t) count > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	} else
		return ((int) count);
}

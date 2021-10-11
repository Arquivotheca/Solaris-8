/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)snprintf.c	1.8	99/12/06 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include "shlib.h"
#include <mtlib.h>
#include <stdarg.h>
#include <values.h>
#include <thread.h>
#include <synch.h>
#include <errno.h>
#include <sys/types.h>
#include "print.h"


/*VARARGS2*/
int
snprintf(char *string, size_t n, const char *format, ...)
{
	ssize_t count;
	FILE siop;
	va_list ap;

	if (n == 0)
		return (EOF);

	if (n > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	}

	siop._cnt = n - 1;
	siop._base = siop._ptr = (unsigned char *)string;
	siop._flag = _IOREAD; /* distinguish dummy file descriptor */

#ifdef _LP64
	/*
	 * _bufend() (_realbufend()) should return NULL for v/snprintf,
	 * so PUT() macro in _doprnt() will bounds check.  For 32-bit,
	 * there is no _end field, and _realbufend() will return NULL
	 * since it cannot find the dummy FILE structure in the linked
	 * list of FILE strucutres.  See bug 4274368.
	 */
	siop._end = NULL;
#endif
	va_start(ap, format);
	count = _doprnt(format, ap, &siop);
	va_end(ap);
	*siop._ptr = '\0'; /* plant terminating null character */

	if (count == EOF)
		return (EOF);

	/* check for overflow */
	if ((size_t)count > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	} else
		return ((int)count);
}

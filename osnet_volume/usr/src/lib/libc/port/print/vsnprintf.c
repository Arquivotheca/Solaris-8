/*
 * Copyright (c) 1996-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)vsnprintf.c	1.7	99/12/06 SMI"

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
vsnprintf(char *string, size_t n, const char *format, va_list ap)
{
	ssize_t count;
	FILE siop;

	if (n == 0)
		return (EOF);

	if (n > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	}

	siop._cnt = n - 1;
	siop._base = siop._ptr = (unsigned char *)string;
	siop._flag = _IOREAD;	/* distinguish dummy file descriptor */

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
	count = _doprnt(format, ap, &siop);
	*siop._ptr = '\0';	/* plant terminating null character */
	/* overflow check */
	if ((size_t) count > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	} else
		return ((int) count);
}

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * vsprintf.c -- vsprintf library routine
 */

#ident	"<@(#)vsprintf.c	1.5	97/03/06 SMI>"

#include <stdarg.h>
#include <stdio.h>

#include "eprintf.h"

/*
 * our_write -- "write" function passed to eprintf to fill in caller's buffer
 */

static maxlen;

static int
our_write(void *arg, char *ptr, int len)
{
	int retval = len;
	char **bufpp = (char **)arg;

	/* just copy the bytes into the buffer passed to _fsprintf... */
	while ((maxlen > 1) && len--) {
		*(*bufpp)++ = *ptr++;
		maxlen -= 1;
	}

	return (retval);	/* no detectable failures in this routine */
}

/*
 * vsnprintf -- printf to buffer with size limits
 */

int
vsnprintf(char *buffer, int len, const char *fmt, va_list ap)
{
	int retval;
	maxlen = len - 1;
	retval = eprintf(our_write, (void *)&buffer, fmt, ap);
	*buffer = '\0';
	return (retval);
}

/*
 * vsprintf -- printf to a buffer with varargs
 */

int
vsprintf(char *buffer, const char *fmt, va_list ap)
{
	return (vsnprintf(buffer, ~0U >> 1, fmt, ap));
}

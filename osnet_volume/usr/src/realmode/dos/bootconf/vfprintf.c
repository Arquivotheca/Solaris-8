/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * vfprintf.c -- vfprintf library routine
 */

#ident	"<@(#)vfprintf.c	1.3	97/03/06 SMI>"

#include <stdarg.h>
#include <stdio.h>
#include "eprintf.h"

/*
 * our_write -- "write" function passed to eprintf to write to caller's fp
 */

static int
our_write(void *arg, char *ptr, int len)
{
	int outcount = 0;
	FILE *fp = (FILE *)arg;

	/* just fputc the bytes to the fp passed to vfprintf... */
	while (len--) {
		if (putc(*ptr, fp) == EOF)
			break;
		outcount++;
		ptr++;	/* ++ here so putc macro doesn't do multiple ++s */
	}

	return (outcount);
}

/*
 * vprintf -- printf with varargs
 */

int
vfprintf(FILE *fp, const char *fmt, va_list ap)
{
	return (eprintf(our_write, (void *)fp, fmt, ap));
}

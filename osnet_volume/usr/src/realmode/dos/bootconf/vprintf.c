/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * vprintf.c -- vprintf library routine
 */

#ident	"<@(#)vprintf.c	1.2	95/11/07 SMI>"

#include <stdarg.h>
#include <stdio.h>

/*
 * vprintf -- printf with varargs
 */

int
vprintf(const char *fmt, va_list ap)
{
	return (vfprintf(stdout, fmt, ap));
}

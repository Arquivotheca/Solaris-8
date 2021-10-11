/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * printf.c -- printf library routine
 */

#ident	"<@(#)printf.c	1.2	95/11/07 SMI>"

#include <stdarg.h>
#include <stdio.h>

/*
 * printf -- print to stdout
 */

int
printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	return (vfprintf(stdout, fmt, ap));
}

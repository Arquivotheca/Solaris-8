/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * sprintf.c -- sprintf library routine
 */

#ident	"<@(#)sprintf.c	1.2	95/11/07 SMI>"

#include <stdarg.h>
#include <stdio.h>

/*
 * sprintf -- printf to a buffer
 */

int
sprintf(char *buffer, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	return (vsprintf(buffer, fmt, ap));
}

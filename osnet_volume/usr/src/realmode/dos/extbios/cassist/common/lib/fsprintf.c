/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Minimal C library for Solars x86 realmode drivers, string printf:
 *
 *     Uses the driver callback to format a string.
 */

#ident	"<@(#)fsprintf.c	1.4	95/11/18	SMI>"
#include <dostypes.h>
#include <stdarg.h>
#include <stdio.h>

int
sprintf(char *sp, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	return (vfprintf((FILE *)sp, fmt, ap));
}

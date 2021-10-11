/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Minimal C library for Solars x86 realmode drivers, file printf:
 *
 *     Uses the driver callback to print on standard output -- regardless of
 *     where caller thinks (s)he's printing!
 */

#ident	"<@(#)fprintf.c	1.3	95/11/18	SMI>"
#include <dostypes.h>
#include <stdarg.h>
#include <stdio.h>

int
fprintf(FILE *fp, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	return (vfprintf(0, fmt, ap));
}

/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * fprintf.c -- fprintf library routine
 */

#ident	"<@(#)fprintf.c	1.2	95/11/07 SMI>"

#include <stdarg.h>
#include <stdio.h>

/*
 * fprintf -- printf to a FILE *fp
 */

int
fprintf(FILE *fp, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	return (vfprintf(fp, fmt, ap));
}

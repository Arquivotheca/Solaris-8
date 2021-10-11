/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */
 
#ident "@(#)fsprintf.c	1.5	97/03/17 SMI"
 
/*
 *  Minimal C library for Solaris x86 real mode drivers, string printf:
 *
 *	Uses the driver callback to format a string.
 */

#include <stdarg.h>
#include <dostypes.h>
#include <stdio.h>

int
sprintf(char *sp, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	return (vfprintf((FILE *)sp, fmt, ap));
}

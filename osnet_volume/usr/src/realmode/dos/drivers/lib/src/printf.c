/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */
 
#ident "@(#)printf.c	1.5	97/03/17 SMI"
 
/*
 *  Minimal C library for Solaris x86 real mode drivers, printf:
 *
 *	Uses the driver callback to print on standard output.
 */

#include <stdarg.h>
#include <dostypes.h>
#include <stdio.h>

int
printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	return (vfprintf(0, fmt, ap));
}

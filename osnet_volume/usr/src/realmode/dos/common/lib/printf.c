/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *    Realmode printf support:
 *
 *    NOTE: It's easier to read this with tabs stops set at 4!
 */

#ident "@(#)printf.c	1.2	95/03/20 SMI\n"
#include <dostypes.h>
#include <stdarg.h>
#include <stdio.h>

int 
printf (const char *fmt, ...)
{	/*
	 *  Print to standard output:
	 *
	 *  This is the most commonly used printf variant, fixed arg list
	 *  with output directed to stdout.
	 */

	va_list ap;
	va_start(ap, fmt);
	return(vfprintf(stdout, fmt, ap));
}

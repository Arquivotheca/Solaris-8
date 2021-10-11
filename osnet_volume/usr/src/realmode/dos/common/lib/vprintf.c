/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *    Realmode printf support:
 *
 *    NOTE: It's easier to read this with tabs stops set at 4!
 */

#ident "@(#)vprintf.c	1.2	95/03/20 SMI\n"
#include <dostypes.h>
#include <stdarg.h>
#include <stdio.h>

int 
vprintf (const char *fmt, va_list ap) 
{	/*
	 *  Print to standard output:
	 *
	 *  Standard printf with variable argument list.
	 */

	return(vfprintf(stdout, fmt, ap));
}

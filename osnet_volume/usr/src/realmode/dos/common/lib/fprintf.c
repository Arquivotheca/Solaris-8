/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *    Realmode printf support:
 *
 *    NOTE: It's easier to read this with tabs stops set at 4!
 */

#ident "@(#)fprintf.c	1.2	95/03/20 SMI\n"
#include <dostypes.h>
#include <stdarg.h>
#include <stdio.h>

int 
fprintf (FILE *fp, const char *fmt, ...)
{	/*
	 *  Print to specified file:
	 *
	 *  Very similar to printf(), except that caller passes pointer to
	 *  FILE where output is to appear.  
	 */

	va_list ap;
	va_start(ap, fmt);
	return(vfprintf(fp, fmt, ap));
}

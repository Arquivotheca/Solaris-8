/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *    Realmode printf support:
 *
 *    NOTE: It's easier to read this with tabs stops set at 4!
 */

#ident "@(#)sprintf.c	1.2	95/03/20 SMI\n"
#include <dostypes.h>
#include <stdarg.h>
#include <stdio.h>

extern char far *_PFstring_;

int 
sprintf (char *buffer, const char *fmt, ...)
{	/*
	 *  Print to to a string:
	 *
	 *  Like printf(), except that the output is placed directly in the
	 *  indicated "buffer" rather than to a file.
	 */

	va_list ap;
	va_start(ap, fmt);
	_PFstring_ = buffer;
	return(vfprintf(0, fmt, ap));
}

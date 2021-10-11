/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *    Realmode printf support:
 *
 *    NOTE: It's easier to read this with tabs stops set at 4!
 */

#ident "@(#)vsprintf.c	1.2	95/03/20 SMI\n"
#include <dostypes.h>
#include <stdarg.h>
#include <stdio.h>

extern char far *_PFstring_;

int 
vsprintf (char *buffer, const char *fmt, va_list ap)
{	/*
	 *  Print to to a string:
	 *
	 *  Like vprintf(), except that the output is placed directly in the
	 *  indicated "buffer" rather than to a file.
	 */

	_PFstring_ = buffer;
	return(vfprintf(0, fmt, ap));
}

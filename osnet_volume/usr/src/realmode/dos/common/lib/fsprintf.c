/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *    Realmode printf support:
 *
 *    NOTE: It's easier to read this with tabs stops set at 4!
 */

#ident "@(#)fsprintf.c	1.2	95/03/20 SMI\n"
#include <dostypes.h>
#include <stdarg.h>
#include <stdio.h>

extern char far *_PFstring_;

int 
_fsprintf (char far *buffer, const char *fmt, ...)
{	/*
	 *  Print to to a string:
	 *
	 *  Like sprintf(), except that we use a far pointer to locate the
	 *  buffer to receive the formatted text!
	 */

	va_list ap;
	va_start(ap, fmt);
	_PFstring_ = buffer;
	return(vfprintf(0, fmt, ap));
}

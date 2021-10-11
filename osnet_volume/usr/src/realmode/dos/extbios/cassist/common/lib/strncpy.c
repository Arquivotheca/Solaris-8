/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Minimal C library for Solars x86 realmode drivers, string copy:
 *
 *     Much like the ANSI strncpy, except pointers are far and we don't return
 *     a value (to prevent caller from misusing it).
 */

#ident	"<@(#)strncpy.c	1.2	95/08/14	SMI>"
#include <dostypes.h>
#include <string.h>

void
strncpy(char far *p, const char far *q, unsigned n)
{
	/*
	 *  Note the absence of bogus "optimization"s!
	 */

	while (n-- && (*p++ = *q++));
}

/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Minimal C library for Solars x86 realmode drivers, string copy:
 *
 *     Much like the ANSI strcpy, except we don't return a value (to prevent
 *     caller from misusing it).
 */

#ident	"<@(#)strcpy.c	1.4	95/08/14	SMI>"
#include <dostypes.h>
#include <string.h>

void
strcpy(char far *p, const char far *q)
{
	/*
	 *  Note the absence of bogus "optimization"s!
	 */

	while (*p++ = *q++);
}

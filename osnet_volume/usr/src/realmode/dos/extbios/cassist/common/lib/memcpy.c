/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Minimal C library for Solars x86 realmode drivers, block copy:
 *
 *     Much like the ANSI memcpy except that we don't return a value (to
 *     prevent caller from misusing it!).
 */

#ident	"<@(#)memcpy.c	1.4	95/08/14	SMI>"
#include <string.h>

void
memcpy(void far *p, const void far *q, unsigned len)
{
	/*
	 *  Note the absence of bogus "optimization"s!
	 */

	char far *cp = (char far *)p;
	const char far *cq = (const char far *)q;

	while (len-- > 0) *cp++ = *cq++;
}

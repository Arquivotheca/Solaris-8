/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Minimal C library for Solars x86 realmode drivers, block initialize:
 *
 *     Much like the ANSI memset except that we don't return a value (to
 *     prevent caller from misusing it!).
 */

#ident	"<@(#)memset.c	1.2	95/08/14	SMI>"
#include <string.h>

void
memset(void far *p, int c, unsigned len)
{
	/*
	 *  Note the absence of bogus "optimization"s!
	 */

	char far *cp = (char far *)p;
	while (len-- > 0) *cp++ = c;
}

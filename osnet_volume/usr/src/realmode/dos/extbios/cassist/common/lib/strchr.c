/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Minimal C library for Solars x86 realmode drivers, string index:
 *
 *    Much like the ANSI strchr, except that input pointer and return value
 *    are both "far".
 */

#ident	"<@(#)strchr.c	1.2	95/08/14 SMI>"
#include <dostypes.h>
#include <string.h>

char far *
strchr(const char far *cp, int c)
{
	/*
	 *  Note the absence of bogus "optimization"s!
	 */

	while (*cp && (*cp != c)) cp++;
	return ((*cp == c) ? (char far *)cp : 0);
}

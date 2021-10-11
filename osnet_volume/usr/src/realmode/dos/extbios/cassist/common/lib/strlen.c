/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Minimal C library for Solars x86 realmode drivers, string length:
 *
 *    Much like the ANSI strcmp, except that input pointers are "far".
 */

#ident	"<@(#)strlen.c	1.3	95/08/14	SMI>"
#include <dostypes.h>
#include <string.h>

int
strlen(const char far *p)
{
	/*
	 *  Note the absence of bogus "optimization"s!
	 */

	int j = 0;

	while (*p++) j++;
	return (j);
}

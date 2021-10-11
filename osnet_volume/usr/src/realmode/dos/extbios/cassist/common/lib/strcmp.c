/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Minimal C library for Solars x86 realmode drivers, string compare:
 *
 *    Much like the ANSI strcmp, except that input pointers are "far".
 */

#ident	"<@(#)strcmp.c	1.4	95/08/14	SMI>"
#include <dostypes.h>
#include <string.h>

int
strcmp(const char far *p, const char far *q)
{
	/*
	 *  Note the absence of bogus "optimization"s!
	 */

	int j;

	while (!(j = (*p - *q)) && *p++) q++;
	return (j);
}

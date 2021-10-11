/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Minimal C library for Solars x86 realmode drivers, block compare
 *
 *    Just like ANSI memcmp, except that input pointers are "far".
 */

#ident	"<@(#)memcmp.c	1.4	95/08/14	SMI>"
#include <string.h>

int
memcmp(const void far *p, const void far *q, unsigned len)
{
	/*
	 *  Note the absence of bogus "optimization"s!
	 */

	int j = 0;
	const unsigned char far *cp = (const unsigned char far *)p;
	const unsigned char far *cq = (const unsigned char far *)q;

	while ((len-- > 0) && !(j = (*cp++ - *cq++)));
	return (j);
}

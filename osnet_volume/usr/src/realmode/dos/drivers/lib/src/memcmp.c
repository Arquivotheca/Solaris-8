/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */
 
#ident "@(#)memcmp.c	1.2	97/03/10 SMI"
 
/*
 *  Minimal C library for Solaris x86 real mode drivers, block compare
 *
 *    Just like ANSI memcmp, except that input pointers are "far".
 */

#include <string.h>

int
memcmp(const void far *p, const void far *q, unsigned len)
{
	/*
	 *  Use a simple algorithm to minimize code size and reduce
	 *  chance of error.
	 */

	int j = 0;
	const unsigned char far *cp = (const unsigned char far *)p;
	const unsigned char far *cq = (const unsigned char far *)q;

	while ((len-- > 0) && !(j = (*cp++ - *cq++)));
	return (j);
}

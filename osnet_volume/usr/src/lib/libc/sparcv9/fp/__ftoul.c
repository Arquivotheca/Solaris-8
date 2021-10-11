/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)__ftoul.c	1.1	97/08/26 SMI"

/*
 * __ftoul(x) converts float x to unsigned long.
 */
unsigned long
__ftoul(float x)
{
	union {
		float		f;
		unsigned int	l;
	} u;

	u.f = x;

	/* handle cases for which float->unsigned long differs from */
	/* float->signed long */
	if ((u.l >> 23) == 0xbe) {
		/* 2^63 <= x < 2^64 */
		return (0x8000000000000000ul | ((long)u.l << 40));
	}

	/* for all other cases, just convert to signed long */
	return ((long)x);
}

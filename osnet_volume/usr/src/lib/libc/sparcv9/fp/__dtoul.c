/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)__dtoul.c	1.1	97/08/26 SMI"

/*
 * __dtoul(x) converts double x to unsigned long.
 */
unsigned long
__dtoul(double x)
{
	union {
		double		d;
		unsigned long	l;
	} u;

	u.d = x;

	/* handle cases for which double->unsigned long differs from */
	/* double->signed long */
	if ((u.l >> 52) == 0x43e) {
		/* 2^63 <= x < 2^64 */
		return (0x8000000000000000ul | (u.l << 11));
	}

	/* for all other cases, just convert to signed long */
	return ((long)x);
}

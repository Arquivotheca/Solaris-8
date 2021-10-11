/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_Q_ulltoq.c	1.5	97/08/26 SMI"

#include "quad.h"

/*
 * _Q_ulltoq(x) returns (long double)x.
 */
union longdouble
_Q_ulltoq(unsigned long long x)
{
	union longdouble	z;
	unsigned int		e;

	/* test for zero */
	if (x == 0) {
		Z.l.msw = Z.l.frac2 = Z.l.frac3 = Z.l.frac4 = 0;
		QUAD_RETURN(Z);
	}

	/* find the most significant bit */
	for (e = 63; (x & (1ll << e)) == 0; e--)
		;

	if (e > 48) {
		Z.l.msw = (x >> (e - 16)) & 0xffff;
		Z.l.frac2 = x >> (e - 48);
		Z.l.frac3 = x << (80 - e);
	} else if (e > 16) {
		Z.l.msw = (x >> (e - 16)) & 0xffff;
		Z.l.frac2 = x << (48 - e);
		Z.l.frac3 = 0;
	} else {
		Z.l.msw = (x << (16 - e)) & 0xffff;
		Z.l.frac2 = Z.l.frac3 = 0;
	}
	Z.l.frac4 = 0;
	Z.l.msw |= ((e + 0x3fff) << 16);
	QUAD_RETURN(Z);
}

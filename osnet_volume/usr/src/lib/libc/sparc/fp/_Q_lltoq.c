/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_Q_lltoq.c	1.4	97/08/26 SMI"

#include "quad.h"

/*
 * _Q_lltoq(x) returns (long double)x.
 */
union longdouble
_Q_lltoq(long long x)
{
	union longdouble	z;
	unsigned int		s, e;

	/* extract the sign */
	s = 0;
	if (x < 0) {
		if ((unsigned long long) x == 0x8000000000000000ull) {
			/* largest negative 64 bit int */
			Z.l.msw = 0xc03e0000;
			Z.l.frac2 = Z.l.frac3 = Z.l.frac4 = 0;
			QUAD_RETURN(Z);
		}
		x = -x;
		s = 0x80000000;
	} else if (x == 0) {
		Z.l.msw = Z.l.frac2 = Z.l.frac3 = Z.l.frac4 = 0;
		QUAD_RETURN(Z);
	}

	/* find the most significant bit */
	for (e = 62; (x & (1ll << e)) == 0; e--)
		;

	if (e > 48) {
		Z.l.msw = ((unsigned long long) x >> (e - 16)) & 0xffff;
		Z.l.frac2 = (unsigned long long) x >> (e - 48);
		Z.l.frac3 = (unsigned long long) x << (80 - e);
	} else if (e > 16) {
		Z.l.msw = ((unsigned long long) x >> (e - 16)) & 0xffff;
		Z.l.frac2 = (unsigned long long) x << (48 - e);
		Z.l.frac3 = 0;
	} else {
		Z.l.msw = ((unsigned long long) x << (16 - e)) & 0xffff;
		Z.l.frac2 = Z.l.frac3 = 0;
	}
	Z.l.frac4 = 0;
	Z.l.msw |= s | ((e + 0x3fff) << 16);
	QUAD_RETURN(Z);
}

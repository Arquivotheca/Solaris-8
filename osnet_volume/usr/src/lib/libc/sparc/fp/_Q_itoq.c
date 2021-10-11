/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_Q_itoq.c	1.6	97/08/26 SMI"

#include "quad.h"

#ifdef __sparcv9

/*
 * _Qp_itoq(pz, x) sets *pz = (long double)x.
 */
void
_Qp_itoq(union longdouble *pz, int x)

#else

/*
 * _Q_itoq(x) returns (long double)x.
 */
union longdouble
_Q_itoq(int x)

#endif /* __sparcv9 */

{
#ifndef __sparcv9
	union longdouble	z;
#endif
	unsigned int		s, e;

	/* extract the sign */
	s = 0;
	if (x < 0) {
		if ((unsigned) x == 0x80000000) {
			/* largest negative int */
			Z.l.msw = 0xc01e0000;
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
	for (e = 30; (x & (1 << e)) == 0; e--)
		;

	if (e > 16) {
		Z.l.msw = ((unsigned) x >> (e - 16)) & 0xffff;
		Z.l.frac2 = (unsigned) x << (48 - e);
	} else {
		Z.l.msw = ((unsigned) x << (16 - e)) & 0xffff;
		Z.l.frac2 = 0;
	}
	Z.l.frac3 = Z.l.frac4 = 0;
	Z.l.msw |= s | ((e + 0x3fff) << 16);
	QUAD_RETURN(Z);
}

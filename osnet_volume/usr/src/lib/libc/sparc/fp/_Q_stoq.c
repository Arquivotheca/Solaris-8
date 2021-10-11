/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_Q_stoq.c	1.6	97/08/26 SMI"

#include "quad.h"

#ifdef __sparcv9

/*
 * _Qp_stoq(pz, x) sets *pz = (long double)x.
 */
void
_Qp_stoq(union longdouble *pz, float x)

#else

/*
 * _Qp_stoq(x) returns (long double)x.
 */
union longdouble
_Q_stoq(float x)

#endif /* __sparcv9 */

{
#ifndef __sparcv9
	union longdouble	z;
#endif
	union {
		float		f;
		unsigned int	l;
	} u;
	unsigned int		m, fsr;

	/* extract the exponent */
	u.f = x;
	m = ((u.l & 0x7f800000) >> 7) + 0x3f800000;
	if (m == 0x3f800000) {
		/* x is zero or denormal */
		if (u.l & 0x7fffff) {
			/* x is denormal, normalize it */
			m = 0x3f810000;
			do {
				u.f += u.f;
				m -= 0x10000;
			} while ((u.l & 0x7f800000) == 0);
		} else {
			m = 0;
		}
	} else if (m == 0x407f0000) {
		/* x is inf or nan */
		m = 0x7fff0000;
		if ((u.l & 0x3fffff) && (u.l & 0x400000) == 0) {
			/* snan, signal invalid */
			__quad_getfsrp(&fsr);
			if (fsr & FSR_NVM) {
				__quad_fstoq(&x, &Z);
				QUAD_RETURN(Z);
			} else {
				fsr = (fsr & ~FSR_CEXC) | FSR_NVA | FSR_NVC;
				__quad_setfsrp(&fsr);
			}
			u.l |= 0x400000;
		}
	}
	Z.l.msw = m | (u.l & 0x80000000) | ((u.l & 0x7fff80) >> 7);
	Z.l.frac2 = u.l << 25;
	Z.l.frac3 = Z.l.frac4 = 0;
	QUAD_RETURN(Z);
}

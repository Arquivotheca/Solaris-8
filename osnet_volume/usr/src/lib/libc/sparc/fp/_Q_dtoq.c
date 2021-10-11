/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_Q_dtoq.c	1.6	97/08/26 SMI"

#include "quad.h"

#ifdef __sparcv9

/*
 * _Qp_dtoq(pz, x) sets *pz = (long double)x.
 */
void
_Qp_dtoq(union longdouble *pz, double x)

#else

/*
 * _Q_dtoq(x) returns (long double)x.
 */
union longdouble
_Q_dtoq(double x)

#endif /* __sparcv9 */

{
#ifndef __sparcv9
	union longdouble	z;
#endif
	union xdouble		u;
	unsigned int		m, fsr;

	/* extract the exponent */
	u.d = x;
	m = ((u.l.hi & 0x7ff00000) >> 4) + 0x3c000000;
	if (m == 0x3c000000) {
		/* x is zero or denormal */
		if ((u.l.hi & 0xfffff) | u.l.lo) {
			/* x is denormal, normalize it */
			m = 0x3c010000;
			do {
				u.d += u.d;
				m -= 0x10000;
			} while ((u.l.hi & 0x7ff00000) == 0);
		} else {
			m = 0;
		}
	} else if (m == 0x43ff0000) {
		/* x is inf or nan */
		m = 0x7fff0000;
		if (((u.l.hi & 0x7ffff) | u.l.lo) && (u.l.hi & 0x80000) == 0) {
			/* snan, signal invalid */
			__quad_getfsrp(&fsr);
			if (fsr & FSR_NVM) {
				__quad_fdtoq(&x, &Z);
				QUAD_RETURN(Z);
			} else {
				fsr = (fsr & ~FSR_CEXC) | FSR_NVA | FSR_NVC;
				__quad_setfsrp(&fsr);
			}
			u.l.hi |= 0x80000;
		}
	}
	Z.l.msw = m | (u.l.hi & 0x80000000) | ((u.l.hi & 0xffff0) >> 4);
	Z.l.frac2 = (u.l.hi << 28) | (u.l.lo >> 4);
	Z.l.frac3 = u.l.lo << 28;
	Z.l.frac4 = 0;
	QUAD_RETURN(Z);
}

/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_Q_qtoi.c	1.6	97/08/26 SMI"

#include "quad.h"

#ifdef __sparcv9
#define	_Q_qtoi	_Qp_qtoi
#endif

/*
 * _Q_qtoi(x) returns (int)*x.
 */
int
_Q_qtoi(const union longdouble *x)
{
	unsigned int	xm, fsr;
	int		i, round;

	xm = x->l.msw & 0x7fffffff;

	__quad_getfsrp(&fsr);

	/* handle nan, inf, and out-of-range cases */
	if (xm >= 0x401e0000) {
		if (x->l.msw == 0xc01e0000 && (x->l.frac2 & 0xfffe0000) == 0) {
			/* return largest negative int */
			i = 0x80000000;
			if ((x->l.frac2 & 0x1ffff) | x->l.frac3 | x->l.frac4) {
				/* signal inexact */
				if (fsr & FSR_NXM) {
					__quad_fqtoi(x, &i);
				} else {
					fsr = (fsr & ~FSR_CEXC) | FSR_NXA |
					    FSR_NXC;
					__quad_setfsrp(&fsr);
				}
			}
			return (i);
		}
		i = ((x->l.msw & 0x80000000)? 0x80000000 : 0x7fffffff);
		if (fsr & FSR_NVM) {
			__quad_fqtoi(x, &i);
		} else {
			fsr = (fsr & ~FSR_CEXC) | FSR_NVA | FSR_NVC;
			__quad_setfsrp(&fsr);
		}
		return (i);
	}
	if (xm < 0x3fff0000) {
		i = 0;
		if (xm | x->l.frac2 | x->l.frac3 | x->l.frac4) {
			/* signal inexact */
			if (fsr & FSR_NXM) {
				__quad_fqtoi(x, &i);
			} else {
				fsr = (fsr & ~FSR_CEXC) | FSR_NXA | FSR_NXC;
				__quad_setfsrp(&fsr);
			}
		}
		return (i);
	}

	/* now x is in the range of int */
	i = (int) (0x40000000 | ((xm & 0xffff) << 14) | (x->l.frac2 >> 18));
	round = i & ((1 << (0x401d - (xm >> 16))) - 1);
	i >>= (0x401d - (xm >> 16));
	if (x->l.msw & 0x80000000)
		i = -i;
	if (round | (x->l.frac2 & 0x3ffff) | x->l.frac3 | x->l.frac4) {
		/* signal inexact */
		if (fsr & FSR_NXM) {
			__quad_fqtoi(x, &i);
		} else {
			fsr = (fsr & ~FSR_CEXC) | FSR_NXA | FSR_NXC;
			__quad_setfsrp(&fsr);
		}
	}
	return (i);
}

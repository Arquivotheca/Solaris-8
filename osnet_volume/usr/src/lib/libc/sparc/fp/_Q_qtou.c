/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_Q_qtou.c	1.7	99/09/03 SMI"

#include "quad.h"

#ifdef __sparcv9
#define	_Q_qtou	_Qp_qtoui
#endif

/*
 * _Q_qtou(x) returns (unsigned int)*x.
 */
unsigned
_Q_qtou(const union longdouble *x)
{
	union longdouble	z;
	unsigned int		xm, fsr;
	int			i, round;

	xm = x->l.msw & 0x7fffffff;

	__quad_getfsrp(&fsr);

	/* handle nan, inf, and out-of-range cases */
	if (xm >= 0x401e0000) {
		if (x->l.msw < 0x401f0000) {
			i = 0x80000000 | ((xm & 0xffff) << 15) |
			    (x->l.frac2 >> 17);
			if ((x->l.frac2 & 0x1ffff) | x->l.frac3 | x->l.frac4) {
				/* signal inexact */
				if (fsr & FSR_NXM) {
					/* z = x - 2^31 */
					if (xm & 0xffff ||
					    x->l.frac2 & 0xffff0000) {
						z.l.msw = xm & 0xffff;
						z.l.frac2 = x->l.frac2;
						z.l.frac3 = x->l.frac3;
						z.l.frac4 = x->l.frac4;
					} else if (x->l.frac2 & 0xffff ||
					    x->l.frac3 & 0xffff0000) {
						z.l.msw = x->l.frac2;
						z.l.frac2 = x->l.frac3;
						z.l.frac3 = x->l.frac4;
						z.l.frac4 = 0;
					} else if (x->l.frac3 & 0xffff ||
					    x->l.frac4 & 0xffff0000) {
						z.l.msw = x->l.frac3;
						z.l.frac2 = x->l.frac4;
						z.l.frac3 = z.l.frac4 = 0;
					} else {
						z.l.msw = x->l.frac4;
						z.l.frac2 = z.l.frac3 =
						    z.l.frac4 = 0;
					}
					xm = 0x401e;
					while ((z.l.msw & 0x10000) == 0) {
						z.l.msw = (z.l.msw << 1) |
						    (z.l.frac2 >> 31);
						z.l.frac2 = (z.l.frac2 << 1) |
						    (z.l.frac3 >> 31);
						z.l.frac3 = (z.l.frac3 << 1) |
						    (z.l.frac4 >> 31);
						z.l.frac4 <<= 1;
						xm--;
					}
					z.l.msw |= (xm << 16);
					__quad_fqtoi(&z, &i);
					i |= 0x80000000;
				} else {
					fsr = (fsr & ~FSR_CEXC) | FSR_NXA |
					    FSR_NXC;
					__quad_setfsrp(&fsr);
				}
			}
			return (i);
		}
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
		if (xm | x->l.frac2 | x->l.frac3 | x->l.frac4) {
			/* signal inexact */
			i = 0;
			if (fsr & FSR_NXM) {
				__quad_fqtoi(x, &i);
			} else {
				fsr = (fsr & ~FSR_CEXC) | FSR_NXA | FSR_NXC;
				__quad_setfsrp(&fsr);
			}
			return (i);
		}
		return (0);
	}

	/* now x is in the range of int */
	i = 0x40000000 | ((xm & 0xffff) << 14) | (x->l.frac2 >> 18);
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

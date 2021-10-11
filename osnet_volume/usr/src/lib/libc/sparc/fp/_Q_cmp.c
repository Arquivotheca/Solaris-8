/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_Q_cmp.c	1.7	97/08/26 SMI"

#include "quad.h"

#ifdef __sparcv9
#define	_Q_cmp	_Qp_cmp
#endif

/*
 * _Q_cmp(x, y) returns the condition code that would result from
 * fcmpq *x, *y (and raises the same exceptions).
 */
enum fcc_type
_Q_cmp(const union longdouble *x, const union longdouble *y)
{
	unsigned int	xm, ym, fsr;

	if (QUAD_ISNAN(*x) || QUAD_ISNAN(*y)) {
		if ((QUAD_ISNAN(*x) && !(x->l.msw & 0x8000)) ||
		    (QUAD_ISNAN(*y) && !(y->l.msw & 0x8000))) {
			/* snan, signal invalid */
			__quad_getfsrp(&fsr);
			if (fsr & FSR_NVM) {
				__quad_fcmpq(x, y, &fsr);
				return ((fsr >> 10) & 3);
			} else {
				fsr = (fsr & ~FSR_CEXC) | FSR_NVA | FSR_NVC;
				__quad_setfsrp(&fsr);
			}
		}
		return (fcc_unordered);
	}

	/* ignore sign of zero */
	xm = x->l.msw;
	if (QUAD_ISZERO(*x))
		xm &= 0x7fffffff;
	ym = y->l.msw;
	if (QUAD_ISZERO(*y))
		ym &= 0x7fffffff;

	if ((xm ^ ym) & 0x80000000)	/* x and y have opposite signs */
		return ((ym & 0x80000000)? fcc_greater : fcc_less);

	if (xm & 0x80000000) {
		if (xm > ym)
			return (fcc_less);
		if (xm < ym)
			return (fcc_greater);
		if (x->l.frac2 > y->l.frac2)
			return (fcc_less);
		if (x->l.frac2 < y->l.frac2)
			return (fcc_greater);
		if (x->l.frac3 > y->l.frac3)
			return (fcc_less);
		if (x->l.frac3 < y->l.frac3)
			return (fcc_greater);
		if (x->l.frac4 > y->l.frac4)
			return (fcc_less);
		if (x->l.frac4 < y->l.frac4)
			return (fcc_greater);
		return (fcc_equal);
	}
	if (xm < ym)
		return (fcc_less);
	if (xm > ym)
		return (fcc_greater);
	if (x->l.frac2 < y->l.frac2)
		return (fcc_less);
	if (x->l.frac2 > y->l.frac2)
		return (fcc_greater);
	if (x->l.frac3 < y->l.frac3)
		return (fcc_less);
	if (x->l.frac3 > y->l.frac3)
		return (fcc_greater);
	if (x->l.frac4 < y->l.frac4)
		return (fcc_less);
	if (x->l.frac4 > y->l.frac4)
		return (fcc_greater);
	return (fcc_equal);
}

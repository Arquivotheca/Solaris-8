/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_Q_neg.c	1.6	97/08/26 SMI"

#include "quad.h"

#ifdef __sparcv9

/*
 * _Qp_neg(pz, x) sets *pz = -*x.
 */
void
_Qp_neg(union longdouble *pz, const union longdouble *x)

#else

/*
 * _Q_neg(x) returns -*x.
 */
union longdouble
_Q_neg(const union longdouble *x)

#endif /* __sparcv9 */

{
#ifndef __sparcv9
	union	longdouble	z;
#endif

	Z.l.msw = x->l.msw ^ 0x80000000;
	Z.l.frac2 = x->l.frac2;
	Z.l.frac3 = x->l.frac3;
	Z.l.frac4 = x->l.frac4;
	QUAD_RETURN(Z);
}

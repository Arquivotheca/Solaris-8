/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)finite.c	1.6	96/12/06 SMI"	/* SVr4 1.6	*/

/*LINTLIBRARY*/

/*	IEEE recommended functions */

#pragma weak finite = _finite
#pragma weak fpclass = _fpclass
#pragma weak unordered = _unordered

#include "synonyms.h"
#include <values.h>
#include "fpparts.h"

#define	P754_NOFAULT 1		/* avoid generating extra code */
#include <ieeefp.h>

/*
 * FINITE(X)
 * finite(x) returns 1 if x > -inf and x < +inf and 0 otherwise
 * NaN returns 0
 */

int
finite(double x)
{
	return ((EXPONENT(x) != MAXEXP));
}

/*
 * UNORDERED(x,y)
 * unordered(x,y) returns 1 if x is unordered with y, otherwise
 * it returns 0; x is unordered with y if either x or y is NAN
 */

int
unordered(double x, double y)
{
	if ((EXPONENT(x) == MAXEXP) && (HIFRACTION(x) || LOFRACTION(x)))
		return (1);
	if ((EXPONENT(y) == MAXEXP) && (HIFRACTION(y) || LOFRACTION(y)))
		return (1);
	return (0);
}	

/*
 * FPCLASS(X)
 * fpclass(x) returns the floating point class x belongs to
 */

fpclass_t
fpclass(double x)
{
	int	sign, exp;

	exp = EXPONENT(x);
	sign = SIGNBIT(x);
	if (exp == 0) { /* de-normal or zero */
		if (HIFRACTION(x) || LOFRACTION(x)) /* de-normal */
			return (sign ? FP_NDENORM : FP_PDENORM);
		else
			return (sign ? FP_NZERO : FP_PZERO);
	}
	if (exp == MAXEXP) { /* infinity or NaN */
		if ((HIFRACTION(x) == 0) && (LOFRACTION(x) == 0)) /* infinity */
			return (sign ? FP_NINF : FP_PINF);
		else
			if (QNANBIT(x))
			/* hi-bit of mantissa set - quiet nan */
				return (FP_QNAN);
			else	return (FP_SNAN);
	}
	/* if we reach here we have non-zero normalized number */
	return (sign ? FP_NNORM : FP_PNORM);
}

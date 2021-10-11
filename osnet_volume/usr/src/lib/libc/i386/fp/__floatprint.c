/*
 * Copyright (c) 1989, 1991, by Sun Microsystems, Inc.
 */

#ident  "@(#)__floatprint.c	1.2	96/08/26 SMI"

/* The following functions are used in floating-point base conversion.	 */

#include "synonyms.h"
#include "base_conversion.h"
#include <memory.h>

/* fix for 1202391  remove ieee_vals.c from i386 libc source */
 
#if defined(__i386) || defined(i386)

static double
min_normal(void) {
	union {
		unsigned u[2];
		double d;
	} x = {
		0x00000000U, 0x00100000U
	};
	return x.d;
}

/*ARGSUSED*/
static double
signaling_nan(long n) {
	union {
		unsigned u[2];
		double d;
	} x = {
		0x00000001U, 0x7ff00000U
	};
	return x.d;
}

#endif
/* Old Code -- Saved for Reference
 *
 * extern double min_normal(void);
 * extern double signaling_nan(long);
 */

void
__base_conversion_set_exception(ef)
	fp_exception_field_type ef;
{
	/*
	 * Cause hardware exception to be generated for each exception
	 * detected during base conversion.
	 */

	register double t;

	if (ef == (1 << fp_inexact)) {
		t = 9.999999962747097015E-1;
		/*
		 * 28 sig bits so product isn't inexact in extended
		 * accumulator, causing two inexact traps.
		 */
	} else if ((ef & (1 << fp_invalid)) != 0) {
		t = signaling_nan(0);
	} else if ((ef & (1 << fp_overflow)) != 0) {
		t = 4.149515553422842866E+180;
		/*
		 * 28 sig bits so product isn't inexact in extended
		 * accumulator, causing inexact trap prior to overflow trap
		 * on store.
		 */
	} else if ((ef & (1 << fp_underflow)) != 0) {
		t = min_normal();
	} else
		goto done;
	__base_conversion_write_only_double = t * t;	/* Storage forces
							 * exception */
done:	;
}

void
__double_to_digits(x, s, pn)
	double          x;
	char           *s;
	int            *pn;

/*
 * Converts a positive integral double 1 <= x <= 2147483647999999744 to *pn
 * <= 19 digits, stored in s[1-*pn]..s[0].
 * 
 * There may be as many as three leading zeros.
 */

{
	long            ix;
	short unsigned  ir;
	char           *pc;
	short unsigned  r;
	int             ihi = 0;
	int             chunkm1;

	pc = &(s[1]);

	if (x >= 1.0e16) {
		chunkm1 = 8;
		ihi = x * 1.0e-9;
		ix = x - 1.0e9 * ihi;
		if (ix < 0) {
			ix += 1000000000;
			ihi -= 1;
		} else if (ix >= 1000000000) {
			ix -= 1000000000;
			ihi += 1;
		}
	} else if (x > 2147483647.0) {
		chunkm1 = 7;
		ihi = x * 1.0e-8;
		ix = x - 1.0e8 * ihi;
		if (ix < 0) {
			ix += 100000000;
			ihi -= 1;
		} else if (ix >= 100000000) {
			ix -= 100000000;
			ihi += 1;
		}
	} else {
		chunkm1 = 0;
		ix = x;
	}

loop:
	if (ix >= 655360) {	/* loop in integer but can't use quorem */
		LONGQUOREM10000(ix, ir);
		pc -= 4;
		__four_digits_quick((short unsigned) ir, pc);
	}
	while (ix > 0) {	/* loop in integer and use quorem */
		QUOREM10000(ix, r);
		pc -= 4;
		__four_digits_quick(r, pc);
	}
	*pn = s - pc + 1;
	if (chunkm1 > 0) {	/* Go back and do some more - large argument. */
		if ((s - pc) < chunkm1)
			(void) memset(s - chunkm1, '0', pc - s + chunkm1);
		pc = s - chunkm1;
		ix = ihi;
		chunkm1 = 0;
		goto loop;
	}
}

double
__arint_set_n(x, nrx, pe)
	double          x;
	int             nrx;
	enum __fbe_type *pe;

/*
 * Converts double to integral value rounding to nearest, returns result and
 * exceptions.  nrx is the number of rounding errors x already contains. *pe
 * will get __fbe_many if the rounded value of x, rx,  is too close to the
 * ambiguous case: nrx > 0 & nrx * ulp(rx) >= 0.5 - |x-rx|
 */

{
	double          rx, rmx;
	double_equivalence kax;

	rx = __arint(x);
	rmx = rx - x;
	kax.x = rmx;
	kax.f.msw.sign = 0;	/* kax.x gets abs(rx-x) */
	rmx = kax.x;
	switch (nrx) {
	case 0:
		if (rmx == 0.0)
			*pe = __fbe_none;
		else if (rmx > 0.5)
			*pe = __fbe_many;
		else
			*pe = __fbe_one;
		break;
	case 1:
		*pe = (__abs_ulp(rx) < (0.5 - rmx)) ? __fbe_one : __fbe_many;
		break;
	default:
		*pe = ((2.0 * __abs_ulp(rx)) < (0.5 - rmx)) ? __fbe_one : __fbe_many;
		break;
	}
	return rx;
}

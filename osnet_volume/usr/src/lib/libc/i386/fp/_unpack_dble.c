/*
 * Copyright (c) 1990, 1991, by Sun Microsystems, Inc.
 */

#ident  "@(#)_unpack_dble.c	1.1	92/04/17 SMI"

#include "synonyms.h"
#include "base_conversion.h"

void
__fp_normalize(pu)
	unpacked *pu;

/* Normalize a number.  Does not affect zeros, infs, or NaNs. */

{
	int             i;
	short unsigned  nlzwords, nlzbits;
	long unsigned   t;

	if ((*pu).fpclass == fp_normal) {
		for (nlzwords = 0; (pu->significand[nlzwords] == 0) && (nlzwords < UNPACKED_SIZE); nlzwords++);
		if (nlzwords >= UNPACKED_SIZE) {
			(*pu).fpclass = fp_zero;
			return;
		}
		if (nlzwords > 0) {
			for (i = 0; i < UNPACKED_SIZE - (int)nlzwords; i++)
				pu->significand[i] = pu->significand[i + nlzwords];
			for (; i < UNPACKED_SIZE; i++)
				pu->significand[i] = 0;
			pu->exponent -= 32 * nlzwords;
		}
		for (; pu->significand[UNPACKED_SIZE - 1 - nlzwords] == 0; nlzwords++);
		/* nlzwords is now the count of trailing zero words. */

		nlzbits = 0;
		t = pu->significand[0];
		/* TESTS to determine normalize count.	 */

#define SHIFTMACRO(n) if (t <= (((unsigned long) 0xffffffff) >> n)) { t = t<<n ; nlzbits += n ; }
		SHIFTMACRO(16);
		SHIFTMACRO(8);
		SHIFTMACRO(4);
		SHIFTMACRO(2);
		SHIFTMACRO(1);
		pu->exponent -= nlzbits;
		if (nlzbits >= 1) {	/* small shift */
			unsigned long   high, low, shiftout = 0;
			for (i = UNPACKED_SIZE - 1 - nlzwords; i >= 0; i--) {
				high = pu->significand[i] << nlzbits;
				low = pu->significand[i] >> (32 - nlzbits);
				pu->significand[i] = shiftout | high;
				shiftout = low;
			}
		}
	}
}

void
__fp_set_exception(ex)
	enum fp_exception_type ex;

/* Set the exception bit in the current exception register. */

{
	_fp_current_exceptions |= 1 << (int) ex;
}

enum fp_class_type
__class_double(x)
	double         *x;
{
	double_equivalence kluge;

	kluge.x = *x;
	if (kluge.f.msw.exponent == 0) {	/* 0 or sub */
		if ((kluge.f.msw.significand == 0) && (kluge.f.significand2 == 0))
			return fp_zero;
		else
			return fp_subnormal;
	} else if (kluge.f.msw.exponent == 0x7ff) {	/* inf or nan */
		if ((kluge.f.msw.significand == 0) && (kluge.f.significand2 == 0))
			return fp_infinity;
		else if (kluge.f.msw.significand >= 0x40000)
			return fp_quiet;
		else
			return fp_signaling;
	} else
		return fp_normal;
}

void
__fp_leftshift(pu, n)
	unpacked       *pu;
	unsigned        n;

/* Left shift significand by 11 <= n <= 16 bits.  Affect all classes.	 */

{
	int             i;

	unsigned long   high, low, shiftout = 0;
	for (i = UNPACKED_SIZE - 1; i >= 0; i--) {
		high = pu->significand[i] << n;
		low = pu->significand[i] >> (32 - n);
		pu->significand[i] = shiftout | high;
		shiftout = low;
	}
}

void
__unpack_double(pu, px)
	unpacked       *pu;	/* unpacked result */
	double         *px;	/* packed double */
{
	double_equivalence x;
	int             i;

	x.x = *px;
	(*pu).sign = x.f.msw.sign;
	pu->significand[1] = x.f.significand2;
	for (i = 2; i < UNPACKED_SIZE; i++)
		pu->significand[i] = 0;
	if (x.f.msw.exponent == 0) {	/* zero or sub */
		if ((x.f.msw.significand == 0) && (x.f.significand2 == 0)) {	/* zero */
			pu->fpclass = fp_zero;
			return;
		} else {	/* subnormal */
			pu->fpclass = fp_normal;
			pu->exponent = 12 - DOUBLE_BIAS;
			pu->significand[0] = x.f.msw.significand;
			__fp_normalize(pu);
			return;
		}
	} else if (x.f.msw.exponent == 0x7ff) {	/* inf or nan */
		if ((x.f.msw.significand == 0) && (x.f.significand2 == 0)) {	/* inf */
			pu->fpclass = fp_infinity;
			return;
		} else {	/* nan */
			if ((x.f.msw.significand & 0x80000) != 0) {	/* quiet */
				pu->fpclass = fp_quiet;
			} else {/* signaling */
				pu->fpclass = fp_quiet;
				__fp_set_exception(fp_invalid);
			}
			pu->significand[0] = 0x80000 | x.f.msw.significand;
			__fp_leftshift(pu, 11);
			return;
		}
	}
	(*pu).exponent = x.f.msw.exponent - DOUBLE_BIAS;
	(*pu).fpclass = fp_normal;
	(*pu).significand[0] = 0x100000 | x.f.msw.significand;
	__fp_leftshift(pu, 11);
}

enum fp_class_type
__class_quadruple(x)
	quadruple      *x;
{
	quadruple_equivalence kluge;
	int             i;

#ifdef __STDC__
	kluge.x = *x;
#else
	for (i = 0; i < 4; i++)
		kluge.x.u[i] = x->u[i];
#endif
	if (kluge.f.msw.exponent == 0) {	/* 0 or sub */
		if ((kluge.f.msw.significand == 0) && (kluge.f.significand2 == 0) && (kluge.f.significand3 == 0) && (kluge.f.significand4 == 0))
			return fp_zero;
		else
			return fp_subnormal;
	} else if (kluge.f.msw.exponent == 0x7fff) {	/* inf or nan */
		if ((kluge.f.msw.significand == 0) && (kluge.f.significand2 == 0) && (kluge.f.significand3 == 0) && (kluge.f.significand4 == 0))
			return fp_infinity;
		else if ((kluge.f.msw.significand & 0xffff) >= (unsigned)0x8000)
			return fp_quiet;
		else
			return fp_signaling;
	} else
		return fp_normal;
}

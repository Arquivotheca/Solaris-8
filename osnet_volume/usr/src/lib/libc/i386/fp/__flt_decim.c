/*
 * Copyright (c) 1990, 1991, by Sun Microsystems, Inc.
 */

#ident  "@(#)__flt_decim.c	1.2	94/03/21 SMI"

/*
 * Conversion between single and extended binary and decimal
 * floating point - separated from double_to_decimal to minimize impact on
 * main(){printf("Hello");}
 */

#ifdef __STDC__
	#pragma weak extended_to_decimal = _extended_to_decimal
	#pragma weak single_to_decimal = _single_to_decimal
#endif

#include "synonyms.h"
#include "base_conversion.h"

void
_split_single_m1(px, pfint, pfrac)
	single_equivalence *px;
	_big_float     *pfint, *pfrac;

/* Exponent <= -1 so result is all fraction.		 */

{
	single_equivalence x = *px;

	pfint->blength = 0;
	pfint->bsignificand[0] = 0;
	pfrac->bsignificand[0] = x.f.msw.significand & 0xffff;
	pfrac->bsignificand[1] = x.f.msw.significand >> 16;
	pfrac->blength = 2;
	if (x.f.msw.exponent == 0) {	/* subnormal */
		pfrac->bexponent++;
		while (pfrac->bsignificand[pfrac->blength - 1] == 0)
			pfrac->blength--;	/* Normalize. */
	} else {		/* normal */
		pfrac->bsignificand[1] += 0x80;	/* Implicit bit */
	}
	_split_shorten(pfrac);
}

void
_split_single_6(px, pfint, pfrac)
	single_equivalence *px;
	_big_float     *pfint, *pfrac;

/* 0 <= exponent <= 6	 */

{
	single_equivalence x = *px;
	long unsigned   fmask, imask;

	fmask = (1 << (-pfint->bexponent - 16)) - 1;	/* Mask for fraction
							 * bits. */
	imask = 0xffff & ~fmask;
	pfint->bexponent += 16;
	pfint->blength = 1;
	pfint->bsignificand[0] = 0x80 | (imask & (x.f.msw.significand >> 16));
	/* _split_shorten(pfint); not needed */
	pfrac->blength = 2;
	pfrac->bsignificand[0] = 0xffff & x.f.msw.significand;
	pfrac->bsignificand[1] = fmask & (x.f.msw.significand >> 16);
	_split_shorten(pfrac);
}

void
_split_single_22(px, pfint, pfrac)
	single_equivalence *px;
	_big_float     *pfint, *pfrac;

/* 7 <= exponent <= 22	 */

{
	single_equivalence x = *px;
	long unsigned   fmask, imask;

	fmask = (1 << (-pfint->bexponent)) - 1;	/* Mask for fraction bits. */
	imask = 0xffff & ~fmask;
	pfint->blength = 2;
	pfint->bsignificand[0] = x.f.msw.significand & imask;
	pfint->bsignificand[1] = 0x80 | (x.f.msw.significand >> 16);
	_split_shorten(pfint);
	pfrac->blength = 1;
	pfrac->bsignificand[0] = x.f.msw.significand & fmask;
	_split_shorten(pfrac);
}

void
_split_single_23(px, pfint, pfrac)
	single_equivalence *px;
	_big_float     *pfint, *pfrac;

/* Exponent >= 23 so result is all integer.         */

{
	single_equivalence x = *px;

	pfrac->blength = 0;
	pfrac->bsignificand[0] = 0;
	pfint->blength = 2;
	pfint->bsignificand[0] = x.f.msw.significand & 0xffff;
	pfint->bsignificand[1] = 0x80 | (x.f.msw.significand >> 16);
	_split_shorten(pfint);
}

void
__unpack_single_two(px, pfint, pfrac)
	single         *px;
	_big_float     *pfint, *pfrac;
/*
 * Unpack normal or subnormal float into big_float integer and fraction
 * parts. One part may be zero, but not both.
 */

{
	single_equivalence x;
	int             exponent;
	_BIG_FLOAT_DIGIT sticky;

	x.x = *px;
	exponent = x.f.msw.exponent - SINGLE_BIAS;
	pfint->bexponent = exponent - 23;
	pfrac->bexponent = exponent - 23;
	pfint->bsize = _BIG_FLOAT_SIZE;
	pfrac->bsize = _BIG_FLOAT_SIZE;
	if (exponent <= 6) {	/* exponent <= 6 */
		if (exponent < 0) {	/* exponent <= -1 */
			_split_single_m1(&x, pfint, pfrac);
		}
		/* exponent <= -1 */
		else {		/* exponent in [0,6] */
			_split_single_6(&x, pfint, pfrac);
		}		/* exponent in [0,6] */
	}
	/* exponent <= 6 */
	else {			/* 7 <= exponent */
		if (exponent <= 22) {	/* exponent in [7,22] */
			_split_single_22(&x, pfint, pfrac);
		} else {	/* 23 <= exponent */
			_split_single_23(&x, pfint, pfrac);
		}		/* 23 <= exponent */
	}			/* 7 <= exponent */
	if ((pfint->bexponent < 0) && (pfint->blength > 0)) {
		__right_shift_base_two(pfint, (short unsigned) -pfint->bexponent, &sticky);	/* right shift */
		pfint->bexponent = 0;	/* adjust exponent */
		if (pfint->bsignificand[pfint->blength - 1] == 0)
			pfint->blength--;	/* check for leading zero */
	}
}

void
single_to_decimal(px, pm, pd, ps)
	single         *px;
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;
{
	single_equivalence kluge;
	fp_exception_field_type ef;

	ef = 0;			/* Initialize *ps - no exceptions. */
	kluge.x = *px;
	pd->sign = kluge.f.msw.sign;
	pd->fpclass = __class_single(px);
	switch (pd->fpclass) {
	case fp_normal:
	case fp_subnormal:
		__k_double_to_decimal((double) *px, pm, pd, &ef);
#ifndef fsoft
		if (ef != 0)
			__base_conversion_set_exception(ef);
#endif
	}
	*ps = ef;
}

void
__unpack_extended_two(px, pfint, pfrac)
	extended       *px;
	_big_float     *pfint, *pfrac;
/*
 * Unpack normal or subnormal extended into big_float integer and fraction
 * parts. One part may be zero, but not both.
 */
{
	extended_equivalence x;
	int             exponent;
	_BIG_FLOAT_DIGIT sticky;

	pfint->bsize = _BIG_FLOAT_SIZE;
	pfrac->bsize = _BIG_FLOAT_SIZE;
	x.x[0] = (*px)[0];
	x.x[1] = (*px)[1];
	x.x[2] = (*px)[2];
	exponent = x.f.msw.exponent - EXTENDED_BIAS;
	if (exponent <= -1) {	/* exponent <= -1 - all fraction, no integer */
		pfint->blength = 0;
		pfrac->bsignificand[0] = x.f.significand2 & 0xffff;
		pfrac->bsignificand[1] = x.f.significand2 >> 16;
		pfrac->bsignificand[2] = x.f.significand & 0xffff;
		pfrac->bsignificand[3] = x.f.significand >> 16;
		if (x.f.msw.exponent == 0) {	/* subnormal */
			pfrac->blength = 4;
			pfrac->bexponent = exponent - 62;
		} else {	/* normal */
			pfrac->blength = 4;
			pfrac->bexponent = exponent - 63;
		}
		while (pfrac->bsignificand[pfrac->blength - 1] == 0)
			pfrac->blength--;	/* remove leading zeros */
		_split_shorten(pfrac);
	} else if (exponent >= 63) {	/* exponent >= 63 - all integer, no
					 * fraction */
		pfint->bexponent = exponent - 63;
		pfint->blength = 4;
		pfrac->blength = 0;
		pfint->bsignificand[0] = x.f.significand2 & 0xffff;
		pfint->bsignificand[1] = x.f.significand2 >> 16;
		pfint->bsignificand[2] = x.f.significand & 0xffff;
		pfint->bsignificand[3] = x.f.significand >> 16;
		_split_shorten(pfint);
	} else {		/* 0 <= exponent <= 62 : 1.0 <= x < 2**63 :
				 * some integer, some fraction */
		_BIG_FLOAT_DIGIT u[4], fmask;
		int             i, midword = (62 - exponent) >> 4;	/* transition word
									 * between int and frac */

		pfint->bexponent = exponent - 63 + 16 * midword;
		pfrac->bexponent = exponent - 63;
		pfint->blength = 4 - midword;
		pfrac->blength = 1 + midword;
		u[0] = x.f.significand2 & 0xffff;
		u[1] = x.f.significand2 >> 16;
		u[2] = x.f.significand & 0xffff;
		u[3] = x.f.significand >> 16;
		for (i = 0; i < midword; i++) {
			pfrac->bsignificand[i] = u[i];
		}
		fmask = 1 + ((62 - exponent) & 0xf);	/* 1..16 */
		fmask = (1 << fmask) - 1;	/* 1..2**16-1 */
		pfrac->bsignificand[midword] = u[midword] & fmask;
		pfint->bsignificand[0] = u[midword] & ~fmask;
		for (i = midword + 1; i < 4; i++) {
			pfint->bsignificand[i - midword] = u[i];
		}
		while (pfrac->bsignificand[pfrac->blength - 1] == 0)
			pfrac->blength--;	/* remove leading zeros */
		if ((pfint->bexponent < 0) && (pfint->blength > 0)) {	/* Normalize if
									 * necessary */
			__right_shift_base_two(pfint, (short unsigned) -pfint->bexponent, &sticky);
			/* right shift */
			pfint->bexponent = 0;	/* adjust exponent */
			if (pfint->bsignificand[pfint->blength - 1] == 0)
				pfint->blength--;	/* remove leading zero */
		}
	}
}

void
extended_to_decimal(px, pm, pd, ps)
	extended       *px;
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;
{
	extended_equivalence kluge;
	_big_float      bfint, bfrac;
	fp_exception_field_type ef;

	ef = 0;			/* Initialize *ps - no exceptions. */
#ifdef i386
#define XMSW 2
#else
#define XMSW 0
#endif
	kluge.x[XMSW] = (*px)[XMSW];
	pd->sign = kluge.f.msw.sign;
	pd->fpclass = __class_extended(px);
	switch (pd->fpclass) {
	case fp_normal:
	case fp_subnormal:
		__unpack_extended_two(px, &bfint, &bfrac);
		_unpacked_to_decimal_two(&bfint, &bfrac, pm, pd, &ef);
#ifndef fsoft
		if (ef != 0)
			__base_conversion_set_exception(ef);
#endif
	}
	*ps = ef;
}

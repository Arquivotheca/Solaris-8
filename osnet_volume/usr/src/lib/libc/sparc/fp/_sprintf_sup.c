/*
 * Copyright (c) 1989, 1991, 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)_sprintf_sup.c	1.6	96/12/06 SMI"
/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include "base_conversion.h"
/*
 * Fundamental utilities of base conversion required for sprintf - but too
 * complex or too seldom used to be worth assembly language coding.
 */

unsigned int
__prodc_b10000(_BIG_FLOAT_DIGIT x, _BIG_FLOAT_DIGIT y, unsigned int c)
	/*
	 * p = x * y + c ; return (p/10000 << 16 |
	 * p%10000)
	 */
{
	unsigned int   p = x * (unsigned int) y + c;
	unsigned short  r;

	QUOREM10000(p, r);
	return ((p << 16) | r);
}

unsigned int
__lshift_b10000(_BIG_FLOAT_DIGIT x, unsigned short n, unsigned int c)
	/*
	 * p = x << n + c ; return (p/10000 << 16 |
	 * p%10000)
	 */
{
	unsigned int   p = (((unsigned int) x) << n) + c;
	unsigned short  r;

	QUOREM10000(p, r);
	return ((p << 16) | r);
}

void
__left_shift_base_ten(_big_float *pbf, unsigned short multiplier)

{
	/*
	 * Multiply a base-10**4 significand by 2<<multiplier.  Extend length
	 * as necessary to accommodate carries.
	 */

	unsigned short  length = pbf->blength;
	int	j;
	unsigned int	carry;
	int	p;

	carry = 0;
	for (j = 0; j < (int) length; j++) {
		p = __lshift_b10000((_BIG_FLOAT_DIGIT) pbf->bsignificand[j],
			    multiplier, carry);
		pbf->bsignificand[j] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
	while (carry != 0) {
		__carry_out_b10000(p, carry);
		pbf->bsignificand[j++] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
	pbf->blength = (unsigned short) j;
}

void
__right_shift_base_two(_big_float *pbf, unsigned short multiplier,
			_BIG_FLOAT_DIGIT *sticky)

{
	/* *pb = *pb / 2**multiplier	to normalize.	15 <= multiplier <= 1 */
	/* Any bits shifted out got to *sticky. */

	unsigned int	p;
	int	j;
	unsigned int	carry;

	carry = 0;
	for (j = pbf->blength - 1; j >= 0; j--) {
		p = _rshift_b65536(pbf->bsignificand[j], multiplier, carry);
		pbf->bsignificand[j] = (_BIG_FLOAT_DIGIT) (p >> 16);
		carry = p & 0xffff;
	}
	*sticky = (_BIG_FLOAT_DIGIT) carry;
}

void
__multiply_base_ten(_big_float *pbf, _BIG_FLOAT_DIGIT multiplier)
{
	/*
	 * Multiply a base-10**4 significand by multiplier.  Extend length as
	 * necessary to accommodate carries.
	 */

	int	j;
	unsigned int	carry;
	int	p;

	carry = 0;
	for (j = 0; j < (int) pbf->blength; j++) {
		p = __prodc_b10000((_BIG_FLOAT_DIGIT) pbf->bsignificand[j],
		    multiplier, carry);
		pbf->bsignificand[j] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
	while (carry != 0) {
		__carry_out_b10000(p, carry);
		pbf->bsignificand[j++] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
	pbf->blength = (unsigned short) j;
}

void
__multiply_base_two(_big_float *pbf, _BIG_FLOAT_DIGIT multiplier,
			unsigned int carry)
{
	/*
	 * Multiply a base-2**16 significand by multiplier.  Extend length as
	 * necessary to accommodate carries.
	 */

	unsigned short  length = pbf->blength;
	unsigned int  p;
	int	j;

	for (j = 0; j < (int) length; j++) {
		p = __prodc_b65536(pbf->bsignificand[j], multiplier, carry);
		pbf->bsignificand[j] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
	if (carry != 0) {
		pbf->bsignificand[j++] = (_BIG_FLOAT_DIGIT)
		    (__carry_out_b65536(carry) & 0xffff);
	}
	pbf->blength = (unsigned short) j;
}

void
__multiply_base_ten_by_two(_big_float *pbf, unsigned short multiplier)
{
	/*
	 * Multiply a base-10**4 significand by 2**multiplier.  Extend length
	 * as necessary to accommodate carries.
	 */

	unsigned short	length = pbf->blength;
	int	j;
	unsigned int	carry, p;

	carry = 0;
	for (j = 0; j < (int) length; j++) {
		p = __lshift_b10000(pbf->bsignificand[j], multiplier, carry);
		pbf->bsignificand[j] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
	while (carry != 0) {
		__carry_out_b10000(p, carry);
		pbf->bsignificand[j++] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
	pbf->blength = (unsigned short) j;
}

void
__mul_65536short(unsigned int carry, _BIG_FLOAT_DIGIT *ps, unsigned short *pn)
/* *pbf *= 65536 ; += carry ; */
{
	int	j = *pn;

	carry = ___mul_65536_n(carry, ps, j);
	while (carry != 0) {
		QUOREM10000(carry, ps[j]);
		j++;
	}
	*pn = (unsigned short) j;
}

void
__big_binary_to_big_decimal(_big_float *pb, _big_float *pd)

{
	/* Convert _big_float from binary form to decimal form. */

	int	i;

	pd->bsignificand[1] = __quorem10000((unsigned int)
		    pb->bsignificand[pb->blength - 1], &(pd->bsignificand[0]));
	if (pd->bsignificand[1] == 0) {
		pd->blength = 1;
	} else {
		pd->blength = 2;
	}
	for (i = pb->blength - 2; i >= 0; i--) {
		/*
		 * Multiply by 2**16 and
		 * add next significand.
		 */
		__mul_65536short((unsigned int) pb->bsignificand[i],
		    (&pd->bsignificand[0]), &(pd->blength));
	}
	for (i = 0; i <= (pb->bexponent - 16); i += 16) {
		/*
		 * Multiply by 2**16 for
		 * each trailing zero.
		 */
		__mul_65536short((unsigned int) 0,
			    (&pd->bsignificand[0]), &(pd->blength));
	}
	if (pb->bexponent > i)
		__left_shift_base_ten(pd, (unsigned short) (pb->bexponent - i));
	pd->bexponent = 0;

#ifdef DEBUG
	printf(" __big_binary_to_big_decimal ");
	__display_big_float(pd, 10);
#endif
}

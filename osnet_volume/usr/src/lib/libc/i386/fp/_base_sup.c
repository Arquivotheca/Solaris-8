/*
 * Copyright (c) 1990, 1991, by Sun Microsystems, Inc.
 */

#ident  "@(#)_base_sup.c	1.1	92/04/17 SMI"

#include "synonyms.h"
#include "base_conversion.h"

#ifdef DEBUG

void
__display_big_float(pbf, base)
	_big_float     *pbf;
	unsigned        base;

{
	int             i;

	for (i = 0; i < pbf->blength; i++) {
		switch (base) {
		case 2:
			printf(" + %d * 2** %d", pbf->bsignificand[i], (16 * i + pbf->bexponent));
			break;
		case 10:
			printf(" + %d * 10** %d", pbf->bsignificand[i], (4 * i + pbf->bexponent));
			break;
		}
		if ((i % 4) == 3)
			printf("\n");
	}
	printf("\n");
}

#endif

void
__integerstring_to_big_decimal(ds, ndigs, nzin, pnzout, pd)
	char            ds[];
	unsigned        ndigs, nzin, *pnzout;
	_big_float     *pd;

{
	/*
	 * Convert ndigs decimal digits from ds, and up to 3 trailing zeros,
	 * into a decimal big_float in *pd.  nzin tells how many implicit
	 * trailing zeros may be used, while *pnzout tells how many were
	 * actually absorbed.  Up to 3 are used if available so that
	 * (ndigs+*pnzout) % 4 = 0.
	 */

	int             extras, taken, id, ids;

#ifdef DEBUG
	printf(" __integerstring_to_big_decimal: ndigs %d nzin %d ds %s \n", ndigs, nzin, ds);
#endif

	/* Compute how many trailing zeros we're going to put in *pd. */

	extras = ndigs % 4;
	if ((extras > 0) && (nzin != 0)) {
		taken = 4 - extras;
		if (taken > nzin)
			taken = nzin;
	} else
		taken = 0;

	*pnzout = nzin - taken;

#define IDIGIT(i) ((i < 0) ? 0 : ((i < ndigs) ? (ds[i] - '0') : 0))

	pd->bexponent = 0;
	pd->blength = (ndigs + taken + 3) / 4;

	ids = (ndigs + taken) - 4 * pd->blength;
	id = pd->blength - 1;

#ifdef DEBUG
	printf(" __integerstring_to_big_decimal exponent %d ids %d id %d \n", pd->bexponent, ids, id);
#endif

	pd->bsignificand[id] = 1000 * IDIGIT(ids) + 100 * IDIGIT(ids + 1) + 10 * IDIGIT(ids + 2) + IDIGIT(ids + 3);
	ids += 4;

	for (; ids < (int) (ndigs + taken - 4); ids += 4) {	/* Additional digits to
								 * be found. Main loop. */
		id--;
		pd->bsignificand[id] = 1000 * ds[ids] + 100 * ds[ids + 1] + 10 * ds[ids + 2] + ds[ids + 3] - 1111 * '0';
	}

#ifdef DEBUG
	assert((id == 1) || (id == 0));
#endif
	if (id != 0)
		pd->bsignificand[0] = 1000 * IDIGIT(ids) + 100 * IDIGIT(ids + 1) + 10 * IDIGIT(ids + 2) + IDIGIT(ids + 3);

#ifdef DEBUG
	printf(" __integerstring_to_big_decimal: ");
	__display_big_float(pd, 10);
#endif
}

void
__fractionstring_to_big_decimal(ds, ndigs, nzin, pbf)
	char            ds[];
	unsigned        ndigs, nzin;
	_big_float     *pbf;

{
	/*
	 * Converts a decimal string containing an implicit point, nzin
	 * leading implicit zeros, and ndigs explicit digits, into a big
	 * float.
	 */

	int             ids, ibf;

#ifdef DEBUG
	printf(" _fractionstring_to_big_decimal ndigs %d nzin %d s %s \n", ndigs, nzin, ds);
#endif

	pbf->bexponent = -(int) (nzin + ndigs);
	pbf->blength = (ndigs + 3) / 4;

	ids = nzin + ndigs - 4 * pbf->blength;
	ibf = pbf->blength - 1;

#ifdef DEBUG
	printf(" _fractionstring_to_big_decimal exponent %d ids %d ibf %d \n", pbf->bexponent, ids, ibf);
#endif

#define FDIGIT(i) ((i < nzin) ? 0 : ((i < (nzin+ndigs)) ? (ds[i-nzin] - '0') : 0))

	pbf->bsignificand[ibf] = 1000 * FDIGIT(ids) + 100 * FDIGIT(ids + 1) + 10 * FDIGIT(ids + 2) + FDIGIT(ids + 3);
	ids += 4;

	for (; ids < (int) (nzin + ndigs - 4); ids += 4) {	/* Additional digits to
								 * be found. Main loop. */
		ibf--;
		pbf->bsignificand[ibf] = 1000 * ds[ids - nzin] + 100 * ds[ids + 1 - nzin] + 10 * ds[ids + 2 - nzin] + ds[ids + 3 - nzin] - 1111 * '0';
	}

	if (ibf > 0) {
#ifdef DEBUG
		assert(ibf == 1);
#endif
		pbf->bsignificand[0] = 1000 * FDIGIT(ids) + 100 * FDIGIT(ids + 1) + 10 * FDIGIT(ids + 2) + FDIGIT(ids + 3);
	} else {
#ifdef DEBUG
		assert(ibf == 0);
#endif
	}

#ifdef DEBUG
	printf(" _fractionstring_to_big_decimal: ");
	__display_big_float(pbf, 10);
#endif
}

unsigned long
__prod_10000_b65536(x, c)	/* p = x * 10000 + c ; return p */
	_BIG_FLOAT_DIGIT x;
	long unsigned   c;
{
	return x * (unsigned long) 10000 + c;
}

void
__mul_10000short(pbf, carry)
	_big_float     *pbf;
	long unsigned   carry;
{
	int             j;
	long unsigned   p;

	for (j = 0; j < (int) pbf->blength; j++) {
		p = __prod_10000_b65536(pbf->bsignificand[j], carry);
		pbf->bsignificand[j] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
	while (carry != 0) {
		__carry_out_b10000(p, carry);
		pbf->bsignificand[j++] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
	pbf->blength = j;
}

void
__big_decimal_to_big_binary(pd, pb)
	_big_float     *pb, *pd;

{
	/* Convert _big_float from decimal form to binary form. */

	int             id, idbound;
	_BIG_FLOAT_DIGIT sticky, carry;
	_BIG_FLOAT_DIGIT multiplier;
	char            bcastring[80];

#ifdef DEBUG
	assert(pd->bexponent >= -3);
	assert(pd->bexponent <= 4);
#endif
	pb->bexponent = 0;
	pb->blength = 1;
	id = pd->blength - 1;
	if ((id == 0) && (pd->bexponent < 0)) {
		pb->bsignificand[0] = 0;
	} else {
		pb->bsignificand[0] = pd->bsignificand[id--];
		idbound = (pd->bexponent < 0) ? 1 : 0;	/* How far to carry next
							 * for loop depends on
							 * whether last digit
							 * requires special
							 * treatment. */
		for (; id >= idbound; id--) {
			__mul_10000short(pb, (long unsigned) pd->bsignificand[id]);
		}
	}
	if (pd->bexponent < 0) {/* Have to save some integer bits, discard
				 * and stick some fraction bits at the end. */
#ifdef DEBUG
		assert(id == 0);
#endif
		sticky = 0;
		carry = pd->bsignificand[0];
		switch (pd->bexponent) {
		case -1:
			carry = __quorem((unsigned long) carry, 10, &sticky);
			multiplier = 1000;
			break;
		case -2:
			carry = __quorem((unsigned long) carry, 100, &sticky);
			multiplier = 100;
			break;
		case -3:
			carry = __quorem((unsigned long) carry, 1000, &sticky);
			multiplier = 10;
			break;
		default:
/* Ifdef'ed out diagnostic message to keep tsort happy. */
#ifdef DEBUG
			(void) sprintf(bcastring, " __big_decimal_to_big_binary exponent %d ", pd->bexponent);
#endif
			__base_conversion_abort(ERANGE, bcastring);
		}
		__multiply_base_two(pb, multiplier, (long unsigned) carry);
		if (sticky != 0)
			pb->bsignificand[0] |= 1;	/* Save lost bits. */
	} else if (pd->bexponent > 0) {	/* Have to append some zeros. */
		switch (pd->bexponent) {
		case 1:
			multiplier = 10;
			break;
		case 2:
			multiplier = 100;
			break;
		case 3:
			multiplier = 1000;
			break;
		case 4:
			multiplier = 10000;
			break;
		default:
/* Ifdef'ed out diagnostic message to keep tsort happy. */
#ifdef DEBUG
			(void) sprintf(bcastring, " __big_decimal_to_big_binary exponent %d ", pd->bexponent);
#endif
			__base_conversion_abort(ERANGE, bcastring);
		}
		carry = 0;
		__multiply_base_two(pb, multiplier, (long unsigned) carry);
	}
#ifdef DEBUG
	printf(" __big_decimal_to_big_binary ");
	__display_big_float(pb, 2);
#endif
}

__big_binary_to_unpacked(pb, pu)
	_big_float     *pb;
	unpacked       *pu;

{
	/* Convert a binary big_float to a binary_unpacked.	 */

	int             ib, iu;

#ifdef DEBUG
	assert(pb->bsignificand[pb->blength - 1] != 0);	/* Assert pb is
							 * normalized. */
#endif

	iu = 0;
	for (ib = pb->blength - 1; ((ib - 1) >= 0) && (iu < UNPACKED_SIZE); ib -= 2) {
		pu->significand[iu++] = pb->bsignificand[ib] << 16 | pb->bsignificand[ib - 1];
	}
	if (iu < UNPACKED_SIZE) {	/* The big float fits in the unpacked
					 * with no rounding. 	 */
		if (ib == 0)
			pu->significand[iu++] = pb->bsignificand[ib] << 16;
		for (; iu < UNPACKED_SIZE; iu++)
			pu->significand[iu] = 0;
	} else {		/* The big float is too big; chop, stick, and
				 * normalize. */
		while (pb->bsignificand[ib] == 0)
			ib--;
		if (ib >= 0)
			pu->significand[UNPACKED_SIZE - 1] |= 1;	/* Stick lsb if nonzero
									 * found. */
	}

	pu->exponent = 16 * pb->blength + pb->bexponent - 1;
	__fp_normalize(pu);

#ifdef DEBUG
	printf(" __big_binary_to_unpacked \n");
	__display_unpacked(pu);
#endif
}

void
__left_shift_base_two(pbf, multiplier)
	_big_float     *pbf;
	short unsigned  multiplier;
{
	/*
	 * Multiply a base-2**16 significand by 2<<multiplier.  Extend length
	 * as necessary to accommodate carries.
	 */

	short unsigned  length = pbf->blength;
	long unsigned   p;
	int             j;
	unsigned long   carry;

	carry = 0;
	for (j = 0; j < (int) length; j++) {
		p = __lshift_b65536(pbf->bsignificand[j], multiplier, carry);
		pbf->bsignificand[j] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
	if (carry != 0) {
		pbf->bsignificand[j++] = (_BIG_FLOAT_DIGIT) (__carry_out_b65536(carry) & 0xffff);
	}
	pbf->blength = j;
}


/* The following functions are used in floating-point base conversion.	 */

double
__digits_to_double(s, n, pe)
	char           *s;
	int             n;
	enum __fbe_type *pe;
/*
 * Converts n decimal ascii digits into double.  Up to 15 digits are always
 * exactly representable, up to 18 can be easily represented with one
 * rounding error.
 */
{
	long int        acc, t2, t8;
	int             i;
	double          t;

	if (n >= 19) {
		*pe = __fbe_many;
		t = 0.0;
	} else if (n <= 9) {
		acc = s[0] - '0';
		for (i = 1; i < n; i++) {
			t2 = acc << 1;	/* 2 * acc */
			t8 = t2 << 2;	/* 8 * acc */
			acc = t2 + t8 + s[i] - '0';
		}
		*pe = __fbe_none;
		t = (double) acc;
	} else {
		acc = s[0] - '0';
		for (i = 1; i < (n - 9); i++) {
			t2 = acc << 1;	/* 2 * acc */
			t8 = t2 << 2;	/* 8 * acc */
			acc = t2 + t8 + s[i] - '0';
		}
		t = 1.0e9 * (double) acc;	/* This will be exact. */
		acc = s[n - 9];
		for (i = n - 8; i < n; i++) {
			t2 = acc << 1;	/* 2 * acc */
			t8 = t2 << 2;	/* 8 * acc */
			acc = t2 + t8 + s[i];
		}
		t = __add_set(t, (double) (acc - 0x3DE43550 /* nine '0' worth */ ), pe);
	}
	return t;
}

/*
 * Copyright (c) 1990, 1991, by Sun Microsystems, Inc.
 */

#ident  "@(#)decimal_bin.c	1.1	92/04/17 SMI"

/* Conversion between binary and decimal floating point. */

#ifdef __STDC__  
	#pragma weak decimal_to_double = _decimal_to_double 
	#pragma weak decimal_to_extended = _decimal_to_extended 
	#pragma weak decimal_to_quadruple = _decimal_to_quadruple 
	#pragma weak decimal_to_single = _decimal_to_single 
#endif

#include "synonyms.h"
#include <string.h>
#include "base_conversion.h"

void
__decimal_to_binary_integer(ds, ndigs, nzeros, nsig, pb)
char            ds[];	/* Input decimal integer string. */
unsigned        ndigs;		/* Input number of explicit digits in ds. */
unsigned        nzeros;		/* Input number of implicit trailing zeros. */
unsigned        nsig;		/* Input number of significant bits required. */
_big_float     *pb;		/* Pointer to big_float to receive result. */

/*
 * Converts a decimal integer string ds with ndigs explicit leading digits
 * and nzeros implicit trailing zeros to a _big_float **pb, which only
 * requires nsig significand bits.
 */
/* Inexactness is indicated by pb->bsignificand[0] |= 1. */
/*
 * If the input is too big for a big_float, pb->bexponent is set to 0x7fff.
 */

{
	unsigned        nzout;
	_big_float      d, *pbout;

	d.bsize = _BIG_FLOAT_SIZE;
	__integerstring_to_big_decimal(ds, ndigs, nzeros, &nzout, &d);
	__big_decimal_to_big_binary(&d, pb);
	if (nzout != 0) {
		__big_float_times_power(pb, 10, (int) nzout, (int) nsig, &pbout);
		if (pbout == BIG_FLOAT_TIMES_TOOBIG) {
#ifdef DEBUG
			(void) printf(" __decimal_to_binary_integer: decimal exponent %d too large for tables \n", nzout);
#endif
			pb->bexponent = 0x7fff;
		} else if (pbout == BIG_FLOAT_TIMES_NOMEM) {
			char            bcastring[80];

/* Ifdef'ed out diagnostic message to keep tsort happy. */
#ifdef DEBUG
			(void) sprintf(bcastring, " decimal exponent %d ", nzout);
#endif
			__base_conversion_abort(ENOMEM, bcastring);
		} else {
#ifdef DEBUG
			if (pbout != pb)
				(void) printf(" __decimal_to_binary_integer: large decimal exponent %d needs heap buffer \n", nzout);
			printf(" __decimal_to_binary_integer: product ");
			__display_big_float(pb, 2);
#endif
			if (pbout != pb) {	/* We don't really need such
						 * a large product; the
						 * target can't be more than
						 * a quad! */
				int             i, allweneed;

				allweneed = 2 + (nsig + 2) / 16;
				for (i = 0; i < allweneed; i++)
					pb->bsignificand[i] = pbout->bsignificand[i + pbout->blength - allweneed];
				for (i = 0; (pbout->bsignificand[i] == 0); i++);
				if (i < ((int)pbout->blength - allweneed))
					pb->bsignificand[0] |= 1;	/* Stick discarded bits. */

				pb->blength = allweneed;
				pb->bexponent = pbout->bexponent + 16 * (pbout->blength - allweneed);
#ifdef DEBUG
				printf(" __decimal_to_binary_integer: removed %d excess digits from product \n", pbout->blength - allweneed);
				__display_big_float(pb, 2);
#endif
				__free_big_float(pbout);
			}
		}
	}
}

void
__decimal_to_binary_fraction(ds, ndigs, nzeros, nsig, pb)
	char            ds[];	/* Decimal integer string input. */
unsigned        ndigs;		/* Number of explicit digits to read. */
unsigned        nzeros;		/* Number of implicit leading zeros before
				 * digits. */
unsigned        nsig;		/* Number of significant bits needed. */
_big_float     *pb;		/* Pointer to intended big_float result. */

/*
 * Converts an explicit decimal string *ds[0]..*ds[ndigs-1] preceded by
 * nzeros implicit leading zeros after the point into a big_float at *pb. If
 * the input does not fit exactly in a big_float, the least significant bit
 * of pbout->significand is stuck on. If the input is too big for the base
 * conversion tables, pb->bexponent is set to 0x7fff.
 */

{
	unsigned        twopower, twosig;
	int             i, excess;
	_big_float      d, *pdout;

	d.bsize = _BIG_FLOAT_SIZE;
	__fractionstring_to_big_decimal(ds, ndigs, nzeros, &d);

	twopower = nsig + 3 + (((nzeros + 1) * (unsigned long) 217706) >> 16);
	twosig = 1 + (((nsig + 2) * (unsigned long) 19729) >> 16);

#ifdef DEBUG
	printf(" __decimal_to_binary_fraction sigbits %d twopower %d twosig %d \n",
	       nsig, twopower, twosig);
#endif
	__big_float_times_power(&d, 2, (int) twopower, (int) twosig, &pdout);
	if (pdout == BIG_FLOAT_TIMES_TOOBIG) {
#ifdef DEBUG
		(void) printf(" __decimal_to_binary_fraction binary exponent %d too large for tables ", twopower);
#endif
		pb->bexponent = 0x7fff;
		goto ret;
	} else if (pdout == BIG_FLOAT_TIMES_NOMEM) {
		char            bcastring[80];

/* Ifdef'ed out diagnostic message to keep tsort happy. */
#ifdef DEBUG
		(void) sprintf(bcastring, " binary exponent %d ", twopower);
#endif
		__base_conversion_abort(ENOMEM, bcastring);
	} else {
#ifdef DEBUG
		if (&d != pdout)
			printf(" __decimal_to_binary_fraction large binary exponent %d needs heap buffer \n", twopower);
		printf(" product ");
		__display_big_float(pdout, 10);
#endif
	}


	if (pdout->bexponent <= -4) {
		/* Have computed appropriate decimal part; now toss fraction. */
		excess = (-pdout->bexponent) / 4;
#ifdef DEBUG
		printf(" discard %d excess fraction digits \n", 4 * excess);
#endif
		for (i = 0; (i < excess) && ((pdout)->bsignificand[i] == 0); i++);
		if (i < excess)
			(pdout)->bsignificand[excess] |= 1;	/* Sticky bit for
								 * discarded fraction. */
		for (i = excess; i < (int)(pdout)->blength; i++)
			(pdout)->bsignificand[i - excess] = (pdout)->bsignificand[i];

		(pdout)->blength -= excess;
		(pdout)->bexponent += 4 * excess;
	}
	__big_decimal_to_big_binary(pdout, pb);
	if (pdout != &d)
		__free_big_float(pdout);
	pb->bexponent = -twopower;

ret:
	return;
}

void
__decimal_to_unpacked(px, pd, significant_bits)
	unpacked       *px;
	decimal_record *pd;
	unsigned        significant_bits;

/*
 * Converts *pd to *px so that *px can be correctly rounded. significant_bits
 * tells how many bits will be significant in the final result to avoid
 * superfluous computation. Inexactness is communicated by sticking on the
 * lsb of px->significand[UNPACKED_SIZE-1]. Integer buffer overflow is
 * indicated with a huge positive exponent.
 */

{
	int             frac_bits, sigint;
	unsigned        length, ndigs, ntz, nlz, ifrac, nfrac;
	_big_float      bi, bf, *ptounpacked = &bi;

	px->sign = pd->sign;
	px->fpclass = pd->fpclass;
	if ((px->fpclass != fp_normal) && (px->fpclass != fp_subnormal))
		goto ret;
	for (length = 0; pd->ds[length] != 0; length++);
	if (length == 0) {	/* A zero significand slipped by. */
		px->fpclass = fp_zero;
		goto ret;
	}
	/* Length contains the number of explicit digits in string. */
	if (pd->exponent >= 0) {/* All integer digits. */
		ndigs = length;
		ntz = pd->exponent;	/* Trailing zeros. */
		ifrac = 0;
		nfrac = 0;	/* No fraction digits. */
		nlz = 0;
	} else if (length <= -pd->exponent) {	/* No integer digits. */
		ndigs = 0;
		ntz = 0;
		ifrac = 0;
		nfrac = length;
		nlz = -pd->exponent - length;	/* Leading zeros. */
	} else {		/* Some integer digits, some fraction digits. */
		ndigs = length + pd->exponent;
		ntz = 0;
		ifrac = ndigs;
		nfrac = -pd->exponent;
		nlz = 0;
		while ((pd->ds[ifrac] == '0') && (nfrac != 0)) {
			ifrac++;
			nfrac--;
			nlz++;
		}		/* Remove leading zeros. */
	}
	if (ndigs != 0) {	/* Convert integer digits. */

		bi.bsize = _BIG_FLOAT_SIZE;
		__decimal_to_binary_integer(pd->ds, ndigs, ntz, significant_bits, &bi);
		if (bi.bexponent == 0x7fff) {	/* Too big for buffer. */
			px->exponent = 0x000fffff;
			px->significand[0] = 0x80000000;
			goto ret;
		}
		sigint = 16 * (bi.blength + bi.bexponent - 1);
		if (sigint < 0)
			sigint = 0;
	} else {		/* No integer digits. */
		bi.blength = 0;
		bi.bsignificand[0] = 0;
		bi.bexponent = 0;
		sigint = 0;
	}
	frac_bits = significant_bits - sigint + 2;
	bf.blength = 0;
	if ((nfrac != 0) && (frac_bits > 0)) {	/* Convert fraction digits,
						 * even if we only need a
						 * round or sticky.  */

		bf.bsize = _BIG_FLOAT_SIZE;
		__decimal_to_binary_fraction(&(pd->ds[ifrac]), nfrac, nlz, (unsigned) frac_bits, &bf);
	} else {		/* Only need fraction bits for sticky. */
		if (nfrac != 0)
			bi.bsignificand[0] |= 1;	/* Stick for fraction. */
	}
	if (bi.blength == 0) {	/* No integer digits; all fraction. */
		if (bf.bexponent == 0x7fff) {	/* Buffer overflowed. */
			px->exponent = -0x000fffff;
			px->significand[0] = 0x80000000;
			goto ret;
		}
		ptounpacked = &bf;	/* Exceptional case - all fraction. */
		goto punpack;
	}
	if (bf.blength != 0) {	/* Combine integer and fraction bits. */
		int             expdiff = bi.bexponent - (bf.bexponent + 16 * (bf.blength - 1));	/* Exponent difference. */
		int             uneeded = 2 + (significant_bits + 2) / 16;	/* Number of big float
										 * digits needed. */
		int             nmove, leftshift, i, if0;

#ifdef DEBUG
		printf(" bi+bf exponent diff is %d \n", expdiff);
		printf(" need %d big float digits \n", uneeded);
		assert(bi.blength != 0);
		assert(bf.blength != 0);
		assert(bi.bsignificand[bi.blength - 1] != 0);	/* Normalized bi. */
		assert(bf.bsignificand[bf.blength - 1] != 0);	/* Normalized bf. */
		assert(bi.bexponent >= 0);	/* bi is all integer */
		assert(((-bf.bexponent - 16 * (bf.blength - 1)) >= 16) ||
		       ((bf.bsignificand[bf.blength - 1] >> (-bf.bexponent - 16 * (bf.blength - 1))) == 0));
		/* assert either bf << 1 or bf < 1 */
		/*
		 * Assert that integer and fraction parts don't overlap by
		 * more than one big digit.
		 */
		assert(expdiff > 0);
		assert(uneeded <= (2 * UNPACKED_SIZE));
#endif


		if ((int)bi.blength >= uneeded) {	/* bi will overflow unpacked,
						 * so bf is just a sticky. */
			bi.bsignificand[0] |= 1;
			goto punpack;
		}
		leftshift = 16 - (expdiff % 16);
		if (leftshift > 0) {	/* shift bf to align with bi. */
			expdiff += 16 * bf.blength;
			__left_shift_base_two(&bf, (short unsigned) leftshift);
			expdiff -= 16 * bf.blength;	/* If bf.blength is
							 * longer, adjust
							 * expdiff. */
		}
		expdiff += leftshift;
		expdiff /= 16;	/* Remaining expdiff in _BIG_FLOAT_DIGITS. */
		expdiff--;
#ifdef DEBUG
		assert(expdiff >= 0);	/* expdiff is now equal to the size
					 * of the hole between bi and bf. */
#endif
		nmove = uneeded - bi.blength;
		/* nmove is the number of words to add to bi. */
		if (nmove < 0)
			nmove = 0;
		if (nmove > (expdiff + (int)bf.blength))
			nmove = (expdiff + bf.blength);
#ifdef DEBUG
		printf(" increase bi by %d words to merge \n", nmove);
#endif
		if (nmove == 0)
			i = -1;
		else
			for (i = (bi.blength - 1 + nmove); i >= nmove; i--)
				bi.bsignificand[i] = bi.bsignificand[i - nmove];
		for (; (i >= 0) && (expdiff > 0); i--) {	/* Fill hole with zeros. */
			expdiff--;
			bi.bsignificand[i] = 0;
		}
		if0 = i;
		for (; i >= 0; i--)
			bi.bsignificand[i] = bf.bsignificand[i + bf.blength - 1 - if0];
		for (i = (bf.blength - 2 - if0); bf.bsignificand[i] == 0; i--);
		/* Find first non-zero. */
		if (i >= 0)
			bi.bsignificand[0] |= 1;	/* If non-zero found,
							 * stick it. */
		bi.blength += nmove;
		bi.bexponent -= 16 * nmove;
		goto punpack;
	}
punpack:
	ptounpacked->bsignificand[0] |= pd->more;	/* Stick in any lost
							 * digits. */

#ifdef DEBUG
	printf(" merged bi and bf: ");
	__display_big_float(ptounpacked, 2);
#endif

	__big_binary_to_unpacked(ptounpacked, px);

ret:
	return;
}


#ifndef fsoft

int
__inrange_single(px, pm, pd, ps)
	single         *px;
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;
{
#ifdef PRINTNOTQUICK
#define NOTQUICK(N) {  printf("\n notquick %c \n",N); goto notquick;}
#define NOTQUICKX(N) {  printf("\n notquick %c \n",N); goto notquickx;}
#else
#define NOTQUICK(N) {  goto notquick;}
#define NOTQUICKX(N) {  goto notquickx;}
#endif
	double          dds, au, ddsplus, ddsminus, df1;
	enum __fbe_type eround;
	int             esum;
	int             ncs, nds, idsexp;
	float           f1, f2;
	__ieee_flags_type fb;
	fp_exception_field_type ef;

	if (pm->rd != fp_nearest)
		NOTQUICK('P');	/* -fsoft doesn't have rounding modes */
	__get_ieee_flags(&fb);
	idsexp = pd->exponent;
	nds = pd->ndigits;
	if (nds <= 18) {
		esum = 0;
		ncs = nds;
	} else {
		esum = 1;
		ncs = 18;
		idsexp += nds - 18;
	}
	dds = __digits_to_double(pd->ds, ncs, &eround);
	if (eround != __fbe_none)
		esum++;
	if (pd->sign == 1)
		dds = -dds;

#if f68881 || f80387
	/* Define maximum and minimum exponents according to table size.	 */
#define IDSMAX	(__TBL_TENS_MAX)
#define IDSMIN  (-__TBL_TENS_MAX)
#else
	/*
	 * Define maximum and minimum exponents to avoid costly overflow and
	 * underflow.
	 */
#define IDSMAX	(38-ncs)
#define IDSMIN  ((ncs >= (__TBL_TENS_MAX-36))?(-__TBL_TENS_MAX):-(ncs+36))
#endif

	if ((idsexp <= IDSMAX) && (idsexp > 0)) {	/* small positive
							 * exponent */
		if (idsexp > __TBL_TENS_EXACT)
			esum++;
		if (esum > 0)
			dds *= __tbl_tens[idsexp];
		else {
			dds = __mul_set(dds, __tbl_tens[idsexp], &eround);
			if (eround != __fbe_none)
				esum++;
		}
	}
	/* small positive exponent */
	else if ((idsexp <= -1) && (idsexp >= IDSMIN)) {	/* small negative
								 * exponent */
		if (-idsexp > __TBL_TENS_EXACT)
			esum++;
		if (esum > 0)
			dds *= __tbl_ntens[-idsexp];
		else {
			dds = __div_set(dds, __tbl_tens[-idsexp], &eround);
			if (eround != __fbe_none)
				esum++;
		}
	} else if (idsexp != 0) {	/* exponent magnitude too large. */
		NOTQUICKX('R');
	} else if (esum > 0) {
		NOTQUICKX('r');	/* many digits * 2**0 ; might be exact */
	}
	/*
	 * At this point dds may have four rounding errors due to more than
	 * 18 significant digits incorrect rounding of 16..18 digits inexact
	 * power of ten inexact multiplication We assume the worst.
	 */
	au = 4.0 * __abs_ulp(dds);
	ddsplus = dds + au;
	ddsminus = dds - au;
	f1 = (float) (ddsplus);
	f2 = (float) (ddsminus);
	if (f1 != f2)
		NOTQUICKX('S');
	df1 = f1;
	if (esum > 0) {
		if (df1 < ddsplus) {
			if (ddsminus < df1)
				NOTQUICKX('Q');	/* Might be exact - can't be
						 * sure. */
		} else {
			if (ddsminus > df1)
				NOTQUICKX('q');	/* Might be exact - can't be
						 * sure. */
		}
		ef = (fp_exception_field_type) (1 << fp_inexact);
	} else
		ef = (df1 == dds) ? (fp_exception_field_type) 0 : (fp_exception_field_type) (1 << fp_inexact);
	*ps = ef;
	*px = f1;
	__set_ieee_flags(&fb);
	return 1;

notquickx:
	__set_ieee_flags(&fb);
notquick:			/* Result can't be computed by simplified
				 * means. */
	return 0;
}

int
__inrange_double(px, pm, pd, ps)
	double         *px;
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;

/*
 * Attempts conversion using floating-point arithmetic. Returns 1 if it
 * works, 0 if not.
 */

{
	double          dds;
	enum __fbe_type eround;
	int             esum = 0, nds;
	__ieee_flags_type fb;
	fp_exception_field_type ef;

	if (pm->rd != fp_nearest)
		NOTQUICK('s');	/* -fsoft doesn't have rounding modes */
	nds = pd->ndigits;
	if (nds >= 19)
		NOTQUICK('t');	/* Too long a string for accurate conversion. */
	__get_ieee_flags(&fb);
	if ((pd->exponent <= __TBL_TENS_EXACT) && (pd->exponent >= 1)) {	/* small positive
										 * exponent */
		dds = __digits_to_double(pd->ds, nds, &eround);
		if (eround != __fbe_none)
			NOTQUICKX('T');
		if (pd->sign == 1)
			dds = -dds;
		dds = __mul_set(dds, __tbl_tens[pd->exponent], &eround);
		esum += (int) eround;
	} else if ((-pd->exponent <= __TBL_TENS_EXACT) && (-pd->exponent >= 1)) {	/* small negative
											 * exponent */
		dds = __digits_to_double(pd->ds, nds, &eround);
		if (eround != __fbe_none)
			NOTQUICKX('U');
		if (pd->sign == 1)
			dds = -dds;
		dds = __div_set(dds, __tbl_tens[-pd->exponent], &eround);
		esum += (int) eround;
	} else if (pd->exponent != 0) {	/* exponent magnitude too large. */
		NOTQUICKX('V');
	} else {
		dds = __digits_to_double(pd->ds, nds, &eround);
		if (eround != __fbe_none)
			NOTQUICKX('v');
		if (pd->sign == 1)
			dds = -dds;
	}
	if (esum > 1)
		NOTQUICKX('W');
	*px = dds;
	ef = (esum == 0) ? (fp_exception_field_type) 0 : (fp_exception_field_type) (1 << fp_inexact);
	__set_ieee_flags(&fb);
	*ps = ef;
	return 1;

notquickx:
	__set_ieee_flags(&fb);
notquick:			/* Result can't be computed by simplified
				 * means. */
	return 0;
}

int
__inrange_quadex(px, pm, pd, ps)
	double         *px;
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;

/*
 * Attempts conversion to double using floating-point arithmetic. Returns 1
 * if it works (NO rounding errors), 0 if it doesn't.
 */

{
	double          dds;
	enum __fbe_type eround;
	int             nds;
	__ieee_flags_type fb;

	if (pm->rd != fp_nearest)
		NOTQUICK('w');	/* -fsoft doesn't have rounding modes */
	__get_ieee_flags(&fb);
	nds = pd->ndigits;
	if ((pd->exponent <= __TBL_TENS_EXACT) && (pd->exponent > 0)) {	/* small positive
									 * exponent */
		dds = __digits_to_double(pd->ds, nds, &eround);
		if (eround != __fbe_none)
			NOTQUICKX('X');
		if (pd->sign == 1)
			dds = -dds;
		dds = __mul_set(dds, __tbl_tens[pd->exponent], &eround);
		if (eround != __fbe_none)
			NOTQUICKX('x');
	}
	/* small positive exponent */
	else if ((pd->exponent <= -1) && (pd->exponent >= -__TBL_TENS_EXACT)) {	/* small negative
										 * exponent */
		dds = __digits_to_double(pd->ds, nds, &eround);
		if (eround != __fbe_none)
			NOTQUICKX('Y');
		if (pd->sign == 1)
			dds = -dds;
		dds = __div_set(dds, __tbl_tens[-pd->exponent], &eround);
		if (eround != __fbe_none)
			NOTQUICKX('y');
	} else if (pd->exponent != 0) {	/* exponent magnitude too large. */
		NOTQUICKX('Z');
	} else {
		dds = __digits_to_double(pd->ds, nds, &eround);
		if (eround != __fbe_none)
			NOTQUICKX('z');
		if (pd->sign == 1)
			dds = -dds;
	}
	*px = dds;
	*ps = (fp_exception_field_type) 0;
	__set_ieee_flags(&fb);
	return 1;
notquickx:
	__set_ieee_flags(&fb);
notquick:			/* Result can't be computed by simplified
				 * means. */
	return 0;
}

#endif				/* ifndef fsoft */

/* PUBLIC FUNCTIONS */

/*
 * decimal_to_floating routines convert the decimal record at *pd to the
 * floating type item at *px, observing the modes specified in *pm and
 * setting exceptions in *ps.
 * 
 * pd->sign and pd->fpclass are always taken into account.  pd->exponent and
 * pd->ds are used when pd->fpclass is fp_normal or fp_subnormal. In these
 * cases pd->ds is expected to contain one or more ascii digits followed by a
 * null. px is set to a correctly rounded approximation to
 * (sign)*(ds)*10**(exponent) If pd->more != 0 then additional nonzero digits
 * are assumed to follow those in ds; fp_inexact is set accordingly.
 * 
 * Thus if pd->exponent == -2 and pd->ds = "1234", *px will get 12.34 rounded to
 * storage precision.
 * 
 * px is correctly rounded according to the IEEE rounding modes in pm->rd.  *ps
 * is set to contain fp_inexact, fp_underflow, or fp_overflow if any of these
 * arise.
 * 
 * pd->ndigits, pm->df, and pm->ndigits are never used.
 * 
 */

void
decimal_to_extended(px, pm, pd, ps)
	extended       *px;
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;
{
	extended_equivalence kluge;
	unpacked        u;
	double          dd;
	fp_exception_field_type ef;

	ef = 0;			/* Initialize to no floating-point
				 * exceptions. */
	kluge.f.msw.sign = pd->sign ? 1 : 0;
	switch (pd->fpclass) {
	case fp_zero:
		kluge.f.msw.exponent = 0;
		kluge.f.significand = 0;
		kluge.f.significand2 = 0;
		break;
	case fp_infinity:
		kluge.f.msw.exponent = 0x7fff;
		kluge.f.significand = 0;
		kluge.f.significand2 = 0;
		break;
	case fp_quiet:
		kluge.f.msw.exponent = 0x7fff;
		kluge.f.significand = 0xffffffff;
		kluge.f.significand2 = 0xffffffff;
		break;
	case fp_signaling:
		kluge.f.msw.exponent = 0x7fff;
		kluge.f.significand = 0x3fffffff;
		kluge.f.significand2 = 0xffffffff;
		break;
	default:
		if (pd->exponent > EXTENDED_MAXE) {	/* Guaranteed overflow. */
			u.sign = pd->sign == 0 ? 0 : 1;
			u.fpclass = fp_normal;
			u.exponent = 0x000fffff;
			u.significand[0] = 0x80000000;
		} else if (pd->exponent >= -EXTENDED_MAXE) {	/* Guaranteed in range. */
			goto inrange;
		} else if (pd->exponent <= (-EXTENDED_MAXE - DECIMAL_STRING_LENGTH)) {	/* Guaranteed deep
											 * underflow. */
			goto underflow;
		} else {	/* Deep underflow possible, depending on
				 * string length. */
			int             i;

			for (i = 0; (pd->ds[i] != 0) && (i < (-pd->exponent - EXTENDED_MAXE)); i++);
			if (i < (-pd->exponent - EXTENDED_MAXE)) {	/* Deep underflow */
		underflow:
				u.sign = pd->sign == 0 ? 0 : 1;
				u.fpclass = fp_normal;
				u.exponent = -0x000fffff;
				u.significand[0] = 0x80000000;
			} else {/* In range. */
		inrange:
#ifndef fsoft
				if (__inrange_quadex(&dd, pm, pd, &ef) == 1)
					__unpack_double(&u, &dd);
				else
#endif
					__decimal_to_unpacked(&u, pd, 64);
			}
		}
		_fp_current_exceptions = 0;
		_fp_current_direction = pm->rd;
		_fp_current_precision = fp_extended;
		__pack_extended(&u, px);
		ef |= _fp_current_exceptions;
		*ps = ef;
#ifndef fsoft
		if (ef != 0)
			__base_conversion_set_exception(ef);
#endif
		return;
	}
	(*px)[0] = kluge.x[0];
	(*px)[1] = kluge.x[1];
	(*px)[2] = kluge.x[2];
	*ps = ef;
}

void
decimal_to_quadruple(px, pm, pd, ps)
	quadruple      *px;
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;
{
	quadruple_equivalence kluge;
	unpacked        u;
	int             i;
	double          dd;
	fp_exception_field_type ef;

	ef = 0;			/* Initialize to no floating-point
				 * exceptions. */
	kluge.f.msw.sign = pd->sign ? 1 : 0;
	switch (pd->fpclass) {
	case fp_zero:
		kluge.f.msw.exponent = 0;
		kluge.f.msw.significand = 0;
		kluge.f.significand2 = 0;
		kluge.f.significand3 = 0;
		kluge.f.significand4 = 0;
		break;
	case fp_infinity:
		kluge.f.msw.exponent = 0x7fff;
		kluge.f.msw.significand = 0;
		kluge.f.significand2 = 0;
		kluge.f.significand3 = 0;
		kluge.f.significand4 = 0;
		break;
	case fp_quiet:
		kluge.f.msw.exponent = 0x7fff;
		kluge.f.msw.significand = 0xffff;
		kluge.f.significand2 = 0xffffffff;
		kluge.f.significand3 = 0xffffffff;
		kluge.f.significand4 = 0xffffffff;
		break;
	case fp_signaling:
		kluge.f.msw.exponent = 0x7fff;
		kluge.f.msw.significand = 0x7fff;
		kluge.f.significand2 = 0xffffffff;
		kluge.f.significand3 = 0xffffffff;
		kluge.f.significand4 = 0xffffffff;
		break;
	default:
		if (pd->exponent > QUAD_MAXE) {	/* Guaranteed overflow. */
			u.sign = pd->sign == 0 ? 0 : 1;
			u.fpclass = fp_normal;
			u.exponent = 0x000fffff;
			u.significand[0] = 0x80000000;
		} else if (pd->exponent >= -QUAD_MAXE) {	/* Guaranteed in range. */
			goto inrange;
		} else if (pd->exponent <= (-QUAD_MAXE - DECIMAL_STRING_LENGTH)) {	/* Guaranteed deep
											 * underflow. */
			goto underflow;
		} else {	/* Deep underflow possible, depending on
				 * string length. */

			for (i = 0; (pd->ds[i] != 0) && (i < (-pd->exponent - QUAD_MAXE)); i++);
			if (i < (-pd->exponent - QUAD_MAXE)) {	/* Deep underflow */
		underflow:
				u.sign = pd->sign == 0 ? 0 : 1;
				u.fpclass = fp_normal;
				u.exponent = -0x000fffff;
				u.significand[0] = 0x80000000;
			} else {/* In range. */
		inrange:
#ifndef fsoft
				if (__inrange_quadex(&dd, pm, pd, &ef) == 1)
					__unpack_double(&u, &dd);
				else
#endif
					__decimal_to_unpacked(&u, pd, 113);
			}
		}
		_fp_current_exceptions = 0;
		_fp_current_direction = pm->rd;
		__pack_quadruple(&u, px);
		ef |= _fp_current_exceptions;
		*ps = ef;
#ifndef fsoft
		if (ef != 0)
			__base_conversion_set_exception(ef);
#endif
		return;
	}
#ifdef __STDC__
	*px = kluge.x;
#else
	for (i = 0; i < 4; i++)
		px->u[i] = kluge.x.u[i];
#endif				/* __STDC__ */
	*ps = ef;
}


void
decimal_to_single(px, pm, pd, ps)
	single         *px;
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;
{
	single_equivalence kluge;
	unpacked        u;
	fp_exception_field_type ef;

	ef = 0;			/* Initialize to no floating-point
				 * exceptions. */
	kluge.f.msw.sign = pd->sign ? 1 : 0;
	switch (pd->fpclass) {
	case fp_zero:
		kluge.f.msw.exponent = 0;
		kluge.f.msw.significand = 0;
		break;
	case fp_infinity:
		kluge.f.msw.exponent = 0xff;
		kluge.f.msw.significand = 0;
		break;
	case fp_quiet:
		kluge.f.msw.exponent = 0xff;
		kluge.f.msw.significand = 0x7fffff;
		break;
	case fp_signaling:
		kluge.f.msw.exponent = 0xff;
		kluge.f.msw.significand = 0x3fffff;
		break;
	default:
		if (pd->exponent > SINGLE_MAXE) {	/* Guaranteed overflow. */
			u.sign = pd->sign == 0 ? 0 : 1;
			u.fpclass = fp_normal;
			u.exponent = 0x000fffff;
			u.significand[0] = 0x80000000;
		} else if (pd->exponent >= -SINGLE_MAXE) {	/* Guaranteed in range. */
			goto inrange;
		} else if (pd->exponent <= (-SINGLE_MAXE - DECIMAL_STRING_LENGTH)) {	/* Guaranteed deep
											 * underflow. */
			goto underflow;
		} else {	/* Deep underflow possible, depending on
				 * string length. */
			int             i;

			for (i = 0; (pd->ds[i] != 0) && (i < (-pd->exponent - SINGLE_MAXE)); i++);
			if (i < (-pd->exponent - SINGLE_MAXE)) {	/* Deep underflow */
		underflow:
				u.sign = pd->sign == 0 ? 0 : 1;
				u.fpclass = fp_normal;
				u.exponent = -0x000fffff;
				u.significand[0] = 0x80000000;
			} else {/* In range. */
		inrange:
#ifndef fsoft
				if (__inrange_single(px, pm, pd, &ef) == 1)
					goto done;
#endif
				/*
				 * Result can't be computed by simplified
				 * means.
				 */
				__decimal_to_unpacked(&u, pd, 24);

			}
		}
		_fp_current_exceptions = 0;
		_fp_current_direction = pm->rd;
		__pack_single(&u, &kluge.x);
		ef |= _fp_current_exceptions;
	}
	*px = kluge.x;
done:	;
	*ps = ef;
#ifndef fsoft
	if (ef != 0)
		__base_conversion_set_exception(ef);
#endif
}

void
decimal_to_double(px, pm, pd, ps)
	double         *px;
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;
{
	double_equivalence kluge;
	unpacked        u;
	fp_exception_field_type ef;

	ef = 0;			/* Initialize to no floating-point
				 * exceptions. */
	kluge.f.msw.sign = pd->sign ? 1 : 0;
	switch (pd->fpclass) {
	case fp_zero:
		kluge.f.msw.exponent = 0;
		kluge.f.msw.significand = 0;
		kluge.f.significand2 = 0;
		break;
	case fp_infinity:
		kluge.f.msw.exponent = 0x7ff;
		kluge.f.msw.significand = 0;
		kluge.f.significand2 = 0;
		break;
	case fp_quiet:
		kluge.f.msw.exponent = 0x7ff;
		kluge.f.msw.significand = 0xfffff;
		kluge.f.significand2 = 0xffffffff;
		break;
	case fp_signaling:
		kluge.f.msw.exponent = 0x7ff;
		kluge.f.msw.significand = 0x7ffff;
		kluge.f.significand2 = 0xffffffff;
		break;
	default:
		if (pd->exponent > DOUBLE_MAXE) {	/* Guaranteed overflow. */
			u.sign = pd->sign == 0 ? 0 : 1;
			u.fpclass = fp_normal;
			u.exponent = 0x000fffff;
			u.significand[0] = 0x80000000;
		} else if (pd->exponent >= -DOUBLE_MAXE) {	/* Guaranteed in range. */
			goto inrange;
		} else if (pd->exponent <= (-DOUBLE_MAXE - DECIMAL_STRING_LENGTH)) {	/* Guaranteed deep
											 * underflow. */
			goto underflow;
		} else {	/* Deep underflow possible, depending on
				 * string length. */
			int             i;

			for (i = 0; (pd->ds[i] != 0) && (i < (-pd->exponent - DOUBLE_MAXE)); i++);
			if (i < (-pd->exponent - DOUBLE_MAXE)) {	/* Deep underflow */
		underflow:
				u.sign = pd->sign == 0 ? 0 : 1;
				u.fpclass = fp_normal;
				u.exponent = -0x000fffff;
				u.significand[0] = 0x80000000;
			} else {/* In range. */
		inrange:
#ifndef fsoft
				if (__inrange_double(px, pm, pd, &ef) == 1)
					goto done;
				/*
				 * Result can't be computed by simplified
				 * means.
				 */
#endif
				__decimal_to_unpacked(&u, pd, 53);
			}
		}
		_fp_current_exceptions = 0;
		_fp_current_direction = pm->rd;
		__pack_double(&u, &kluge.x);
		ef |= _fp_current_exceptions;
	}
	*px = kluge.x;
done:	;
	*ps = ef;
#ifndef fsoft
	if (ef != 0)
		__base_conversion_set_exception(ef);
#endif
}

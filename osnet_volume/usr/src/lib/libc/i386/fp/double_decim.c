#if !defined(lint) && defined(SCCSIDS)
static char     sccsid[] = "@(#)double_decim.c 1.3 93/12/09 SMI";
#endif

/*
 * Copyright (C) 1990 by Sun Microsystems, Inc.
 */

/* Conversion between binary and decimal floating point. */

#ifdef __STDC__
	#pragma weak double_to_decimal = _double_to_decimal
	#pragma weak quadruple_to_decimal = _quadruple_to_decimal
#endif

#include "synonyms.h"
#include <memory.h>
#include <stdio.h>
#include "base_conversion.h"

/* PRIVATE FUNCTIONS */

void
__decimal_round(pm, pd, ps, round, sticky)
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;
	char            round;
	unsigned        sticky;

/*
 * Rounds decimal record *pd according to modes in *pm, recording exceptions
 * for inexact or overflow in *ps.  round is the round digit and sticky is 0
 * or non-zero to indicate exact or inexact. pd->ndigits is expected to be
 * correctly set.
 */

{
	int             lsd, i;

	if ((round == '0') && (sticky == 0)) {	/* Exact. */
		goto done;
	}
	*ps |= 1 << fp_inexact;

	switch (pm->rd) {
	case fp_nearest:
		if (round < '5')
			goto done;
		if (round > '5')
			goto roundup;
		if (sticky != 0)
			goto roundup;
		/* Now in ambiguous case; round up if lsd is odd. */
		if (pd->ndigits <= 0)
			goto done;	/* Presumed 0. */
		lsd = pd->ds[pd->ndigits - 1] - '0';
		if ((lsd % 2) == 0)
			goto done;
		goto roundup;
	case fp_positive:
		if (pd->sign != 0)
			goto done;
		goto roundup;
	case fp_negative:
		if (pd->sign == 0)
			goto done;
		goto roundup;
	case fp_tozero:
		goto done;
	}
roundup:
	for (i = (pd->ndigits - 1); (pd->ds[i] == '9') && (i >= 0); i--)
		pd->ds[i] = '0';
	if (i >= 0)
		(pd->ds[i])++;
	else {			/* Rounding carry out has occurred. */
		pd->ds[0] = '1';
		if (pm->df == floating_form) {	/* For E format, simply
						 * adjust exponent. */
			pd->exponent++;
		} else {	/* For F format, increase length of string. */
			if (pd->ndigits > 0)
				pd->ds[pd->ndigits] = '0';
			pd->ndigits++;
		}
	}
	goto ret;
done:
	if (pd->ndigits <= 0) {	/* Create zero string. */
		pd->ds[0] = '0';
		pd->ndigits = 1;
	}
ret:
	pd->ds[pd->ndigits] = 0;/* Terminate string. */
	return;
}

void
__binary_to_decimal_integer(pfint, nsig, ds, nzeros, ndigs)
	_big_float     *pfint;
	unsigned        nsig;	/* Input number of significant digits
				 * required. */
	char            ds[];	/* Output decimal integer string output -
				 * must be large enough. */
unsigned       *nzeros;		/* Output number of implicit trailing zeros
				 * produced. */
unsigned       *ndigs;		/* Output number of explicit digits produced
				 * in ds. */

/*
 * Converts an unpacked integer value *pu into a decimal string in *ds, of
 * length returned in *ndigs.  Inexactness is indicated by setting
 * ds[ndigs-1] odd.
 */

{

	_big_float     *pd, d;
	int             e, i, is, excessbdigits;
	char            s[4];

	d.bsize = _BIG_FLOAT_SIZE;
	e = pfint->bexponent;
	pfint->bexponent = 0;
	__big_binary_to_big_decimal(pfint, &d);
	if (e <= 0)
		pd = &d;
	else {
		__big_float_times_power(&d, 2, e, (int) nsig, &pd);
		if (pd == BIG_FLOAT_TIMES_TOOBIG) {
			char            bcastring[80];

#ifdef DEBUG
			(void) sprintf(bcastring, " binary exponent %d ", e);
#endif
			__base_conversion_abort(ERANGE, bcastring);
		} else if (pd == BIG_FLOAT_TIMES_NOMEM) {
			char            bcastring[80];

#ifdef DEBUG
			(void) sprintf(bcastring, " binary exponent %d ", e);
#endif
			__base_conversion_abort(ENOMEM, bcastring);
		} else {
#ifdef DEBUG
			if (pd != &d)
				(void) printf(" large binary exponent %d needs heap buffer \n", e);
			printf(" product ");
			__display_big_float(pd, 10);
#endif
		}
	}
	__four_digits_quick((short unsigned) pd->bsignificand[pd->blength - 1], s);
	for (i = 0; s[i] == '0'; i++);	/* Find first non-zero digit. */
	for (is = 0; i <= 3;)
		ds[is++] = s[i++];	/* Copy subsequent digits. */

	excessbdigits = pd->blength - (DECIMAL_STRING_LENGTH - 1) / 4;
	if (excessbdigits < 0)
		excessbdigits = 0;

	for (i = (pd->blength - 2); i >= excessbdigits; i--) {	/* Convert powers of
								 * 10**4 to decimal
								 * digits. */
		__four_digits_quick((short unsigned) pd->bsignificand[i], &(ds[is]));
		is += 4;
	}
	for (; i >= 0; i--) {	/* Check excess b digits for nonzero */
		if (pd->bsignificand[i] != 0) {	/* mark lsb sticky */
			ds[is - 1] |= 1;
			break;
		}
	}
	ds[is] = 0;
	*ndigs = is;
	*nzeros = pd->bexponent + 4 * excessbdigits;
	if (pd != &d)
		__free_big_float(pd);

#ifdef DEBUG
	printf(" binary to decimal integer result %s * 10**%d \n", ds, *nzeros);
#endif
}

void
__binary_to_decimal_fraction(pfrac, nsig, nfrac, ds, nzeros, ndigs)
	_big_float     *pfrac;
	unsigned        nsig;	/* Input number of significant digits
				 * required. */
	unsigned        nfrac;	/* Input number of digits after point
				 * required. */
	char            ds[];	/* Output decimal integer string output -
				 * must be large enough. */
int            *nzeros;		/* Output number of implicit leading zeros
				 * produced. */
int            *ndigs;		/* Output number of explicit digits produced
				 * in ds. */

/*
 * Converts an unpacked fraction value *pu into a decimal string consisting
 * of a) an implicit '.' b) *nzeros implicit leading zeros c) *ndigs explicit
 * digits in ds ds contains at least nsig significant digits. nzeros + *
 * *ndigs is at least nfrac digits after the point. Inexactness is indicated
 * by sticking to the lsb.
 */

{
	_big_float     *pb, d;
	int             i, j, is, excess;
	char            s[4];
	int             tensig, tenpower;
	_BIG_FLOAT_DIGIT stickyshift;

	*nzeros = 0;
	if (pfrac->blength == 0) {	/* Exact zero. */
		int             ncopy;
		ncopy = (nsig > nfrac) ? nsig : nfrac;
		(void) memset(ds, '0', ncopy);
		*ndigs = ncopy;
		return;
	}
	d.bsize = _BIG_FLOAT_SIZE;
	tenpower = nsig + (int) (((17 - pfrac->bexponent - 16 * pfrac->blength) * (unsigned long) 19729) >> 16);
	if (tenpower < nfrac)
		tenpower = nfrac;
	tensig = nfrac;
	if (nsig > tensig)
		tensig = nsig;
	tensig = 1 + (((tensig + 2) * 217706) >> 16);

#ifdef DEBUG
	(void) printf(" binary to decimal fraction nsig %d nfrac %d tenpower %d tensig %d \n", nsig, nfrac, tenpower, tensig);
#endif
	__big_float_times_power(pfrac, 10, tenpower, tensig, &pb);
	if (pb == BIG_FLOAT_TIMES_TOOBIG) {
		char            bcastring[80];

#ifdef DEBUG
		(void) sprintf(bcastring, " decimal exponent %d ", tenpower);
#endif
		__base_conversion_abort(ERANGE, bcastring);
	} else if (pb == BIG_FLOAT_TIMES_NOMEM) {
		char            bcastring[80];

#ifdef DEBUG
		(void) sprintf(bcastring, " decimal exponent %d ", tenpower);
#endif
		__base_conversion_abort(ENOMEM, bcastring);
	} else {
#ifdef DEBUG
		if (pb != pfrac)
			printf(" large decimal exponent %d needs heap buffer \n", tenpower);
		printf(" product ");
		__display_big_float(pb, 2);
#endif
	}

	if (pb->bexponent <= -16) {
		/* Have computed appropriate decimal part; now toss fraction. */
		excess = (-pb->bexponent) / 16;
		if (excess >= (int)pb->blength) {
			ds[0] = '1';	/* sticky bit */
			is = 1;
			goto finish_ds;
		}
#ifdef DEBUG
		printf(" discard %d excess fraction bits \n", 16 * excess);
#endif
		for (i = 0; (i < excess) && (pb->bsignificand[i] == 0); i++);
		if (i < excess)
			pb->bsignificand[excess] |= 1;	/* Sticky bit for
							 * discarded fraction. */
		for (i = excess; i < (int)pb->blength; i++)
			pb->bsignificand[i - excess] = pb->bsignificand[i];
		/* can't use memcpy because of possible overlap	 */
		pb->blength -= excess;
		pb->bexponent += 16 * excess;
	}
	if (pb->bexponent < 0) {
		__right_shift_base_two(pb, (short unsigned) -pb->bexponent, &stickyshift);
		if (stickyshift != 0)
			pb->bsignificand[0] |= 1;	/* Stick to lsb. */
	}
	__big_binary_to_big_decimal(pb, &d);

	i = d.blength - 1;
	while (d.bsignificand[i] == 0)
		i--;
	__four_digits_quick((short unsigned) d.bsignificand[i], s);
	for (j = 0; s[j] == '0'; j++);	/* Find first non-zero digit. */
	for (is = 0; j <= 3;)
		ds[is++] = s[j++];	/* Copy subsequent digits. */

	for (i--; i >= 0; i--) {/* Convert powers of 10**4 to decimal digits. */
		__four_digits_quick((short unsigned) d.bsignificand[i], &(ds[is]));
		is += 4;
	}
finish_ds:
	ds[is] = 0;
	*ndigs = is;
#ifdef DEBUG
	assert(tenpower >= is);
#endif
	*nzeros = tenpower - is;/* There were supposed to be tenpower leading
				 * digits, and is were found. */

	if (pb != pfrac)
		__free_big_float(pb);

#ifdef DEBUG
	printf(" binary to decimal fraction result .%s * 10**%d \n", ds, -(*nzeros));
#endif

}

void
_unpacked_to_decimal_two(pfint, pfrac, pm, pd, ps)
	_big_float     *pfint, *pfrac;
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;
{
	int             i, intdigs, fracdigs, fraczeros, fracsigs, ids = 0, idsbound, lzbound, fracnonzero;
	unsigned        nsig, nfrac, intzeros, intsigs;
	decimal_string  is, fs;
	char            round = '0';
	unsigned        sticky = 0;

	if ((pm->ndigits >= DECIMAL_STRING_LENGTH) ||
	    ((pm->df == floating_form) && (pm->ndigits < 1))) {	/* Gross overflow or bad
								 * spec. */
overflow:
		*ps |= 1 << fp_overflow;
		return;
	}
	if (pfint->blength > 0) {	/* Compute integer part of result. */
		if (pm->df == floating_form)
			nsig = pm->ndigits + 1;	/* Significant digits wanted
						 * for E format, plus one for
						 * rounding. */
		else
			nsig = DECIMAL_STRING_LENGTH - 1;	/* Significant digits
								 * wanted for F format
								 * == all. */

		__binary_to_decimal_integer(pfint, nsig, is, &intzeros, &intsigs);
	} else {
		intsigs = 0;
		intzeros = 0;
	}
	intdigs = intsigs + intzeros;
	fracdigs = 0;
	fracnonzero = (pfrac->blength != 0);
	if (((pm->df == fixed_form) && (pm->ndigits >= 0)) ||
	    ((pm->df == floating_form) && ((pm->ndigits + 1) > intdigs))) {	/* Need to compute
										 * fraction part. */
		if (pm->df == floating_form) {	/* Need more significant
						 * digits. */
			nsig = pm->ndigits + 2 - intdigs;	/* Add two for rounding,
								 * sticky. */
			if (nsig > DECIMAL_STRING_LENGTH)
				nsig = DECIMAL_STRING_LENGTH;
			nfrac = 1;
		} else {	/* Need fraction digits. */
			nsig = 0;
			nfrac = pm->ndigits + 2;	/* Add two for rounding,
							 * sticky. */
			if (nfrac > DECIMAL_STRING_LENGTH)
				nfrac = DECIMAL_STRING_LENGTH;
		}
		__binary_to_decimal_fraction(pfrac, nsig, nfrac, fs, &fraczeros, &fracsigs);
		fracdigs = fraczeros + fracsigs;
	}
	if (pm->df == floating_form) {	/* Combine integer and fraction for E
					 * format. */
		idsbound = intsigs;
		if (idsbound > pm->ndigits)
			idsbound = pm->ndigits;
		{
			int             ncopy = idsbound;
			if (idsbound > 0) {
				(void) memcpy((char *) (pd->ds), (char *) is, (int) ncopy);
				ids = ncopy;
			}
		}
		/* Put integer into output string. */
		idsbound = intsigs + intzeros;
		if (idsbound > pm->ndigits)
			idsbound = pm->ndigits;
		if (idsbound > ids) {
			(void) memset(&(pd->ds[ids]), '0', idsbound - ids);
			ids = idsbound;
		}
		if (ids == pm->ndigits) {	/* Integer part had enough
						 * significant digits. */
			pd->ndigits = ids;
			pd->exponent = intdigs - ids;
			if (ids < intdigs) {	/* Gather rounding info. */
				if (ids < intsigs)
					round = is[ids++];
				else
					round = '0';
				for (; (is[ids] == '0') && (ids < intsigs); ids++);
				if (ids < intsigs)
					sticky = 1;
				if (fracnonzero)
					sticky = 1;
			} else {/* Integer part is exact - round from
				 * fraction. */
				if (fracnonzero) {
					int             stickystart;
					/* Fraction non-zero. */
					if (fraczeros > 0) {	/* Round digit is zero. */
						round = '0';
						stickystart = 0;	/* Stickies start with
									 * fs[0]. */
					} else {	/* Round digit is fs[0]. */
						round = fs[0];
						stickystart = 1;	/* Stickies start with
									 * fs[1]. */
					}
					if (sticky == 0) {	/* Search for sticky
								 * bits. */
						for (ids = stickystart; (fs[ids] == '0') && (ids < fracdigs); ids++);
						if (ids < fracdigs)
							sticky = 1;
					}
				}
			}
		} else {	/* Need more significant digits from fraction
				 * part. */
			idsbound = pm->ndigits - ids;
			if (ids == 0) {	/* No integer part - find first
					 * significant digit. */
				for (i = 0; fs[i] == '0'; i++);
				idsbound = i + idsbound + fraczeros;
				i += fraczeros;	/* Point i at first
						 * significant digit. */
			} else
				i = 0;
			if (idsbound > fracdigs)
				idsbound = fracdigs;
			pd->exponent = -idsbound;

			if (fraczeros < idsbound)	/* Compute number of
							 * leading zeros
							 * required. */
				lzbound = fraczeros;
			else
				lzbound = idsbound;
			if (lzbound > i) {
				int             ncopy = lzbound - i;
				(void) memset(&(pd->ds[ids]), '0', ncopy);
				ids += ncopy;
				i += ncopy;
			} {
				int             ncopy = idsbound - i;
				if (ncopy > 0) {
					(void) memcpy(&(pd->ds[ids]), &(fs[i - fraczeros]), ncopy);
					i += ncopy;
					ids += ncopy;
				}
			}
			i -= fraczeros;	/* Don't worry about leading zeros
					 * from now on, we're just rounding */
			if (i < fracsigs) {	/* Gather rounding info.  */
				if (i < 0)
					round = '0';
				else
					round = fs[i];
				i++;
				if (sticky == 0) {	/* Find out if remainder
							 * is exact. */
					if (i < 0)
						i = 0;
					for (; (fs[i] == '0') && (i < fracsigs); i++);
					if (i < fracsigs)
						sticky = 1;
				}
			} else {/* Fraction part is exact - add zero digits
				 * if required. */
				int             ncount;
				ncount = pm->ndigits - ids;
				if (ncount > 0) {
					(void) memset(&(pd->ds[ids]), '0', ncount);
					ids += ncount;
				}
			}
			pd->ndigits = ids;
		}
		__decimal_round(pm, pd, ps, round, sticky);
	} else {		/* Combine integer and fraction for F format. */
		if (pm->ndigits >= 0) {	/* Normal F format. */
			if ((intdigs + pm->ndigits) >= DECIMAL_STRING_LENGTH)
				goto overflow;
			{
				int             ncopy = intsigs;
				if (ncopy > 0) {
					/* Copy integer digits. */
					(void) memcpy(&(pd->ds[0]), &(is[0]), ncopy);
					ids = ncopy;
				}
			}
			{
				int             ncopy = intdigs - ids;
				if (ncopy > 0) {
					(void) memset(&(pd->ds[ids]), '0', ncopy);
					ids = intdigs;
				}
			}
			idsbound = fracdigs;
			if (idsbound > pm->ndigits)
				idsbound = pm->ndigits;
			if (fraczeros < idsbound)	/* Compute number of
							 * leading zeros
							 * required. */
				lzbound = fraczeros;
			else
				lzbound = idsbound;
			{
				int             ncopy = lzbound;
				i = 0;
				if (ncopy > 0) {
					(void) memset(&(pd->ds[ids]), '0', ncopy);
					i = ncopy;
					ids += ncopy;
				}
				ncopy = idsbound - i;
				if (ncopy > 0) {
					(void) memcpy(&(pd->ds[ids]), &(fs[i - fraczeros]), ncopy);
					i = idsbound;
					ids += ncopy;
				}
				ncopy = pm->ndigits - i;
				if (ncopy > 0) {
					(void) memset(&(pd->ds[ids]), '0', ncopy);
					i = pm->ndigits;
					ids += ncopy;
				}
			}
			/* Copy trailing zeros if necessary. */
			pd->ndigits = ids;
			pd->exponent = intdigs - ids;
			i -= fraczeros;	/* Don't worry about leading zeros
					 * from now on, we're just rounding */
			if (i < fracsigs) {	/* Gather rounding info.  */
				if (i < 0)
					round = '0';
				else
					round = fs[i];
				i++;
				if (sticky == 0) {	/* Find out if remainder
							 * is exact. */
					if (i < 0)
						i = 0;
					for (; (fs[i] == '0') && (i < fracsigs); i++);
					if (i < fracsigs)
						sticky = 1;
				}
			}
			__decimal_round(pm, pd, ps, round, sticky);
		} else {	/* Bizarre F format - round to left of point. */
			int             roundpos = -pm->ndigits;

			if (intdigs >= DECIMAL_STRING_LENGTH)
				goto overflow;
			if (roundpos >= DECIMAL_STRING_LENGTH)
				goto overflow;
			if (intdigs <= roundpos) {	/* Not enough integer
							 * digits. */
				if (intdigs == roundpos) {
					round = is[0];
					i = 1;
				} else {
					round = '0';
					i = 0;
				}
				for (; (is[i] == '0') && (i < intsigs); i++);
				/* Search for sticky bits. */
				if (i < intsigs)
					sticky = 1;
				pd->ndigits = 0;
			} else {/* Some integer digits do not get rounded
				 * away. */
				{
					int             ncopy = intsigs - roundpos;
					if (ncopy > 0) {
						/* Copy integer digits. */
						(void) memcpy(&(pd->ds[0]), &(is[0]), ncopy);
						ids = ncopy;
					}
				}
				{
					int             ncopy = intdigs - roundpos - ids;
					if (ncopy > 0) {
						(void) memset(&(pd->ds[ids]), '0', ncopy);
						ids += ncopy;
					}
				}
				pd->ndigits = ids;
				if (ids < intsigs) {	/* Inexact. */
					round = is[ids++];
					for (; (is[ids] == '0') && (ids < intsigs); ids++);
					/* Search for non-zero digits. */
					if (ids < intsigs)
						sticky = 1;
				}
			}
			if (fracnonzero)
				sticky = 1;
			__decimal_round(pm, pd, ps, round, sticky);
			{
				int             ncopy = roundpos;
				if (ncopy > 0) {
					/* Blank out rounded away digits. */
					(void) memset(&(pd->ds[pd->ndigits]), '0', ncopy);
					i = pd->ndigits + roundpos;
				}
			}
			pd->exponent = 0;
			pd->ndigits = i;
			pd->ds[i] = 0;	/* Terminate string. */
		}
	}
}

void
_split_shorten(p)
	_big_float     *p;

/* Shorten significand length if possible */

{
	int             length = p->blength;
	int             zeros, i;

	for (zeros = 0; p->bsignificand[zeros] == 0; zeros++);	/* Count trailing zeros. */
	length -= zeros;
	if (length < 0)
		length = 0;
	if ((zeros > 0) && (length > 0)) {
		p->bexponent += 16 * zeros;
		for (i = 0; i < length; i++)
			p->bsignificand[i] = p->bsignificand[i + zeros];
	}
	p->blength = length;
}

void
_split_double_m1(px, pfint, pfrac)
	double_equivalence *px;
	_big_float     *pfint, *pfrac;

/* Exponent <= -1 so result is all fraction.		 */

{
	double_equivalence x = *px;

	pfint->blength = 0;
	pfint->bsignificand[0] = 0;
	pfrac->bsignificand[0] = x.f.significand2 & 0xffff;
	pfrac->bsignificand[1] = x.f.significand2 >> 16;
	pfrac->bsignificand[2] = x.f.msw.significand & 0xffff;
	pfrac->bsignificand[3] = x.f.msw.significand >> 16;
	pfrac->blength = 4;
	if (x.f.msw.exponent == 0) {	/* subnormal */
		pfrac->bexponent++;
		while (pfrac->bsignificand[pfrac->blength - 1] == 0)
			pfrac->blength--;	/* Normalize. */
	} else {		/* normal */
		pfrac->bsignificand[3] += 0x10;	/* Implicit bit */
	}
	_split_shorten(pfrac);
}

void
_split_double_3(px, pfint, pfrac)
	double_equivalence *px;
	_big_float     *pfint, *pfrac;

/* 0 <= exponent <= 3	 */

{
	double_equivalence x = *px;
	long unsigned   fmask, imask;

	fmask = (1 << (-pfint->bexponent - 48)) - 1;	/* Mask for fraction
							 * bits. */
	imask = 0xffff & ~fmask;
	pfint->bexponent += 48;
	pfint->blength = 1;
	pfint->bsignificand[0] = 0x10 | (imask & (x.f.msw.significand >> 16));
	/* _split_shorten(pfint); not needed */
	pfrac->blength = 4;
	pfrac->bsignificand[0] = 0xffff & x.f.significand2;
	pfrac->bsignificand[1] = x.f.significand2 >> 16;
	pfrac->bsignificand[2] = 0xffff & x.f.msw.significand;
	pfrac->bsignificand[3] = fmask & (x.f.msw.significand >> 16);
	_split_shorten(pfrac);
}

void
_split_double_19(px, pfint, pfrac)
	double_equivalence *px;
	_big_float     *pfint, *pfrac;

/* 4 <= exponent <= 19	 */

{
	double_equivalence x = *px;
	long unsigned   fmask, imask;

	fmask = (1 << (-pfint->bexponent - 32)) - 1;	/* Mask for fraction
							 * bits. */
	imask = 0xffff & ~fmask;
	pfint->bexponent += 32;
	pfint->blength = 2;
	pfint->bsignificand[0] = imask & x.f.msw.significand;
	pfint->bsignificand[1] = 0x10 + (x.f.msw.significand >> 16);
	_split_shorten(pfint);
	pfrac->blength = 3;
	pfrac->bsignificand[0] = 0xffff & x.f.significand2;
	pfrac->bsignificand[1] = x.f.significand2 >> 16;
	pfrac->bsignificand[2] = fmask & x.f.msw.significand;
	_split_shorten(pfrac);
}

void
_split_double_35(px, pfint, pfrac)
	double_equivalence *px;
	_big_float     *pfint, *pfrac;

/* 20 <= exponent <= 35	 */

{
	double_equivalence x = *px;
	long unsigned   fmask, imask;

	fmask = (1 << (-pfint->bexponent - 16)) - 1;	/* Mask for fraction
							 * bits. */
	imask = 0xffff & ~fmask;
	pfint->bexponent += 16;
	pfint->blength = 3;
	pfint->bsignificand[0] = imask & (x.f.significand2) >> 16;
	pfint->bsignificand[1] = x.f.msw.significand & 0xffff;
	pfint->bsignificand[2] = 0x10 + (x.f.msw.significand >> 16);
	_split_shorten(pfint);
	pfrac->blength = 2;
	pfrac->bsignificand[0] = 0xffff & x.f.significand2;
	pfrac->bsignificand[1] = fmask & (x.f.significand2 >> 16);
	_split_shorten(pfrac);
}

void
_split_double_51(px, pfint, pfrac)
	double_equivalence *px;
	_big_float     *pfint, *pfrac;

/* 36 <= exponent <= 51	 */

{
	double_equivalence x = *px;
	long unsigned   fmask, imask;

	fmask = (1 << (-pfint->bexponent)) - 1;	/* Mask for fraction bits. */
	imask = 0xffff & ~fmask;
	pfint->blength = 4;
	pfint->bsignificand[0] = x.f.significand2 & imask;
	pfint->bsignificand[1] = x.f.significand2 >> 16;
	pfint->bsignificand[2] = x.f.msw.significand & 0xffff;
	pfint->bsignificand[3] = 0x10 + (x.f.msw.significand >> 16);
	_split_shorten(pfint);
	pfrac->blength = 1;
	pfrac->bsignificand[0] = x.f.significand2 & fmask;
	_split_shorten(pfrac);
}

void
_split_double_52(px, pfint, pfrac)
	double_equivalence *px;
	_big_float     *pfint, *pfrac;

/* Exponent >= 52 so result is all integer.         */

{
	double_equivalence x = *px;

	pfrac->blength = 0;
	pfrac->bsignificand[0] = 0;
	pfint->blength = 4;
	pfint->bsignificand[0] = x.f.significand2 & 0xffff;
	pfint->bsignificand[1] = x.f.significand2 >> 16;
	pfint->bsignificand[2] = x.f.msw.significand & 0xffff;
	pfint->bsignificand[3] = 0x10 + (x.f.msw.significand >> 16);
	_split_shorten(pfint);
}

void
__unpack_double_two(px, pfint, pfrac)
	double         *px;
	_big_float     *pfint, *pfrac;

/*
 * Unpack normal or subnormal double into big_float integer and fraction
 * parts. One part may be zero, but not both.
 */

{
	double_equivalence x;
	int             exponent;
	_BIG_FLOAT_DIGIT sticky;

	x.x = *px;
	exponent = x.f.msw.exponent - DOUBLE_BIAS;
	pfint->bexponent = exponent - 52;
	pfrac->bexponent = exponent - 52;
	pfint->bsize = _BIG_FLOAT_SIZE;
	pfrac->bsize = _BIG_FLOAT_SIZE;
	if (exponent <= 19) {	/* exponent <= 19 */
		if (exponent < 0) {	/* exponent <= -1 */
			_split_double_m1(&x, pfint, pfrac);
		}
		/* exponent <= -1 */
		else {		/* exponent in [0,19] */
			if (exponent <= 3) {	/* exponent in [0,3] */
				_split_double_3(&x, pfint, pfrac);
			}
			/* exponent in [0,3] */
			else {	/* exponent in [4,19] */
				_split_double_19(&x, pfint, pfrac);
			}	/* exponent in [4,19] */
		}		/* exponent in [0,19] */
	}
	/* exponent <= 19 */
	else {			/* 20 <= exponent */
		if (exponent <= 35) {	/* exponent in [20,35] */
			_split_double_35(&x, pfint, pfrac);
		}
		/* exponent in [20,35] */
		else {		/* 36 <= exponent */
			if (exponent >= 52) {	/* 52 <= exponent */
				_split_double_52(&x, pfint, pfrac);
			}
			/* 52 <= exponent */
			else {	/* exponent in [36,51] */
				_split_double_51(&x, pfint, pfrac);
			}	/* exponent in [36,51] */
		}		/* 36 <= exponent */
	}			/* 20 <= exponent */
	if ((pfint->bexponent < 0) && (pfint->blength > 0)) {
		__right_shift_base_two(pfint, (short unsigned) -pfint->bexponent, &sticky);	/* right shift */
		pfint->bexponent = 0;	/* adjust exponent */
		if (pfint->bsignificand[pfint->blength - 1] == 0)
			pfint->blength--;	/* remove leading zero */
	}
}

void
__k_double_to_decimal(dd, pm, pd, ps)
	double          dd;
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;
{
	_big_float      bfint, bfrac;
#ifndef fsoft
	enum __fbe_type eround;
	int             esum = 0;
#define LS 24			/* length of s buffer */
	char            s[LS + 1];
	int             is, ns;
	double          dds;
	double_equivalence de;
	int             retry, idsexp;
	double          prod;
	int             unbiasedexponent;
	__ieee_flags_type fb;
	fp_exception_field_type ef;

#ifdef PRINTNOTQUICK
#define NOTQUICK(N) {  printf("\n notquick %c \n",N); goto notquick;}
#define NOTQUICKX(N) {  printf("\n notquick %c \n",N); goto notquickx;}
#else
#define NOTQUICK(N) {  goto notquick;}
#define NOTQUICKX(N) {  goto notquickx;}
#endif

	if (pm->rd != fp_nearest)
		NOTQUICK('A');
	if (pm->ndigits < 0)
		NOTQUICK('B');
	if (pm->df == fixed_form) {	/* F format */
		if ((pm->ndigits <= __TBL_TENS_MAX) && (pm->ndigits > 0)) {
			/* small positive */
			__get_ieee_flags(&fb);
			if (pm->ndigits > __TBL_TENS_EXACT) {
				dds = __dabs(dd) * __tbl_tens[pm->ndigits];
				esum += 2;
			} else {
				dds = __mul_set(__dabs(dd), __tbl_tens[pm->ndigits], &eround);
				esum += (int) eround;
			}
		}
		/* small positive */
		else if (pm->ndigits != 0) {	/* exponent magnitude too
						 * large. */
			NOTQUICK('C');
		} else {
			dds = __dabs(dd);
			__get_ieee_flags(&fb);
		}
		if (dds > 2147483647999999744.0)
			NOTQUICKX('E');
		dds = __arint_set_n(dds, esum, &eround);
		if (eround == __fbe_many)
			NOTQUICKX('F');
		if (dds == 0.0) {
			is = (pm->ndigits >= 0) ? pm->ndigits : -pm->ndigits;
			if (is == 0)
				is = 1;
			(void) memset(pd->ds, '0', is);
			eround++;
		} else {	/* normal non-zero case */
			__double_to_digits(dds, &s[LS - 1], &ns);
			while (s[LS - ns] == '0')
				ns--;
			if (ns < 1)
				ns = 1;
			if (ns < pm->ndigits) {
				(void) memset(pd->ds, '0', pm->ndigits - ns);
				is = pm->ndigits - ns;
			} else
				is = 0;
			(void) memcpy(&(pd->ds[is]), &(s[LS - ns]), ns);
			is += ns;
		}
		pd->ndigits = is;
		pd->exponent = -pm->ndigits;
		pd->ds[pd->ndigits] = 0;	/* Terminate string. */
	} else {		/* E format - harder */
		if (pm->ndigits < 1)
			NOTQUICK('G');
		if (pm->ndigits > 18)
			NOTQUICK('H');
		__get_ieee_flags(&fb);
		dds = __dabs(dd);
		de.x = dds;
		unbiasedexponent = (int) de.f.msw.exponent - 0x3ff;
		idsexp = pm->ndigits - 1 - ((unbiasedexponent * 1233 + (int) __tbl_baselg[de.f.msw.significand >> 13]) >> 12);	/* exponent * log10(2) */
		retry = 0;

tryidsexp:
		esum = 0;
		if ((0 < idsexp) && (idsexp <= __TBL_TENS_MAX)) {	/* Positive power. */
			if (idsexp > __TBL_TENS_EXACT) {
				prod = dds * __tbl_tens[idsexp];
				esum += 2;
			} else {
				prod = __mul_set(dds, __tbl_tens[idsexp], &eround);
				esum += (int) eround;
			}
		} else if ((0 < -idsexp) && (-idsexp <= __TBL_TENS_MAX)) {	/* Negative power to
										 * scale. */
			if (-idsexp > __TBL_TENS_EXACT) {
				prod = dds / __tbl_tens[-idsexp];
				esum += 2;
			} else {
				prod = __div_set(dds, __tbl_tens[-idsexp], &eround);
				esum += (int) eround;
			}
		} else if (idsexp == 0)
			prod = dds;
		else
			NOTQUICKX('K');
		prod = __arint_set_n(prod, esum, &eround);
		if (eround == __fbe_many)
			NOTQUICKX('L');
		if (__tbl_tens[pm->ndigits - 1] >= prod) {
			if (__tbl_tens[pm->ndigits - 1] > prod) {
			/*
			 * __tbl_tens[pm->ndigits - 1] > prod implies
			 * power of ten too small or other failure.
			 */
#ifdef DEBUG
				if (retry > 0)
					(void) printf("idsexp too small second try x %X %X nd %d idsexp %d prod %30.1f \n", dds, pm->ndigits, idsexp, prod);
				else
					(void) printf("idsexp too small first  try x %X %X nd %d idsexp %d prod %30.1f \n", dds, pm->ndigits, idsexp, prod);
#endif DEBUG
				if (retry > 0)
					NOTQUICKX('M');
				retry = 1;
				idsexp++;
				goto tryidsexp;
			}
			else {
				if (eround != __fbe_none) {
			/*
			 * __tbl_tens[pm->ndigits - 1] == prod inexactly
			 * may imply power of ten too small - bug 1096836
			 * as in "9995" rounding to "1000".
			 */
					if (retry > 0)
						NOTQUICKX('O');
				}
				retry = 1;
				idsexp++;
				goto tryidsexp;
			}
		} else if (prod >= __tbl_tens[pm->ndigits]) {	/* prod too big - wrong
								 * power of ten or other
								 * failure. */
#ifdef DEBUG
			if (retry > 0)
				(void) printf("idsexp too big second try x %X %X nd %d idsexp %d prod %30.1f \n", dds, pm->ndigits, idsexp, prod);
			else
				(void) printf("idsexp too big first  try x %X %X nd %d idsexp %d prod %30.1f \n", dds, pm->ndigits, idsexp, prod);
#endif
			if (retry > 0)
				NOTQUICKX('N');
			retry = 1;
			idsexp--;
			goto tryidsexp;
		}
#ifdef DEBUG
		if (retry == 0)
			(void) printf("idsexp ok  first try x %X %X nd %d idsexp %d prod %30.1f \n", dds, pm->ndigits, idsexp, prod);
#endif
		__double_to_digits(prod, &s[LS - 1], &ns);
		(void) memcpy(pd->ds, &(s[LS - pm->ndigits]), pm->ndigits);
		pd->ndigits = pm->ndigits;
		pd->exponent = -idsexp;
		pd->ds[pm->ndigits] = 0;	/* Terminate string. */
	}
	ef = (eround == __fbe_none) ? 0 : (1 << fp_inexact);
	*ps = ef;
	__set_ieee_flags(&fb);
	return;

notquickx:

	__set_ieee_flags(&fb);

notquick:			/* Can't do it the easy way. */
#endif				/* not defined fsoft */
	__unpack_double_two(&dd, &bfint, &bfrac);
	_unpacked_to_decimal_two(&bfint, &bfrac, pm, pd, ps);
}

void
double_to_decimal(px, pm, pd, ps)
	double         *px;
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;
{
	double_equivalence kluge;
	fp_exception_field_type ef;

	ef = 0;			/* Initialize *ps. */
	kluge.x = *px;
	pd->sign = kluge.f.msw.sign;
	pd->fpclass = __class_double(px);
	switch (pd->fpclass) {
	case fp_normal:
	case fp_subnormal:
		{
			__k_double_to_decimal(*px, pm, pd, &ef);
#ifndef fsoft
			if (ef != 0)
				__base_conversion_set_exception(ef);
#endif
		}
	}
	*ps = ef;
}

/*
 * Following should be in __quad_decim.c but has to be here to avoid problems
 * in static linking on SunOS 4.1
 */

#if !defined(lint) && defined(SCCSIDS)
static char     sccsid[] = "@(#)double_decim.c 1.3 93/12/09 SMI";
#endif

/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

/*
 * Conversion between from quadruple binary to decimal floating point
 */

/* #include "base_conversion.h"	 */

void
__unpack_quadruple_two(px, pfint, pfrac)
	quadruple      *px;
	_big_float     *pfint, *pfrac;
/*
 * Unpack normal or subnormal quad into big_float integer and fraction parts.
 * One part may be zero, but not both.
 */
{
	quadruple_equivalence x;
	int             exponent;
	_BIG_FLOAT_DIGIT sticky;

	pfint->bsize = _BIG_FLOAT_SIZE;
	pfrac->bsize = _BIG_FLOAT_SIZE;
	x.x = *px;
	exponent = x.f.msw.exponent - QUAD_BIAS;
	if (exponent <= -1) {	/* exponent <= -1 - all fraction, no integer */
		pfint->blength = 0;
		pfrac->bsignificand[0] = x.f.significand4 & 0xffff;
		pfrac->bsignificand[1] = x.f.significand4 >> 16;
		pfrac->bsignificand[2] = x.f.significand3 & 0xffff;
		pfrac->bsignificand[3] = x.f.significand3 >> 16;
		pfrac->bsignificand[4] = x.f.significand2 & 0xffff;
		pfrac->bsignificand[5] = x.f.significand2 >> 16;
		pfrac->bsignificand[6] = x.f.msw.significand;
		if (x.f.msw.exponent == 0) {	/* subnormal */
			pfrac->blength = 7;
			pfrac->bexponent = exponent - 111;
			while (pfrac->bsignificand[pfrac->blength - 1] == 0)
				pfrac->blength--;	/* remove leading zeros */
		} else {	/* normal */
			pfrac->blength = 8;
			pfrac->bexponent = exponent - 112;
			pfrac->bsignificand[7] = 1;
		}
		_split_shorten(pfrac);
	} else if (exponent >= 112) {	/* exponent >= 112 - all integer, no
					 * fraction */
		pfint->bexponent = exponent - 112;
		pfint->blength = 8;
		pfrac->blength = 0;
		pfint->bsignificand[0] = x.f.significand4 & 0xffff;
		pfint->bsignificand[1] = x.f.significand4 >> 16;
		pfint->bsignificand[2] = x.f.significand3 & 0xffff;
		pfint->bsignificand[3] = x.f.significand3 >> 16;
		pfint->bsignificand[4] = x.f.significand2 & 0xffff;
		pfint->bsignificand[5] = x.f.significand2 >> 16;
		pfint->bsignificand[6] = x.f.msw.significand;
		pfint->bsignificand[7] = 1;
		_split_shorten(pfint);
	} else {		/* 0 <= exponent <= 111 : 1.0 <= x < 2**112 :
				 * some integer, some fraction */
		_BIG_FLOAT_DIGIT u[8], fmask;
		int             i, midword = (111 - exponent) >> 4;	/* transition word
									 * between int and frac */

		pfint->bexponent = exponent - 112 + 16 * midword;
		pfrac->bexponent = exponent - 112;
		pfint->blength = 8 - midword;
		pfrac->blength = 1 + midword;
		u[0] = x.f.significand4 & 0xffff;
		u[1] = x.f.significand4 >> 16;
		u[2] = x.f.significand3 & 0xffff;
		u[3] = x.f.significand3 >> 16;
		u[4] = x.f.significand2 & 0xffff;
		u[5] = x.f.significand2 >> 16;
		u[6] = x.f.msw.significand;
		u[7] = 1;
		for (i = 0; i < midword; i++) {
			pfrac->bsignificand[i] = u[i];
		}
		fmask = 1 + ((111 - exponent) & 0xf);	/* 1..16 */
		fmask = (1 << fmask) - 1;	/* 1..2**16-1 */
		pfrac->bsignificand[midword] = u[midword] & fmask;
		pfint->bsignificand[0] = u[midword] & ~fmask;
		for (i = midword + 1; i < 8; i++) {
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
__k_quadruple_to_decimal(px, pm, pd, ps)
	quadruple      *px;
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;
{
	quadruple_equivalence kluge;
	_big_float      bfint, bfrac;
	fp_exception_field_type ef;

	ef = 0;			/* Initialize ef - no exceptions. */
#ifdef i386
#define QMSW 3
#else
#define QMSW 0
#endif
#ifdef __STDC__
	kluge.x = *px;
#else
	kluge.x.u[QMSW] = px->u[QMSW];
#endif
	pd->sign = kluge.f.msw.sign;
	pd->fpclass = __class_quadruple(px);
	switch (pd->fpclass) {
	case fp_normal:
	case fp_subnormal:
		__unpack_quadruple_two(px, &bfint, &bfrac);
		_unpacked_to_decimal_two(&bfint, &bfrac, pm, pd, &ef);
	}
	*ps = ef;
}

void
quadruple_to_decimal(px, pm, pd, ps)
	quadruple      *px;
	decimal_mode   *pm;
	decimal_record *pd;
	fp_exception_field_type *ps;
{
	quadruple_equivalence kluge;
	fp_exception_field_type ef;

	ef = 0;			/* Initialize ef - no exceptions. */
#ifdef i386
#define QMSW 3
#else
#define QMSW 0
#endif
#ifdef __STDC__
	kluge.x = *px;
#else
	kluge.x.u[QMSW] = px->u[QMSW];
#endif
	pd->sign = kluge.f.msw.sign;
	pd->fpclass = __class_quadruple(px);
	switch (pd->fpclass) {
	case fp_normal:
	case fp_subnormal:
		__k_quadruple_to_decimal(px, pm, pd, &ef);
#ifndef fsoft
		if (ef != 0)
			__base_conversion_set_exception(ef);
#endif
	}
	*ps = ef;
}

/*
 * Copyright (c) 1989, 1991, by Sun Microsystems, Inc.
 */

#ident  "@(#)__x_power.c	1.1	92/04/17 SMI"

#include "synonyms.h"
#include "base_conversion.h"
#include <stdio.h>
#include <malloc.h>
#include <memory.h>

void
__copy_big_float_digits(p1, p2, n)
	_BIG_FLOAT_DIGIT *p1, *p2;
	short unsigned  n;

{				/* Copies p1[n] = p2[n] */
	if (n > 0) {
		(void) memcpy((char *) p1, (char *) p2, (int) (sizeof(_BIG_FLOAT_DIGIT) * n));
		p1 += n;
		p2 += n;
	}
}

void
__free_big_float(p)
	_big_float     *p;

{
	/* Central routine to call free for base conversion.	 */

	char           *freearg = (char *) p;

	(void) free(freearg);
#ifdef DEBUG
	(void) printf(" free called with %X \n", freearg);
#endif
}

void
__base_conversion_abort(ern, bcastring)
	int             ern;
	char           *bcastring;

{
	char            pstring[160];

	errno = ern;
#ifdef DEBUG
	(void) sprintf(pstring, " libc base conversion file %s line %d: %s", __FILE__, __LINE__, bcastring);
	perror(pstring);
#endif
	abort();
}

void
__big_float_times_power(
#ifdef __STDC__
/* function to multiply a big_float times a positive power of two or ten. */
		       _big_float * pbf,	/* Operand x, to be replaced
						 * by the product x * mult **
						 * n. */
	int             mult,	/* if mult is two, x is base 10**4; if mult
				 * is ten, x is base 2**16 */
	int             n,
	int             precision,	/* Number of bits of precision
					 * ultimately required (mult=10) or
					 * number of digits of precision
					 * ultimately required (mult=2).
					 * Extra bits are allowed internally
					 * to permit correct rounding. */
	_big_float    **pnewbf 	/* Return result *pnewbf is set to: pbf if
				 * uneventful BIG_FLOAT_TIMES_TOOBIG  if n is
				 * bigger than the tables permit ;
				 * BIG_FLOAT_TIMES_NOMEM   if pbf->blength
				 * was insufficient to hold the product, and
				 * malloc failed to produce a new block ;
				 * &newbf                  if pbf->blength
				 * was insufficient to hold the product, and
				 * a new _big_float was allocated by malloc.
				 * newbf holds the product.  It's the
				 * caller's responsibility to free this space
				 * when no longer needed. */
)
#else
		                       pbf, mult, n, precision, pnewbf
)
	_big_float     *pbf;
	int             mult, n, precision;
	_big_float    **pnewbf;
#endif

{
	short unsigned  base;
	unsigned short  productsize, trailing_zeros_to_delete, needed_precision, *pp, *table[3], *start[3], tablepower[3];
	int             i, j, itlast;
	_big_float     *pbfold = pbf;

	if (mult == 2) {
		/* *pbf is in base 10**4 so multiply by a power of two */
		base = 10;
		needed_precision = 2 + precision / 4;	/* Precision is in base
							 * ten; counts hang over
							 * in leading and
							 * following. */
		table[0] = (unsigned short *)__tbl_2_small_digits;
		start[0] = (unsigned short *)__tbl_2_small_start;
		if (n < __TBL_2_SMALL_SIZE) {
			itlast = 0;
			tablepower[0] = n;
			goto compute_product_size;
		}
		table[1] = (unsigned short *)__tbl_2_big_digits;
		start[1] = (unsigned short *)__tbl_2_big_start;
		if (n < (__TBL_2_SMALL_SIZE * __TBL_2_BIG_SIZE)) {
			itlast = 1;
			tablepower[0] = n % __TBL_2_SMALL_SIZE;
			tablepower[1] = n / __TBL_2_SMALL_SIZE;
			goto compute_product_size;
		}
		table[2] = (unsigned short *)__tbl_2_huge_digits;
		start[2] = (unsigned short *)__tbl_2_huge_start;
		if (n < (__TBL_2_SMALL_SIZE * __TBL_2_BIG_SIZE * __TBL_2_HUGE_SIZE)) {
			itlast = 2;
			tablepower[0] = n % __TBL_2_SMALL_SIZE;
			n /= __TBL_2_SMALL_SIZE;
			tablepower[1] = n % __TBL_2_BIG_SIZE;
			tablepower[2] = n / __TBL_2_BIG_SIZE;
			goto compute_product_size;
		}
	} else {
		/* *pbf is in base 2**16 so multiply by a power of ten */
		base = 2;
		pbf->bexponent += n;	/* Convert *5**n to *10**n */
		needed_precision = 2 + precision / 16;
		table[0] = (unsigned short *)__tbl_10_small_digits;
		start[0] = (unsigned short *)__tbl_10_small_start;
		if (n < __TBL_10_SMALL_SIZE) {
			itlast = 0;
			tablepower[0] = n;
			goto compute_product_size;
		}
		table[1] = (unsigned short *)__tbl_10_big_digits;
		start[1] = (unsigned short *)__tbl_10_big_start;
		if (n < (__TBL_10_SMALL_SIZE * __TBL_10_BIG_SIZE)) {
			itlast = 1;
			tablepower[0] = n % __TBL_10_SMALL_SIZE;
			tablepower[1] = n / __TBL_10_SMALL_SIZE;
			goto compute_product_size;
		}
		table[2] = (unsigned short *)__tbl_10_huge_digits;
		start[2] = (unsigned short *)__tbl_10_huge_start;
		if (n < (__TBL_10_SMALL_SIZE * __TBL_10_BIG_SIZE * __TBL_10_HUGE_SIZE)) {
			itlast = 2;
			tablepower[0] = n % __TBL_10_SMALL_SIZE;
			n /= __TBL_10_SMALL_SIZE;
			tablepower[1] = n % __TBL_10_BIG_SIZE;
			tablepower[2] = n / __TBL_10_BIG_SIZE;
			goto compute_product_size;
		}
	}

	/*
	 * The tables aren't big enough to accomodate mult**n, but it doesn't
	 * matter since the result would undoubtedly overflow even binary
	 * quadruple precision format.  Return an error code.
	 */
#ifdef DEBUG
	(void) printf("\n _times_power failed: mult=%d n=%d precision=%d \n",
		mult, n, precision);
#endif
	*pnewbf = BIG_FLOAT_TIMES_TOOBIG;
	goto ret;

compute_product_size:
	productsize = pbf->blength;
	for (i = 0; i <= itlast; i++)
		productsize += (start[i])[tablepower[i] + 1] - (start[i])[tablepower[i]];

	if (productsize < needed_precision)
		needed_precision = productsize;

	if (productsize <= pbf->bsize) {
		*pnewbf = pbf;	/* Work with *pnewbf from now on. */
	} else {		/* Need more significance than *pbf can hold. */
		char           *mallocresult;
		int             mallocarg;

		mallocarg = sizeof(_big_float) + sizeof(_BIG_FLOAT_DIGIT) * (productsize - _BIG_FLOAT_SIZE);
		mallocresult = malloc(mallocarg);
#ifdef DEBUG
		(void) printf(" malloc arg %X result %X \n", mallocarg, (int) mallocresult);
#endif
		if (mallocresult == (char *) 0) {	/* Not enough memory
							 * left, bail out. */
			*pnewbf = BIG_FLOAT_TIMES_NOMEM;
			goto ret;
		}
		*pnewbf = (_big_float *) mallocresult;
		__copy_big_float_digits((*pnewbf)->bsignificand, pbf->bsignificand, pbf->blength);
		(*pnewbf)->blength = pbf->blength;
		(*pnewbf)->bexponent = pbf->bexponent;
		pbf = *pnewbf;
		pbf->bsize = productsize;
	}

	/* pbf now points to the input and the output big_floats.	 */

	for (i = 0; i <= itlast; i++)
		if (tablepower[i] != 0) {	/* Step through each of the
						 * tables. */
			unsigned        lengthx, lengthp;

			/* Powers of 10**4 have leading zeros in base 2**16. */
			lengthp = (start[i])[tablepower[i] + 1] - (start[i])[tablepower[i]];
			lengthx = pbf->blength;


#ifdef DEBUG
			{
				int             id;

				(void) printf(" step %d x operand length %d \n", i, lengthx);
				__display_big_float(pbf, base);
				(void) printf(" step %d p operand length %d power %d \n", i, lengthp, tablepower[i]);
				for (id = 0; id < lengthp; id++) {
					(void) printf("+ %d * ", (table[i])[id + (start[i])[tablepower[i]]]);
					if (base == 2)
						(void) printf("2**%d", 16 * (id));
					if (base == 10)
						(void) printf("10**%d", 4 * (id));
					if ((id % 4) == 3)
						(void) printf("\n");
				}
				(void) printf("\n");
			}
#endif

			/*
			 * if (base == 2) { pbf->bexponent += 16 *
			 * (lz[i])[tablepower[i]]; }
			 */
			if (lengthp == 1) {	/* Special case - multiply by
						 * <= 10**4 or 2**13 */
				if (base == 10) {
					__multiply_base_ten_by_two(pbf, tablepower[i]);
				} else {
					__multiply_base_two(pbf, (_BIG_FLOAT_DIGIT) ((table[i])[tablepower[i]]), (unsigned long) 0);
				}
#ifdef DEBUG
				assert(pbf->blength <= pbf->bsize);
#endif
			} else if (lengthx == 1) {	/* Special case of short
							 * multiplicand. */
				_BIG_FLOAT_DIGIT multiplier = pbf->bsignificand[0];

				__copy_big_float_digits(pbf->bsignificand, (unsigned short *) &((table[i])[(start[i])[tablepower[i]]]), lengthp);
				pbf->blength = lengthp;
				if (base == 10)
					__multiply_base_ten(pbf, multiplier);
				else
					__multiply_base_two(pbf, multiplier, (unsigned long) 0);
#ifdef DEBUG
				assert(pbf->blength <= pbf->bsize);
#endif
			} else {/* General case. */
				short unsigned  canquit;
				short unsigned  excess;
				int             excess_check;

				/*
				 * The result will be accumulated in *pbf
				 * from most significant to least
				 * significant.
				 */


				/* Generate criterion for early termination.	 */
				if (i == itlast)
					canquit = ((base == 2) ? 65535 : 9999) - ((lengthx < lengthp) ? lengthx : lengthp);

				pbf->bsignificand[lengthx + lengthp - 1] = 0;	/* Only gets filled by
										 * carries. */
				for (j = lengthx + lengthp - 2; j >= 0; j--) {
					int             istart = j - lengthp + 1, istop = lengthx - 1;
					_BIG_FLOAT_DIGIT product[3];

					pp = (unsigned short *) &((table[i])[(start[i])[tablepower[i]]]);
					if (j < istop)
						istop = j;
					if (0 > istart)
						istart = 0;

					if (base == 2) {
						__multiply_base_two_vector((short unsigned) (istop - istart + 1), &(pbf->bsignificand[istart]), &(pp[j - istop]), product);
						if (product[2] != 0)
							__carry_propagate_two((unsigned long) product[2], &(pbf->bsignificand[j + 2]));
						if (product[1] != 0)
							__carry_propagate_two((unsigned long) product[1], &(pbf->bsignificand[j + 1]));
					} else {
						__multiply_base_ten_vector((short unsigned) (istop - istart + 1), &(pbf->bsignificand[istart]), &(pp[j - istop]), product);
						if (product[2] != 0)
							__carry_propagate_ten((unsigned long) product[2], &(pbf->bsignificand[j + 2]));
						if (product[1] != 0)
							__carry_propagate_ten((unsigned long) product[1], &(pbf->bsignificand[j + 1]));
					}
					pbf->bsignificand[j] = product[0];
					if (i < itlast)
						goto pastdiscard;
					excess_check = lengthx + lengthp;
					if (pbf->bsignificand[excess_check - 1] == 0)
						excess_check--;
					excess_check -= (int) needed_precision + 4;
					if ((j <= excess_check) && (pbf->bsignificand[j + 1] <= canquit)
					    && ((pbf->bsignificand[j + 1] | pbf->bsignificand[j]) != 0)) {
						/*
						 * On the last
						 * multiplication, it's not
						 * necessary to develop the
						 * entire product, if further
						 * digits can't possibly
						 * affect significant digits,
						 * unless there's a chance
						 * the product might be
						 * exact!
						 */
						/*
						 * Note that the product
						 * might be exact if the j
						 * and j+1 terms are zero; if
						 * they are non-zero, then it
						 * won't be after they're
						 * discarded.
						 */

						excess = j + 2;	/* Can discard j+1, j,
								 * ... 0 */
#ifdef DEBUG
						(void) printf(" decided to quit early at j %d since s[j+1] is %d <= %d \n", j, pbf->bsignificand[j + 1], canquit);
						(void) printf(" s[j+2..j] are %d %d %d \n", pbf->bsignificand[j + 2], pbf->bsignificand[j + 1], pbf->bsignificand[j]);
						(void) printf(" requested precision %d needed_precision %d big digits out of %d \n", precision, needed_precision, lengthx + lengthp);
#endif
						pbf->bsignificand[excess] |= 1;	/* Sticky bit on. */
#ifdef DEBUG
						(void) printf(" discard %d digits - last gets %d \n", excess, pbf->bsignificand[excess]);
#endif
						trailing_zeros_to_delete = excess;
						goto donegeneral;
						/*
						 * END possible opportunity
						 * to quit early
						 */
					}
			pastdiscard:	;
#ifdef DEBUG
					/*
					 * else { (void) printf(" early termination
					 * rejected at j %d since s[j+1] =
					 * %d, canquit = %d \n", j,
					 * pbf->bsignificand[j + 1],
					 * canquit); (void) printf(" s[j+2..j] are
					 * %d %d %d \n", pbf->bsignificand[j
					 * + 2], pbf->bsignificand[j + 1],
					 * pbf->bsignificand[j]); (void) printf("
					 * requested precision %d
					 * needed_precision %d big digits out
					 * of %d \n", precision,
					 * needed_precision, lengthprod); }
					 */
#endif
				}
				trailing_zeros_to_delete = 0;
		donegeneral:
				pbf->blength = lengthx + lengthp;
				if (pbf->bsignificand[pbf->blength - 1] == 0)
					pbf->blength--;
				for (; pbf->bsignificand[trailing_zeros_to_delete] == 0; trailing_zeros_to_delete++);
				/*
				 * Look for additional trailing zeros to
				 * delete.
				 */
				
                                if (base == 10) {
                                        int             deletelimit = (1 - ((pbf->bexponent + 3) / 4));
					if (deletelimit < 0)
						deletelimit = 0;
                                        if ((int)trailing_zeros_to_delete > deletelimit) {
                                                /*
                                                 * If too many trailing zeros
                                                 * are deleted, we will
                                                 * violate the assertion that
                                                 * pbf->bexponent is in
                                                 * [-3,+4]
                                                 */
#ifdef DEBUG
                                                printf("\n __x_power trailing zeros delete count lowered from %d to %d \n",
                                                       trailing_zeros_to_delete, deletelimit);
#endif
						/*
						 * Must set the uninitialized
						 * cells to zero; otherwise they
						 * may contain random values and
						 * four_digits_quick may SEGV.
						 */
						for (j = deletelimit; j < (int)
						    trailing_zeros_to_delete -
						    2; j++)
							pbf->bsignificand[j] = 0;
                                                trailing_zeros_to_delete = deletelimit;
                                        }
                                }

				if (trailing_zeros_to_delete > 0) {
#ifdef DEBUG
					(void) printf(" %d trailing zeros deleted \n", trailing_zeros_to_delete);
#endif
					__copy_big_float_digits(pbf->bsignificand, &(pbf->bsignificand[trailing_zeros_to_delete]), pbf->blength - trailing_zeros_to_delete);
					pbf->blength -= trailing_zeros_to_delete;
					pbf->bexponent += ((base == 2) ? 16 : 4) * trailing_zeros_to_delete;
					/* END delete trailing zeros */
				}
				/*
				 * END general multiplication case, usually
				 * the last step
				 */
			}
			/* END for i=0,itlast ; if (tablepower[i] != 0) */
		}
	if ((pbfold != pbf) && (pbf->blength <= pbfold->bsize)) {	/* Don't need that huge
									 * buffer after all! */
#ifdef DEBUG
		(void) printf(" free called from times_power because final length %d <= %d original size \n", pbf->blength, pbfold->bsize);
#endif

		/* Copy product to original buffer. */
		pbfold->blength = pbf->blength;
		pbfold->bexponent = pbf->bexponent;
		__copy_big_float_digits(pbfold->bsignificand, pbf->bsignificand, pbf->blength);
		__free_big_float(*pnewbf);	/* Free new buffer. */
		*pnewbf = pbfold;	/* New buffer pointer now agrees with
					 * original. */
	}
ret:
	return;
}

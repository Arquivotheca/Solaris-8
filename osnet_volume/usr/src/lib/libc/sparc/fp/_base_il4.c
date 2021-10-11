/*
 * Copyright (C) 1989, 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)_base_il4.c	1.4	96/12/06 SMI"
/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include "base_conversion.h"
#include <math.h>

/* The following should be coded as inline expansion templates.	 */

/*
 * Fundamental utilities that multiply two shorts into a unsigned int, add
 * carry, compute quotient and remainder in underlying base, and return
 * quo<<16 | rem as  a unsigned int.
 */

/*
 * C compilers tend to generate bad code - forcing full unsigned int by
 * unsigned int multiplies when what is really wanted is the unsigned int
 * product of half-int operands. Similarly the quotient and remainder are
 * all half-int. So these functions should really be implemented by inline
 * expansion templates.
 */

unsigned short
__quorem(unsigned int x, unsigned short y, unsigned short *pr)

/* __quorem gets x/y ; *pr gets x%y		 */

{
	*pr = (unsigned short) (x % y);
	return (unsigned short) (x / y);
}

unsigned short
__quorem10000(unsigned int x, unsigned short *pr)

/* __quorem gets x/10000 ; *pr gets x%10000		 */

{
	*pr = (unsigned short) (x % 10000);
	return (unsigned short) (x / 10000);
}

unsigned int
__longquorem10000(unsigned int d,  unsigned short *pr)
{
	*pr = d % 10000;
	return (d / 10000);
}

/* p = x + c ; return (p/10000 << 16 | * p%10000) */
unsigned int
__carry_in_b10000(_BIG_FLOAT_DIGIT x, unsigned int c)
{
	unsigned int   p = x + c;
	unsigned short  r;

	QUOREM10000(p, r);
	return ((p << 16) | r);
}

void
__carry_propagate_ten(unsigned int carry, _BIG_FLOAT_DIGIT *psignificand)
{
	/*
	 * Propagate carries in a base-10**4 significand.
	 */

	int	j;
	unsigned int   p;

	j = 0;
	while (carry != 0) {
		p = __carry_in_b10000(psignificand[j], carry);
		psignificand[j++] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
}

#ifdef	BASE_IL4_ALL_IN_C
void
__multiply_base_two_vector(unsighed short n, _BIG_FLOAT_DIGIT *px,
		unsigned short *py, _BIG_FLOAT_DIGIT *product[3])

{
	/*
	 * Given xi and yi, base 2**16 vectors of length n, computes dot
	 * product
	 *
	 * sum (i=0,n-1) of x[i]*y[n-1-i]
	 *
	 * Product may fill as many as three short-unsigned buckets. Product[0]
	 * is least significant, product[2] most.
	 */

	unsigned int   acc, p;
	unsigned short  carry;
	int	i;

	acc = 0;
	carry = 0;
	for (i = 0; i < (int)n; i++) {
		p = __umac(px[i], py[n - 1 - i], acc);
		if (p < acc)
			carry++;
		acc = p;
	}
	product[0] = (_BIG_FLOAT_DIGIT) (acc & 0xffff);
	product[1] = (_BIG_FLOAT_DIGIT) (acc >> 16);
	product[2] = (_BIG_FLOAT_DIGIT) (carry);
}
#endif	/* defined(BASE_IL4_ALL_IN_C) */

void
__multiply_base_ten_vector(unsigned short n,  _BIG_FLOAT_DIGIT *px,
		    unsigned short *py,  _BIG_FLOAT_DIGIT product[3])

{
	/*
	 * Given xi and yi, base 10**4 vectors of length n, computes dot
	 * product
	 *
	 * sum (i=0,n-1) of x[i]*y[n-1-i]
	 *
	 * Product may fill as many as three short-unsigned buckets. Product[0]
	 * is least significant, product[2] most.
	 */

#define	ABASE	3000000000	/* Base of accumulator. */

	unsigned int   acc;
	unsigned short  carry;
	int	i;

	acc = 0;
	carry = 0;
	for (i = 0; i < (int)n; i++) {
		acc = __umac(px[i], py[n - 1 - i], acc);
		if (acc >= (unsigned int) ABASE) {
			carry++;
			acc -= ABASE;
		}
	}
	/*
	 * NOTE: because acc <= ABASE-1, acc/10000 <= 299999 which would
	 * overflow a short unsigned
	 */
	LONGQUOREM10000(acc, product[0]);
	QUOREM10000(acc, product[1]);
	product[2] = (_BIG_FLOAT_DIGIT) (acc + ((unsigned int)ABASE
			    / 100000000) * carry);
}

#ifdef BASE_IL4_ALL_IN_C

/*
 *  Returns IEEE mode/status and sets up
 * standard environment for base conversion.
 */
void
__get_ieee_flags(__ieee_flags_type *b)
{
	/*
	 * Inline template should save old mode/status in b and reset
	 * defaults:
	 *	round to nearest extended
	 *	rounding to double precision
	 *	no traps enabled (I didn't bother in this C code)
	 */
	char	instring[20], outstring[20];
	char   	*in = instring, *out = outstring;

	b->mode = ieee_flags("get", "direction", in, &out);
	b->status = ieee_flags("get", "precision", in, &out);
	(void) ieee_flags("set", "direction", "nearest", &out);
	(void) ieee_flags("set", "precision", "double", &out);
}

void
__set_ieee_flags(__ieee_flags_type *b)
/* Restores previous IEEE mode/status */
{
	/*
	 * Inline template should restore old mode/status from b
	 */
	char	outstring[20];
	char	*out = outstring;

	switch ((enum fp_direction_type) b->mode) {
	case fp_nearest:
		ieee_flags("set", "direction", "nearest", &out);
		break;
	case fp_tozero:
		ieee_flags("set", "direction", "tozero", &out);
		break;
	case fp_negative:
		ieee_flags("set", "direction", "negative", &out);
		break;
	case fp_positive:
		ieee_flags("set", "direction", "positive", &out);
		break;
	}
	switch ((enum fp_precision_type) b->status) {
	case fp_single:
		ieee_flags("set", "precision", "single", &out);
		break;
	case fp_double:
		ieee_flags("set", "precision", "double", &out);
		break;
	default:
		ieee_flags("set", "precision", "extended", &out);
		break;
	}
}
#endif	/* defined(BASE_IL4_ALL_IN_C) */

unsigned int
___mul_65536_n(unsigned int carry, _BIG_FLOAT_DIGIT *ps, int n)
{
	int 	j;

	for (j = n; j > 0; j--) {
		carry += ((unsigned int) (*ps)) << 16;
		QUOREM10000(carry, *ps);
		ps++;
	}
	return (carry);
}

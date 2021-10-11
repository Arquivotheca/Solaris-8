#ifndef lint
static char     sccsid[] = "@(#)_base_il4.c	1.2	96/08/26 SMI";
#endif

/*
 * Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#include "synonyms.h"
#include "base_conversion.h"

/* The following should be coded as inline expansion templates.	 */

/*
 * Fundamental utilities that multiply two shorts into a unsigned long, add
 * carry, compute quotient and remainder in underlying base, and return
 * quo<<16 | rem as  a unsigned long.
 */

/*
 * C compilers tend to generate bad code - forcing full unsigned long by
 * unsigned long multiplies when what is really wanted is the unsigned long
 * product of half-long operands. Similarly the quotient and remainder are
 * all half-long. So these functions should really be implemented by inline
 * expansion templates.
 */

unsigned short
__quorem(x, y, pr)
	unsigned long   x;
	unsigned short  y;
	unsigned short *pr;

/* __quorem gets x/y ; *pr gets x%y		 */

{
	*pr = (short unsigned) (x % y);
	return (short unsigned) (x / y);
}

unsigned short
__quorem10000(x, pr)
	unsigned long   x;
	unsigned short *pr;

/* __quorem gets x/10000 ; *pr gets x%10000		 */

{
	*pr = (short unsigned) (x % 10000);
	return (short unsigned) (x / 10000);
}

unsigned long
__longquorem10000(d, pr)
	unsigned long   d;
	unsigned short  *pr;
{
	*pr = d % 10000;
	return d / 10000;
}

unsigned long
__carry_in_b10000(x, c)		/* p = x + c ; return (p/10000 << 16 |
				 * p%10000) */
	_BIG_FLOAT_DIGIT x;
	long unsigned   c;
{
	unsigned long   p = x + c;
	unsigned short  r;

	QUOREM10000(p, r);
	return (p << 16) | r;
}

void
__carry_propagate_ten(carry, psignificand)
	unsigned long   carry;
	_BIG_FLOAT_DIGIT *psignificand;
{
	/*
	 * Propagate carries in a base-10**4 significand.
	 */

	int             j;
	unsigned long   p;

	j = 0;
	while (carry != 0) {
		p = __carry_in_b10000(psignificand[j], carry);
		psignificand[j++] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
}

void
__multiply_base_two_vector(n, px, py, product)
	short unsigned  n, *py;
	_BIG_FLOAT_DIGIT *px, product[3];

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

	unsigned long   acc, p;
	short unsigned  carry;
	int             i;

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

void
__multiply_base_ten_vector(n, px, py, product)
	short unsigned  n, *py;
	_BIG_FLOAT_DIGIT *px, product[3];

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

#define ABASE	3000000000	/* Base of accumulator. */

	unsigned long   acc;
	short unsigned  carry;
	int             i;

	acc = 0;
	carry = 0;
	for (i = 0; i < (int)n; i++) {
		acc = __umac(px[i], py[n - 1 - i], acc);
		if (acc >= (unsigned long) ABASE) {
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
	product[2] = (_BIG_FLOAT_DIGIT) (acc + ((unsigned long)ABASE / 100000000) * carry);
}

void
__get_ieee_flags(b)		/* Returns IEEE mode/status and sets up
				 * standard environment for base conversion. */
	__ieee_flags_type *b;
{
	/* 1202391 remove ieee_flags.c from i386 libc
	 */
	extern void _getcw(), _putcw();
	unsigned short cw;
	/* This assumes 1251222 is fixed in such a way that _getcw() in
	 * lib/libc/i386/fp/fpcw.s implements _getcw() as
	 *	extern void _getcw(struct _cw87 *cwp);
	 */
	_getcw(&cw);
	b->mode = (int) cw;
	/*
	 * set CW to...
	 * RC (bits 10:11)	0 == round to nearest even
	 * PC (bits 8:9)	2 == round to double
	 * EM (bits 0:5)	0x3f == all exception trapping masked off
	 */
	cw = (cw & ~0xf3f) | 0x23f;
	_putcw(cw);

	/* Old Code -- Saved for Reference
	 *
	 * Inline template should save old mode/status in b and reset
	 * defaults: 
 	 *	round to nearest extended 
	 *	rounding to double precision
	 *	no traps enabled (I didn't bother in this C code)
	 *
	 * char            instring[20], outstring[20];
	 * char           *in = instring, *out = outstring;
	 *
	 * b->mode = ieee_flags("get", "direction", in, &out);
	 * b->status = ieee_flags("get", "precision", in, &out);
	 * (void) ieee_flags("set", "direction", "nearest", &out);
	 * (void) ieee_flags("set", "precision", "double", &out);
	 */
}

void
__set_ieee_flags(b)		/* Restores previous IEEE mode/status */
	__ieee_flags_type *b;
{
	/*
	 * fix for 1202391  remove ieee_flags.c from i386 libc
	 */
	extern void _putcw();
	_putcw(b->mode);

	/*
	 * Inline template should restore old mode/status from b
	 *
	 * char            outstring[20];
	 * char           *out = outstring;
	 *
	 * switch ((enum fp_direction_type) b->mode) {
	 * case fp_nearest:
 	 * 		ieee_flags("set", "direction", "nearest", &out);
 	 *		break;
	 * case fp_tozero:
 	 *		ieee_flags("set", "direction", "tozero", &out);
	 *		break;
	 * case fp_negative:
	 *	ieee_flags("set", "direction", "negative", &out);
	 *	break;
	 * case fp_positive:
	 *	ieee_flags("set", "direction", "positive", &out);
	 *	break;
	 * }
	 * switch ((enum fp_precision_type) b->status) {
	 * case fp_single:
	 *	ieee_flags("set", "precision", "single", &out);
	 *	break;
	 * case fp_double:
	 *	ieee_flags("set", "precision", "double", &out);
 	 *	break;
	 * default:
	 *	ieee_flags("set", "precision", "extended", &out);
	 *	break;
	 * }
	 */
}

unsigned long
___mul_65536_n(carry, ps, n)
        unsigned long   carry;
        _BIG_FLOAT_DIGIT *ps;
        int             n;
{
        int             j;
 
        for (j = n; j > 0; j--) {
                carry += ((unsigned long) (*ps)) << 16;
                QUOREM10000(carry, *ps);
                ps++;
        }
        return carry;
}


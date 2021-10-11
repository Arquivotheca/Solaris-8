#ifndef lint
static char     sccsid[] = "@(#)_base_il.c	1.2	93/12/23 SMI";
#endif

/*
 * Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#include "synonyms.h"
#include "base_conversion.h"
#include <math.h>

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

unsigned long
__umac(x, y, c)			/* p = x * y + c ; return p */
	_BIG_FLOAT_DIGIT x, y;
	unsigned long   c;
{
	return x * (unsigned long) y + c;
}

void
__carry_propagate_two(carry, psignificand)
	unsigned long   carry;
	_BIG_FLOAT_DIGIT *psignificand;
{
	/*
	 * Propagate carries in a base-2**16 significand.
	 */

	long unsigned   p;
	int             j;

	j = 0;
	while (carry != 0) {
		p = __carry_in_b65536(psignificand[j], carry);
		psignificand[j++] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
}

void
__four_digits_quick(u, s)
	short unsigned  u;
	char           *s;

/* Convert u to four digits at *s        which may not be aligned. */

{
	register int    carry;
	const char	*pt;

	pt = __four_digits_quick_table + ((u >> 1) & ~3);
	carry = (u & 7) + pt[3];
	if (carry <= '9') {
		s[3] = carry;
		goto move3;
	}
	s[3] = carry - 10;
	carry = pt[2];
	if (carry <= '8') {
		s[2] = carry + 1;
		goto move2;
	}
	s[2] = carry - 9;
	carry = pt[1];
	if (carry <= '8') {
		s[1] = carry + 1;
		goto move1;
	}
	s[1] = carry - 9;
	s[0] = pt[0] + 1;
	return;
move3:
	s[2] = pt[2];
move2:
	s[1] = pt[1];
move1:
	s[0] = pt[0];
	return;
}

int
__trailing_zeros(d)
        double          d;

/* Computes number of trailing zeros of a double.        */

{
        char           *pc, *pc0;

#if i386
        pc0 = (char *) &d;
        pc = pc0;
        while (*pc == 0)
                pc++;
        return pc - pc0;
#else
        pc0 = ((char *) &d) + 7;
        pc = pc0;
        while (*pc == 0)
                pc--;
        return pc0 - pc;
#endif
}
 
double
__mul_set(x, y, pe)
	double          x, y;
	enum __fbe_type *pe;

/* Multiplies two normal or subnormal doubles, returns result and exceptions. */
/*
 * Machine-coded versions will judge inexactness from hardware rather than
 * estimating from trailing zeros.
 */

{

	if ((__trailing_zeros(x) + __trailing_zeros(y)) >= 7) {	/* Result has <= 50
								 * significant bits,
								 * will be exact in any
								 * arithmetic. */
		*pe = __fbe_none;
	} else {		/* Result may not be exact. */
		*pe = __fbe_one;
	}
	return x * y;
}

double
__div_set(x, y, pe)
	double          x, y;
	enum __fbe_type *pe;

/*
 * Divides two normal or subnormal doubles x/y, returns result and
 * exceptions.
 */
/*
 * Machine-coded versions will judge inexactness from hardware rather than
 * estimating from trailing zeros.
 */

{
	double          z;

	z = x / y;

	if (((__trailing_zeros(z) + __trailing_zeros(y)) >= 7)
	    && ((z * y) == x)) {
		/*
		 * Residual has <= 50 significant bits, will be exact in any
		 * arithmetic.
		 */
		*pe = __fbe_none;
	} else {		/* Residual may not be inexact, hence result
				 * may not be exact. */
		*pe = __fbe_one;
	}
	return z;
}

double
__add_set(x, y, pe)
	double          x, y;
	enum __fbe_type *pe;

/*
 * Adds two normal or subnormal doubles x+y, returns result and exceptions. x
 * should have larger magnitude.
 */
/*
 * Machine-coded versions will judge inexactness from hardware rather than
 * estimating from trailing zeros.
 */

{
	double          z;

	z = x + y;

	if ((z - x) == y)
		*pe = __fbe_none;
	else			/* Residual may not be inexact, hence result
				 * may not be exact. */
		*pe = __fbe_one;

	return z;
}

double
__abs_ulp(x)
	double          x;
{				/* Returns one ulp of abs(x). */
	double_equivalence kx;

	kx.x = x;
	kx.f.significand2 ^= 1;	/* Flip lsb. */
	kx.x -= x;
	kx.f.msw.sign = 0;
	return kx.x;
}

double
__arint(x)
	double          x;
{
#define BIGINT 4.503599627370496e15	/* 2**52 - all doubles >= this are
					 * integral */

	/* Separate function for benefit of 68881 and possibly 80387. */

	double          rx;

#if i386
#define MSW 1			/* where to look for equivalenced integer
				 * with sign bit */
#else
#define MSW 0
#endif

	if (((int *) (&x))[MSW] >= 0) {
		rx = x + BIGINT;
		rx -= BIGINT;
	} else {
		rx = x - BIGINT;
		rx += BIGINT;
	}
	return rx;
}

double
__dabs(d)
	double          d;
{
	/* Expect inline expansion template for this fabs clone. */
	return (d < 0.0) ? -d : d;
}

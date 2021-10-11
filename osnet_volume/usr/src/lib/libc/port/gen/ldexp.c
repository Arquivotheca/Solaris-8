/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ldexp.c	1.13	96/12/12 SMI"	/* SVr4.0 2.12.1.4	*/

/*LINTLIBRARY*/
/*
 *	double ldexp (double value, int exp)
 *
 *	Ldexp returns value * 2**exp, if that result is in range.
 *	If underflow occurs, it returns zero.  If overflow occurs,
 *	it returns a value of appropriate sign and largest single-
 *	precision magnitude.  In case of underflow or overflow,
 *	the external int "errno" is set to ERANGE.  Note that errno is
 *	not modified if no error occurs, so if you intend to test it
 *	after you use ldexp, you had better set it to something
 *	other than ERANGE first (zero is a reasonable value to use).
 */

#include "synonyms.h"
#include "shlib.h"
#include <sys/types.h>
#include <values.h>
#include <math.h>
#include <nan.h>
#include <errno.h>

/* Largest signed long int power of 2 */
#define	MAXSHIFT	(int)(BITSPERBYTE * sizeof (int) - 2)

double
ldexp(double value, int exp)
{
	double Y;
	int old_exp;

	if (exp == 0 || value == 0.0) /* nothing to do for zero */
		return (value);

	if ((Y = frexp(value, &old_exp)), IsNANorINF(Y)) {
		errno = EDOM;
		return (Y);

	}
	if (exp > 0) {
		if (exp + old_exp > MAXBEXP) { /* overflow */
			errno = ERANGE;
			if (_lib_version == c_issue_4)
				return (value < 0 ? -HUGE : HUGE);
			else
				return (value < 0 ? -HUGE_VAL : HUGE_VAL);
		}
		for (; exp > MAXSHIFT; exp -= MAXSHIFT)
			value *= (unsigned)(1L << MAXSHIFT);
		return (value * (1L << exp));
	}
	if (exp + old_exp < (int)MINBEXP) { /* underflow */
		errno = ERANGE;
		return (0.0);
	}
	for (; exp < -MAXSHIFT; exp += MAXSHIFT)
		value *= 1.0/(unsigned)(1L << MAXSHIFT);
				/* mult faster than div */
	return (value / (1L << -exp));
}

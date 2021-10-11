/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)scalb.c	1.8	96/12/06 SMI"	/* SVr4 1.5	*/

/* LINTLIBRARY */

/*
 * SCALB(X, N)
 * return x * 2**N without computing 2**N - this is the standard
 * C library ldexp() routine except that signaling NANs generate
 * invalid op exception - errno = EDOM
 */

#pragma weak scalb = _scalb

#include "synonyms.h"
#include <values.h>
#include <math.h>
#include <errno.h>
#include "fpparts.h"
#include <limits.h>

double
scalb(double x, double n)
{
#if _IEEE
	if ((EXPONENT(x) == MAXEXP) && !QNANBIT(x) && (HIFRACTION(x) ||
	    LOFRACTION(x))) {
		errno = EDOM;
		return (x + 1.0); /* signaling NaN - raise exception */
	}
#endif

	if (isnan(x)) {
		errno = EDOM;
		return (x);
	}
	if (isnan(n)) {
		errno = EDOM;
		return (n);
	}

	if (x == 0.0)
		return (x);
	if ((n >= (double)INT_MAX) || (n <= (double)INT_MIN)) {
	/* over or underflow */
		errno = ERANGE;
		if (n < 0)   /* kcng suggested solution */
			return (0.0);  /* underflow */
		else {
			if (_lib_version == c_issue_4)
				return (x > 0.0 ? HUGE : -HUGE);
			else
				return (x > 0.0 ? HUGE_VAL : -HUGE_VAL);
		}
	}
	return (ldexp(x, (int)n));

}

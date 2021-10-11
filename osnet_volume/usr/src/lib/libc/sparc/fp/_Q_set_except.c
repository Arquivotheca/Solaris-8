/*
 * Copyright (C) 1988-1996, by Sun Microsystems, Inc. All Rights Reserved.
 */

#pragma ident	"@(#)_Q_set_except.c	1.1	97/09/04 SMI"

/* LINTLIBRARY */
#include "synonyms.h"
#include <sys/ieeefp.h>
#include <ieeefp.h>

static const double zero = 0.0, tiny = 1.0e-307, tiny2 = 1.001e-307,
	huge = 1.0e300;

/*
 * _Q_set_exception(ex) simulates the floating point exceptions indicated by
 * ex.  This routine is not used by the new quad emulation routines but is
 * still used by ../crt/_ftoll.c.
 */
_Q_set_exception(unsigned int ex)
{
	volatile double t;

	if (ex == 0)
		t = zero - zero;			/* clear cexc */
	else {
		if ((ex & (1 << fp_invalid)) != 0)
			t = zero / zero;
		if ((ex & (1 << fp_overflow)) != 0)
			t = huge * huge;
		if ((ex & (1 << fp_underflow)) != 0) {
			if ((ex & (1 << fp_inexact)) != 0 ||
			    (fpgetmask() & FP_X_UFL) != 0)
				t = tiny * tiny;
			else
				t = tiny2 - tiny;	/* exact */
		}
		if ((ex & (1 << fp_division)) != 0)
			t = tiny / zero;
		if ((ex & (1 << fp_inexact)) != 0)
			t = huge + tiny;
	}
}

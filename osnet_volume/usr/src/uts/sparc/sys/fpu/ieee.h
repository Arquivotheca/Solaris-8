/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	_SYS_FPU_IEEE_H
#define	_SYS_FPU_IEEE_H

#pragma ident	"@(#)ieee.h	1.10	92/07/14 SMI"	/* SunOS-4.0 1.4 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Sparc IEEE floating-point support PUBLIC include file.
 */

/*	PUBLIC TYPES 	*/

/*
 * IEEE Arithmetic types... numbered to correspond to fsr fields.
 */

/*
 * rounding direction
 */
enum fp_rounding_direction {
	fp_rd_nearest	= 0,
	fp_rd_zero	= 1,
	fp_rd_plus	= 2,
	fp_rd_minus	= 3
};

/*
 * extended rounding precision
 */
enum fp_rounding_precision {
	fp_rp_extended	= 0,
	fp_rp_single	= 1,
	fp_rp_double	= 2,
	fp_rp_3		= 3
};

/*
 * exceptions according to cexc bit number
 */
enum fp_exception_type {
	fp_inexact	= 0,
	fp_divide	= 1,
	fp_underflow	= 2,
	fp_overflow	= 3,
	fp_invalid	= 4
};

/*
 * floating-point classes according to fclass
 */
enum fp_class_type {
	fp_zero		= 0,
	fp_normal	= 1,		/* Includes subnormal. */
	fp_infinity   	= 2,
	fp_nan		= 3,		/* Includes quiet and signaling NaN. */
};


/* PUBLIC GLOBAL VARIABLES */

unsigned fp_accrued_exceptions;	/* Sticky accumulated exceptions. */


/* PUBLIC FUNCTIONS */

#ifdef	__STDC__

extern enum
fp_rounding_direction swap_rounding_direction(enum fp_rounding_direction);

/* Change rounding mode; return previous. */

extern int swap_accrued_exceptions(int);

/* Change accrued exceptions; return previous. */

#else	/* __STDC__ */

extern enum fp_rounding_direction swap_rounding_direction(/* rd */);
/*	extern enum fp_rounding_direction rd;  */

/* Change rounding mode; return previous. */

extern int swap_accrued_exceptions(/* x */);
/*	int x; */

/* Change accrued exceptions; return previous. */

#endif	/* ! __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FPU_IEEE_H */

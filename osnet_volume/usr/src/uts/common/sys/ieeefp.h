/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_IEEEFP_H
#define	_SYS_IEEEFP_H

#pragma ident	"@(#)ieeefp.h	1.16	99/05/04 SMI"	/* SunOS4.0 1.6	*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Sun types for IEEE floating point.
 */
#if defined(sparc) || defined(__sparc)
enum fp_direction_type {	/* rounding direction */
	fp_nearest	= 0,
	fp_tozero	= 1,
	fp_positive	= 2,
	fp_negative	= 3
};

enum fp_precision_type {	/* extended rounding precision */
	fp_extended	= 0,
	fp_single	= 1,
	fp_double	= 2,
	fp_precision_3	= 3
};

enum fp_exception_type {	/* exceptions according to bit number */
	fp_inexact	= 0,
	fp_division	= 1,
	fp_underflow	= 2,
	fp_overflow	= 3,
	fp_invalid	= 4
};

enum fp_trap_enable_type {	/* trap enable bits according to bit number */
	fp_trap_inexact	= 0,
	fp_trap_division	= 1,
	fp_trap_underflow	= 2,
	fp_trap_overflow	= 3,
	fp_trap_invalid	= 4
};
#endif	/* defined(sparc) || defined(__sparc) */

#if defined(i386) || defined(__i386) || defined(__ia64)
enum fp_direction_type {	/* rounding direction */
	fp_nearest	= 0,
	fp_negative	= 1,
	fp_positive	= 2,
	fp_tozero	= 3
};

enum fp_precision_type {	/* extended rounding precision */
	fp_single	= 0,
	fp_precision_3	= 1,
	fp_double	= 2,
	fp_extended	= 3
};

enum fp_exception_type {	/* exceptions according to bit number */
	fp_invalid	= 0,
	fp_denormalized	= 1,
	fp_division	= 2,
	fp_overflow	= 3,
	fp_underflow	= 4,
	fp_inexact	= 5
};

enum fp_trap_enable_type {	/* trap enable bits according to bit number */
	fp_trap_invalid	= 0,
	fp_trap_denormalized	= 1,
	fp_trap_division	= 2,
	fp_trap_overflow	= 3,
	fp_trap_underflow	= 4,
	fp_trap_inexact	= 5
};
#endif	/* defined(i386) || defined(__i386) */

enum fp_class_type {		/* floating-point classes */
	fp_zero		= 0,
	fp_subnormal	= 1,
	fp_normal	= 2,
	fp_infinity   	= 3,
	fp_quiet	= 4,
	fp_signaling	= 5
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_IEEEFP_H */

/*
 * Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#ident  "@(#)__tbl_tens.c	1.1	92/04/17 SMI"

/*
 * A table of all the powers of ten that fit exactly in an IEEE double, plus
 * a few more.
 */

#include "base_conversion.h"

const double __tbl_tens[__TBL_TENS_MAX + 1] = {
	1.0, 10.0, 100.0, 1000.0, 1.0e4, 1.0e5, 1.0e6, 1.0e7, 1.0e8, 1.0e9,
	1.0e10, 1.0e11, 1.0e12, 1.0e13, 1.0e14, 1.0e15, 1.0e16, 1.0e17, 1.0e18, 1.0e19,
	1.0e20, 1.0e21, 1.0e22,	/* Exact down to here! */
	1.0e23, 1.0e24, 1.0e25, 1.0e26, 1.0e27, 1.0e28, 1.0e29,
	1.0e30, 1.0e31, 1.0e32, 1.0e33, 1.0e34, 1.0e35, 1.0e36, 1.0e37, 1.0e38, 1.0e39,
	1.0e40, 1.0e41, 1.0e42, 1.0e43, 1.0e44, 1.0e45, 1.0e46, 1.0e47, 1.0e48, 1.0e49};

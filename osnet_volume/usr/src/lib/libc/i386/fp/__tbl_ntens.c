/*
 * Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#ident  "@(#)__tbl_ntens.c	1.1	92/04/17 SMI"

/*
 * A table of negative powers of ten for use in converting ascii to single
 * precision
 */

#include "base_conversion.h"

const double __tbl_ntens[__TBL_TENS_MAX + 1] = {
	1.0, 1.0e-1, 1.0e-2, 1.0e-3, 1.0e-4, 1.0e-5, 1.0e-6, 1.0e-7, 1.0e-8, 1.0e-9,
	1.0e-10, 1.0e-11, 1.0e-12, 1.0e-13, 1.0e-14, 1.0e-15, 1.0e-16, 1.0e-17, 1.0e-18, 1.0e-19,
	1.0e-20, 1.0e-21, 1.0e-22, 1.0e-23, 1.0e-24, 1.0e-25, 1.0e-26, 1.0e-27, 1.0e-28, 1.0e-29,
	1.0e-30, 1.0e-31, 1.0e-32, 1.0e-33, 1.0e-34, 1.0e-35, 1.0e-36, 1.0e-37, 1.0e-38, 1.0e-39,
	1.0e-40, 1.0e-41, 1.0e-42, 1.0e-43, 1.0e-44, 1.0e-45, 1.0e-46, 1.0e-47, 1.0e-48, 1.0e-49};

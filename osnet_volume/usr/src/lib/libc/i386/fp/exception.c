/*
 * Copyright (c) 1988, 1989, 1991, by Sun Microsystems, Inc.
 */

#ident	"@(#)exception.c	1.2 0 SMI"

#include <sys/ieeefp.h>

#define FP_EX_MASK	0x3f
#define FP_EX_INVALID	0x01
#define FP_EX_DENORMAL	0x02
#define FP_EX_ZERODIVIDE 0x04
#define FP_EX_OVERFLOW	0x08
#define FP_EX_UNDERFLOW	0x10
#define FP_EX_PRECISION	0x20

_Q_set_exception(ex)
	unsigned ex;
{
	unsigned short cw;
	_getcw(&cw);

	if (ex == 0) {
		cw &= ~FP_EX_MASK;
	} else {
		if (ex & (1 << fp_invalid))
			cw |= FP_EX_INVALID;
		else if (ex & (1 << fp_overflow))
			cw |= FP_EX_OVERFLOW;
		else if (ex & (1 << fp_underflow))
			cw |= FP_EX_UNDERFLOW;
		else if (ex & (1 << fp_inexact))
			cw |= FP_EX_PRECISION;
		else if (ex & (1 << fp_division))
			cw |= FP_EX_ZERODIVIDE;
	}
	_putcw(cw);
	return 0;
}

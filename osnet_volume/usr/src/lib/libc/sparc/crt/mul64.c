/*
 * Copyright (c) 1991-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)mul64.c	1.2	97/04/05 SMI"

#include "synonyms.h"

/*
 * These routines are to support the compiler run-time only, and
 * should NOT be called directly from C!
 */

extern unsigned long long __umul32x32to64(unsigned, unsigned);

long long
__mul64(long long i, long long j)
{
	unsigned i0, i1, j0, j1;
	int sign = 0;
	long long result = 0;

	if (i < 0) {
		i = -i;
		sign = 1;
	}
	if (j < 0) {
		j = -j;
		sign ^= 1;
	}

	i1 = (unsigned)i;
	j0 = j >> 32;
	j1 = (unsigned)j;

	if (j1) {
		if (i1)
			result = __umul32x32to64(i1, j1);
		if ((i0 = i >> 32) != 0)
			result += ((unsigned long long)(i0 * j1)) << 32;
	}
	if (j0 && i1)
		result += ((unsigned long long)(i1 * j0)) << 32;
	return (sign ? -result : result);
}


unsigned long long
__umul64(unsigned long long i, unsigned long long j)
{
	unsigned i0, i1, j0, j1;
	unsigned long long result = 0;

	i1 = i;
	j0 = j >> 32;
	j1 = j;

	if (j1) {
		if (i1)
			result = __umul32x32to64(i1, j1);
		if ((i0 = i >> 32) != 0)
			result += ((unsigned long long)(i0 * j1)) << 32;
	}
	if (j0 && i1)
		result += ((unsigned long long)(i1 * j0)) << 32;
	return (result);
}

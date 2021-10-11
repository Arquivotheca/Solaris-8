/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)modff.c	1.11	96/11/26 SMI"	/* SVr4.0 1.7	*/

/*LINTLIBRARY*/
/*
 * modff(value, iptr) returns the signed fractional part of value
 * and stores the integer part indirectly through iptr.
 *
 */

#pragma weak modff = _modff

#include <sys/types.h>
#include "synonyms.h"
#include <values.h>
#if _IEEE /* machines with IEEE floating point only */
#include <signal.h>
#include <unistd.h>


typedef union {
	float f;
	uint32_t word;
} _fval;
#define	EXPMASK	0x7f800000
#define	FRACTMASK 0x7fffff
#define	FWORD(X)	(((_fval *)&(X))->word)


#endif

float
modff(float value, float *iptr)
{
	float absvalue;

#if _IEEE
	/* raise exception on NaN - 3B only */
	if (((FWORD(value) & EXPMASK) == EXPMASK) &&
		(FWORD(value) & FRACTMASK))
		(void) kill(getpid(), SIGFPE);
#endif
	if ((absvalue = (value >= (float)0.0) ? value : -value) >= FMAXPOWTWO)
		*iptr = value; /* it must be an integer */
	else {
		*iptr = absvalue + FMAXPOWTWO; /* shift fraction off right */
		*iptr -= FMAXPOWTWO; /* shift back without fraction */
		while (*iptr > absvalue) /* above arithmetic might round */
			*iptr -= (float)1.0; /* test again just to be sure */
		if (value < (float)0.0)
			*iptr = -*iptr;
	}
	return (value - *iptr); /* signed fractional part */
}

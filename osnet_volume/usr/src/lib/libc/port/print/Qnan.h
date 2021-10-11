/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_QNAN_H
#define	_QNAN_H

#ident	"@(#)Qnan.h	1.3	96/11/17 SMI"	/* SVr4.0 1.12.1.6	*/

/* Handling of Not_a_Number's (only in IEEE floating-point standard) */

#ifndef _IEEE
#include <values.h>
#endif

#if _IEEE
typedef union
{
	struct
	{
	unsigned sign    : 1;
	unsigned exponent:15;
	unsigned bits    :16;
	unsigned fraction_low1: 32;
	unsigned fraction_low2: 32;
	unsigned fraction_low3: 32;
	} inf_parts;

	struct
	{
	unsigned sign	: 1;
	unsigned exponent :15;
	unsigned qnan_bit : 1;
	unsigned bits	:15;
	unsigned fraction_low1: 32;
	unsigned fraction_low2: 32;
	unsigned fraction_low3: 32;
	} nan_parts;

	long double q;

} qnan;

/*
 * QIsNANorINF checks that exponent of long double == 32767
 * i.e. that number is a NaN or an infinity
 */

#define	QIsNANorINF(X)  (((qnan *)&(X))->nan_parts.exponent == 0x7fff)

/*
 * QIsINF must be used after QIsNANorINF
 * has checked the exponent
 */

#define	QIsINF(X)  (((qnan *)&(X))->inf_parts.bits == 0 &&  \
		((qnan *)&(X))->inf_parts.fraction_low1 == 0 && \
		((qnan *)&(X))->inf_parts.fraction_low2 == 0 && \
		((qnan *)&(X))->inf_parts.fraction_low3 == 0)

/*
 * QIsPosNAN and QIsNegNAN can be used
 * to check the sign of infinities too
 */

#define	QIsPosNAN(X)  (((qnan *)&(X))->nan_parts.sign == 0)

#define	QIsNegNAN(X)  (((qnan *)&(X))->nan_parts.sign == 1)

/*
 * QGETNaNPC gets the leftmost 32 bits
 * of the fraction part
 */

#define	QGETNaNPC(qval)   (((qnan *)&(qval))->inf_parts.bits << 16 | \
	((qnan *)&(qval))->nan_parts.fraction_low1>> 16)

#define	QNaN(X)  (((qnan *)&(X))->nan_parts.exponent == 0x7fff)

#else _IEEE

typedef long double qnan;
#define	QIsNANorINF(X)	0
#define	QIsINF(X)   0
#define	QIsPINF(X)  0
#define	QIsNegNAN(X)  0
#define	QIsPosNAN(X)  0
#define	QIsNAN(X)   0
#define	QGETNaNPC(X)   0L
#define	QNaN(X)  0

#endif _IEEE

#endif 	/* _QNAN_H */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)frexpl.c	1.1	92/04/17 SMI"
/*LINTLIBRARY*/
/*
 * frexpl(value, eptr)
 * returns a long double x such that x = 0 or 0.5 <= |x| < 1.0
 * and stores an integer n such that value = x * 2 ** n
 * indirectly through eptr.
 *
 */
#include "synonyms.h"
#include "shlib.h"
#include <nan.h>

asm	long double
xfrexpl(val,contwo,ptr)
{
%mem	val,contwo,ptr;
	movl	ptr,%eax
	fldt	val
	fxtract
	fxch	%st(1)
	fistpl	(%eax)
	fidiv	contwo
	incl	(%eax)
}

long double
frexpl(value, eptr)
long double value; /* don't declare register, because of KILLNan! */
int *eptr;
{
	static int contwo = 2;

	KILLNaN(value); /* raise exception on Not-a-Number (3b only) */
	*eptr = 0;
	if (value == 0.0) /* nothing to do for zero */
		return (value);
	return(xfrexpl(value,contwo,eptr));
}

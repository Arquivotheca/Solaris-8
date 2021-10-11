/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)frexp.c	1.9	96/11/15 SMI"	/* SVr4.0 1.9.1.3	*/

/*LINTLIBRARY*/
/*
 * frexp(value, eptr)
 * returns a double x such that x = 0 or 0.5 <= |x| < 1.0
 * and stores an integer n such that value = x * 2 ** n
 * indirectly through eptr.
 *
 */
#include <sys/types.h>
#include "synonyms.h"
#include "shlib.h"
#include <nan.h>
#include <signal.h>


double
frexp(double value, int *eptr)
{
	double absvalue;

	*eptr = 0;
	if (value == 0.0 || IsNANorINF(value)) /* nothing to do for zero */
		return (value);
	absvalue = (value > 0.0) ? value : -value;
	for (; absvalue >= 1.0; absvalue *= 0.5)
		++*eptr;
	for (; absvalue < 0.5; absvalue += absvalue)
		--*eptr;
	return (value > 0.0 ? absvalue : -absvalue);
}

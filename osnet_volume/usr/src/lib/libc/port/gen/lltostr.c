/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)lltostr.c	1.3	96/10/15 SMI"

/*LINTLIBRARY*/
/*
 *	lltostr -- convert long long to decimal string
 *
 *	char *
 *	lltostr(value, ptr)
 *	long long value;
 *	char *ptr;
 *
 *	Ptr is assumed to point to the byte following a storage area
 *	into which the decimal representation of "value" is to be
 *	placed as a string.  Lltostr converts "value" to decimal and
 *	produces the string, and returns a pointer to the beginning
 *	of the string.  No leading zeroes are produced, and no
 *	terminating null is produced.  The low-order digit of the
 *	result always occupies memory position ptr-1.
 *	Lltostr's behavior is undefined if "value" is negative.  A single
 *	zero digit is produced if "value" is zero.
 */

#pragma weak lltostr = _lltostr
#pragma weak ulltostr = _ulltostr

#include "synonyms.h"
#include <sys/types.h>
#include <stdlib.h>

char *
lltostr(longlong_t value, char *ptr)
{
	longlong_t t;

	do {
		*--ptr = (char)('0' + value - 10 * (t = value / 10));
	} while ((value = t) != 0);

	return (ptr);
}

char *
ulltostr(u_longlong_t value, char *ptr)
{
	u_longlong_t t;

	do {
		*--ptr = (char)('0' + value - 10 * (t = value / 10));
	} while ((value = t) != 0);

	return (ptr);
}

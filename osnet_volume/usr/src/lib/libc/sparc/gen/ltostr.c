/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ltostr.c	1.5	96/11/25 SMI"	/* SVr4.0 1.4 */

/*LINTLIBRARY*/

/*
 *	ltostr -- convert long to decimal string
 *
 *
 *	char *
 *	ltostr(value, ptr)
 *	long value;
 *	char *ptr;
 *
 *	Ptr is assumed to point to the byte following a storage area
 *	into which the decimal representation of "value" is to be
 *	placed as a string.  Ltostr converts "value" to decimal and
 *	produces the string, and returns a pointer to the beginning
 *	of the string.  No leading zeroes are produced, and no
 *	terminating null is produced.  The low-order digit of the
 *	result always occupies memory position ptr-1.
 *	Ltostr's behavior is undefined if "value" is negative.  A single
 *	zero digit is produced if "value" is zero.
 *
 */
#include "synonyms.h"
#include <sys/types.h>

char *
_ltostr(long value, char *ptr)
{
	long t;

	do {
		*--ptr = '0' + (char)(value - 10 * (t = value / 10));
	} while ((value = t) != 0);

	return (ptr);
}

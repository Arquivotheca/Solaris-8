/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strtrns.c	1.8	97/03/28 SMI"	/* SVr4.0 1.1.2.2 */

/*LINTLIBRARY*/

#pragma weak strtrns = _strtrns

#include "synonyms.h"

/*
 *	Copy `str' to `result' replacing any character found
 *	in both `str' and `old' with the corresponding character from `new'.
 *	Return `result'.
 */

char *
strtrns(const char *str, const char *old, const char *new,
    char *result)
{
	char		*r;
	const char	*o;

	for (r = result; *r = *str++; r++)
		for (o = old; *o; )
			if (*r == *o++) {
				*r = new[o - old -1];
				break;
			}
	return (result);
}

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strfind.c	1.10	97/08/12 SMI" /* SVr4.0 1.1.2.2 */

/*LINTLIBRARY*/

#pragma weak strfind = _strfind

#include "synonyms.h"
#include <sys/types.h>

/*
 *	If `s2' is a substring of `s1' return the offset of the first
 *	occurrence of `s2' in `s1',
 *	else return -1.
 */

int
strfind(const char *as1, const char *as2)
{
	const char	*s1, *s2;
	char		c;
	ptrdiff_t	offset;

	s1 = as1;
	s2 = as2;
	c = *s2;

	while (*s1)
		if (*s1++ == c) {
			offset = s1 - as1 - 1;
			s2++;
			while ((c = *s2++) == *s1++ && c)
				;
			if (c == 0)
				return ((int)offset);
			s1 = offset + as1 + 1;
			s2 = as2;
			c = *s2;
		}
	return (-1);
}

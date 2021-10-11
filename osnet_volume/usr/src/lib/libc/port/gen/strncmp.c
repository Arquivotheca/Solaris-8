/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)strncmp.c	1.10	97/05/19 SMI"	/* SVr4.0 1.11	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <string.h>
#include <sys/types.h>

/*
 * Compare strings (at most n bytes)
 *	returns: s1>s2; >0  s1==s2; 0  s1<s2; <0
 */
int
strncmp(const char *s1, const char *s2, size_t n)
{
	n++;
	if (s1 == s2)
		return (0);
	while (--n != 0 && *s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (n == 0 ? 0 : *(unsigned char *)s1 - *(unsigned char *)--s2);
}

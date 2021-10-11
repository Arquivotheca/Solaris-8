/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)memcmp.c	1.11	96/11/25 SMI"

/*LINTLIBRARY*/

#pragma	weak	memcmp = _memcmp

#include "synonyms.h"

#include <sys/types.h>
#include <string.h>
#include <stddef.h>
#include <memory.h>

/*
 * Compare n bytes:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 */
int
memcmp(const void *s1, const void *s2, size_t n)
{
	if (s1 != s2 && n != 0) {
		const unsigned char *ps1 = s1;
		const unsigned char *ps2 = s2;

		do {
			if (*ps1++ != *ps2++)
				return (ps1[-1] - ps2[-1]);
		} while (--n != 0);
	}
	return (NULL);
}

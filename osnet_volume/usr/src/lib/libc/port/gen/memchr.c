/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)memchr.c	1.10	96/11/25 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <stddef.h>
#include <string.h>
#include <memory.h>

/*
 * Return the ptr in sptr at which the character c1 appears;
 * or NULL if not found in n chars; don't stop at \0.
 */
void *
memchr(const void *sptr, int c1, size_t n)
{
	if (n != 0) {
		unsigned char c = (unsigned char)c1;
		const unsigned char *sp = sptr;

		do {
			if (*sp++ == c)
				return ((void *)--sp);
		} while (--n != 0);
	}
	return (NULL);
}

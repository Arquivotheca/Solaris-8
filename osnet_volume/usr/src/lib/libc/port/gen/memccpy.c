/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)memccpy.c	1.12	96/11/25 SMI"

/*LINTLIBRARY*/

#pragma weak memccpy = _memccpy

#include "synonyms.h"
#include <sys/types.h>
#include <string.h>
#include <stddef.h>
#include <memory.h>

/*
 * Copy s0 to s, stopping if character c is copied. Copy no more than n bytes.
 * Return a pointer to the byte after character c in the copy,
 * or NULL if c is not found in the first n bytes.
 */
void *
memccpy(void *s, const void *s0, int c, size_t n)
{
	if (n != 0) {
		unsigned char *s1 = s;
		const unsigned char *s2 = s0;
		do {
			if ((*s1++ = *s2++) == (unsigned char)c)
				return (s1);
		} while (--n != 0);
	}
	return (NULL);
}

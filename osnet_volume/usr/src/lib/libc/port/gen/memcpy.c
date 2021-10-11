/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)memcpy.c	1.10	96/11/25 SMI"

/*LINTLIBRARY*/

#pragma	weak	memcpy = _memcpy

#include "synonyms.h"
#include <sys/types.h>
#include <stddef.h>
#include <string.h>
#include <memory.h>

/*
 * Copy s0 to s, always copy n bytes.
 * Return s
 */
void *
memcpy(void *s, const void *s0, size_t n)
{
	if (n != 0) {
		char *s1 = s;
		const char *s2 = s0;

		do {
			*s1++ = *s2++;
		} while (--n != 0);
	}
	return (s);
}

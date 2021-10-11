/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)memset.c	1.11	96/11/25 SMI"

/*LINTLIBRARY*/

#pragma	weak	memset = _memset

#include "synonyms.h"
#include <sys/types.h>
#include <string.h>
#include <memory.h>

/*
 * Set an array of n chars starting at sp to the character c.
 * Return sp.
 */
void *
memset(void *sp1, int c, size_t n)
{
	if (n != 0) {
		unsigned char *sp = sp1;
		do {
			*sp++ = (unsigned char)c;
		} while (--n != 0);
	}

	return (sp1);
}

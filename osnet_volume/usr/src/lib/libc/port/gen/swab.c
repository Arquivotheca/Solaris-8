/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)swab.c	1.8	96/12/04 SMI"	/* SVr4.0 1.10	*/

/*LINTLIBRARY*/
/*
 * Swab bytes
 */

#pragma weak swab = _swab

#include "synonyms.h"
#include <sys/types.h>
#include <unistd.h>


#define	STEP    if (1) \
		{ temp = *from++; *to++ = *from++; *to++ = temp; } \
		else

void
swab(const void *src, void *dest, ssize_t n)
{
	char temp;
	const char *from = (const char *) src;
	char *to = (char *) dest;

	if (n <= 1)
		return;
	n >>= 1; n++;
	/* round to multiple of 8 */
	while ((--n) & 07)
		STEP;
	n >>= 3;
	while (--n >= 0) {
		STEP; STEP; STEP; STEP;
		STEP; STEP; STEP; STEP;
	}
}

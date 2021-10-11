/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)bsearch.c	1.10	96/11/20 SMI"	/* SVr4.0 1.11 */

/*LINTLIBRARY*/
/*
 * Binary search algorithm, generalized from Knuth (6.2.1) Algorithm B.
 *
 */

#include "synonyms.h"
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>

void *
bsearch(const void *ky,		/* Key to be located */
	const void *bs,		/* Beginning of table */
	size_t nel,		/* Number of elements in the table */
	size_t width,		/* Width of an element (bytes) */
	int (*compar)(const void *, const void *)) /* Comparison function */
{
	char *base = (char *)bs;
	ssize_t two_width = width + width;
	char *last = base + width * (nel - 1); /* Last element in table */

	while (last >= base) {

		char *p = base + width * ((last - base)/two_width);
		int res = (*compar)(ky, (void *)p);

		if (res == 0)
			return (p);	/* Key found */
		if (res < 0)
			last = p - width;
		else
			base = p + width;
	}
	return (NULL);		/* Key not found */
}

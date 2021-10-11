/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)lsearch.c	1.10	96/12/06 SMI"	/* SVr4.0 1.15	*/

/*LINTLIBRARY*/
/*
 * Linear search algorithm, generalized from Knuth (6.1) Algorithm Q.
 *
 * This version no longer has anything to do with Knuth's Algorithm Q,
 * which first copies the new element into the table, then looks for it.
 * The assumption there was that the cost of checking for the end of the
 * table before each comparison outweighed the cost of the comparison, which
 * isn't true when an arbitrary comparison function must be called and when the
 * copy itself takes a significant number of cycles.
 * Actually, it has now reverted to Algorithm S, which is "simpler."
 */

#pragma weak lsearch = _lsearch

#include "synonyms.h"
#include <mtlib.h>
#include <sys/types.h>
#include <stddef.h>
#include <string.h>
#include <thread.h>
#include <synch.h>

#ifdef _REENTRANT
mutex_t __lsearch_lock = DEFAULTMUTEX;
#endif _REENTRANT

void *
lsearch(const void *ky, const void *bs, size_t *nelp, size_t width, int (*compar)(const void *, const void *))
{
	char * key = (char *)ky;
	char * base = (char *)bs;
	char * next = base + *nelp * width;	/* End of table */
	void *res;

	(void) _mutex_lock(&__lsearch_lock);
	for (; base < next; base += width)
		if ((*compar)(key, base) == 0) {
			(void) _mutex_unlock(&__lsearch_lock);
			return (base);	/* Key found */
		}
	++*nelp;			/* Not found, add to table */
	res = memcpy(base, key, width);	/* base now == next */
	(void) _mutex_unlock(&__lsearch_lock);
	return (res);
}

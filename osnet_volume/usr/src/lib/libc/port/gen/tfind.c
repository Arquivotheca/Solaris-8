/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tfind.c	1.10	96/11/25 SMI"	/* SVr4.0 1.10	*/

/*LINTLIBRARY*/
/*
 * Tree search algorithm, generalized from Knuth (6.2.2) Algorithm T.
 *
 * The NODE * arguments are declared in the lint files as char *,
 * because the definition of NODE isn't available to the user.
 */

#pragma weak tfind = _tfind

#include "synonyms.h"
#include <mtlib.h>
#include <sys/types.h>
#include <search.h>
#include <thread.h>
#include <synch.h>

#ifdef _REENTRANT
extern mutex_t __treelock;
#endif _REENTRANT
typedef struct node { void *key; struct node *llink, *rlink; } NODE;


/*	tfind - find a node, or return 0	*/
/*
 * ky is Key to be located
 * rtp is the Address of the root of the tree
 * compar is the Comparison function
 */
static void *
_tfind_unlocked(const void *ky, void *const *rtp, int (*compar)(const void *, const void * ) )
{
	void *key = (char *)ky;
	NODE **rootp = (NODE **)rtp;
	if (rootp == NULL)
		return (NULL);
	while (*rootp != NULL) {			/* T1: */
		int r = (*compar)(key, (*rootp)->key);	/* T2: */
		if (r == 0)
			return ((void *)*rootp);	/* Key found */
		rootp = (r < 0) ?
		    &(*rootp)->llink :		/* T3: Take left branch */
		    &(*rootp)->rlink;		/* T4: Take right branch */
	}
	return (NULL);
}

void *
tfind(const void *ky, void *const *rtp, int (*compar)(const void *,
const void *))

{
	void *r;
	(void) _mutex_lock(&__treelock);
	r = _tfind_unlocked(ky, rtp, compar);
	(void) _mutex_unlock(&__treelock);
	return (r);
}

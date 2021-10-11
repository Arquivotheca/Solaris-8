/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)slp_search.c	1.1	99/04/02 SMI"

/*
 * Binary tree access. The routines contained in this file are:
 *  slp_tsearch
 *  slp_tfind
 *  slp_twalk
 *
 * These have the same interfaces as tsearch(3C), tfind(3C), and
 * twalk(3C), with two important distinctions:
 * - libc twalk is inadequate, since it doesn't allow the caller to pass
 *   cookies into the action function (prohibiting thread-safe usage).
 *   slp_twalk allows cookies.
 * - libc tsearch and tfind *always* lock access to the tree. This can
 *   be inefficient when it isn't necessary to lock the tree in order
 *   to ensure thread-safety. It is the responsibility of the caller to
 *   ensure that slp_tsearch and slp_tfind are safe for concurrent access.
 *
 * It is possible for this implementation to degenerate into a
 * linked-list algorithm with certain inputs. If this proves to be
 * a problem in practice, these routines can be optimized by balancing
 * the trees.
 */

#include <stdio.h>
#include <stdlib.h>
#include <slp-internal.h>

static struct node { char *key; struct node *llink, *rlink; };
typedef struct node NODE;

void slp_twalk(void *r,
		void (*action)(void *, VISIT, int, void *),
		int level, void *cookie) {
	NODE *root = (NODE *) r;
	if (root->llink == NULL && root->rlink == NULL)
		(*action)(root, leaf, level, cookie);
	else {
		(*action)(root, preorder, level, cookie);
		if (root->llink != NULL)
			slp_twalk(root->llink, action, level + 1, cookie);
		(*action)(root, postorder, level, cookie);
		if (root->rlink != NULL)
			slp_twalk(root->rlink, action, level + 1, cookie);
		(*action)(root, endorder, level, cookie);
	}
}

/* Find or insert key into search tree */
void *slp_tsearch(const void *ky, void **rtp, int (* compar)()) {
	char *key = (char *)ky;
	NODE **rootp = (NODE **)rtp;
	NODE *q;	/* New node if key not found */

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
	q = (NODE *) malloc(sizeof (NODE));	/* T5: Not found */
	if (q != NULL) {			/* Allocate new node */
		*rootp = q;			/* Link new node to old */
		q->key = key;			/* Initialize new node */
		q->llink = q->rlink = NULL;
	}
	return ((void *)q);
}

void *slp_tfind(const void *ky, void *const *rtp,
		int (*compar)(const void *, const void *)) {
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

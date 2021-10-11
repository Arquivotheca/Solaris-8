/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma  ident	"@(#)tsearch.c	1.12	98/01/30 SMI"	/* SVr4.0 2.11	*/

/*LINTLIBRARY*/
/*
 * Tree search algorithm, generalized from Knuth (6.2.2) Algorithm T.
 *
 *
 * The NODE * arguments are declared in the lint files as char *,
 * because the definition of NODE isn't available to the user.
 */

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma weak tdelete = _tdelete
#pragma weak tsearch = _tsearch
#pragma weak twalk = _twalk

#include "synonyms.h"
#include <mtlib.h>
#include <sys/types.h>
#include <search.h>
#include <stdlib.h>
#include <thread.h>
#include <synch.h>

#ifdef _REENTRANT
mutex_t __treelock = DEFAULTMUTEX;
#endif _REENTRANT



typedef struct node { char * key; struct node *llink, *rlink; } NODE;

static void * _tsearch_unlocked(const void *, void **, int (*)(const
void *, const void *));
static void * _tdelete_unlocked(const void *, void **, int (*)(const
void *, const void *));
static void __twalk(NODE *, void (*)(const void *,VISIT,int), int);


/* Find or insert key into search tree */
static void *
_tsearch_unlocked(const void *ky,   /* Key to be located */
		void ** rtp,        /* Address of the root of the tree */
		int (* compar)())   /* Comparison function */
{
	char * key = (char *)ky;
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

/* Find or insert key into search tree */
void *
tsearch(const void *ky,		/* Key to be located */
	void **rtp,		/*Address of the root of the tree */
	int (*compar)(const void *k1, const void *k2))/* Comparison function */
{
	void *r;
	(void)_mutex_lock(&__treelock);
	r = _tsearch_unlocked(ky, rtp, compar);
	(void)_mutex_unlock(&__treelock);
	return (r);
}

/* Delete node with key key */
static void *
_tdelete_unlocked(const void *ky,  /* Key to be deleted */
		  void **rtp,	   /* Address of the root of tree */
 	          int (*compar)()) /* Comparison function */
{
	char * key = (char *) ky;
	NODE **rootp = (NODE **)rtp;
	NODE *p;		/* Parent of node to be deleted */
	NODE *q;	/* Successor node */
	NODE *r;	/* Right son node */
	int ans;		/* Result of comparison */

	if (rootp == NULL || (p = *rootp) == NULL)
		return (NULL);
	while ((ans = (*compar)(key, (*rootp)->key)) != 0) {
		p = *rootp;
		rootp = (ans < 0) ?
		    &(*rootp)->llink :		/* Take left branch */
		    &(*rootp)->rlink;		/* Take right branch */
		if (*rootp == NULL)
			return (NULL);		/* Key not found */
	}
	r = (*rootp)->rlink;			/* D1: */
	if ((q = (*rootp)->llink) == NULL)	/* Llink NULL? */
		q = r;
	else if (r != NULL) {			/* Rlink NULL? */
		if (r->llink == NULL) {		/* D2: Find successor */
			r->llink = q;
			q = r;
		} else {			/* D3: Find NULL link */
			for (q = r->llink; q->llink != NULL; q = r->llink)
				r = q;
			r->llink = q->rlink;
			q->llink = (*rootp)->llink;
			q->rlink = (*rootp)->rlink;
		}
	}
	free((char *) *rootp);		/* D4: Free node */
	*rootp = q;			/* Link parent to replacement */
	return ((void *)p);
}


/* Delete node with key key */
void *
tdelete(const void *ky, 	/* Key to be deleted */
	void **rtp, 		 /* Address of the root of tree */
 	int (*compar)(const void *k1, const void *k2))/* Comparison function */
{
	void *r;
	(void)_mutex_lock(&__treelock);
	r = _tdelete_unlocked(ky, rtp, compar);
	(void)_mutex_unlock(&__treelock);
	return (r);
}


/* Walk the nodes of a tree */
void
twalk(   const void *rt,		/* Root of the tree to be walked */
	 void (*action)(const void *, VISIT, int))/*Function to be called at each node*/
{
	NODE *root = (NODE *)rt;

	(void)_mutex_lock(&__treelock);
	if (root != NULL && action != NULL)
		__twalk(root, action, 0);
	(void)_mutex_unlock(&__treelock);
}


/* Walk the nodes of a tree */
static void
__twalk(NODE *root,		/* Root of the tree to be walked */
 	void (*action)(const void *,VISIT,int),/*Function to be called each node */
 	int level)
{
	if (root->llink == NULL && root->rlink == NULL)
		(*action)(root, leaf, level);
	else {
		(*action)(root, preorder, level);
		if (root->llink != NULL)
			__twalk(root->llink, action, level + 1);
		(*action)(root, postorder, level);
		if (root->rlink != NULL)
			__twalk(root->rlink, action, level + 1);
		(*action)(root, endorder, level);
	}
}

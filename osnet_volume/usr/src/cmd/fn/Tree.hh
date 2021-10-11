/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _TREE_HH
#define	_TREE_HH

#pragma ident	"@(#)Tree.hh	1.1	94/12/05 SMI"


// Generic tree structure.  A tree is simply a list of subtrees.
// Subclasses define the information (if any) stored at each node
// of the tree.


#include <stdlib.h>
#include "List.hh"


class Tree : public List, public ListItem {
public:
	// The parent of this tree node, or NULL if none.
	Tree *parent;

	Tree();
	~Tree();

	// Compare the roots of two trees.  Return value is as for strcmp().
	virtual int compare_root(const Tree *) const;

	// Assign the root of another tree to the root of this tree.
	// Subtrees are not affected.
	virtual void assign_root(const Tree *);

	// Iterate through the subtrees, returning NULL when there are
	// no more.
	Tree *first(void *&iter_pos);
	Tree *next(void *&iter_pos);

	// Add and return a new subtree.  If a subtree with the same
	// root (according to compare_root()) already exists, then
	// copy the root of the argument into the existing subtree,
	// delete the argument, and return the existing subtree instead.
	// Return NULL on error.
	Tree *insert(Tree *);

	// Operations that are not implemented.
	Tree(const Tree &);
	Tree &operator=(const Tree &);
	ListItem *copy();
};


#endif	// _TREE_HH

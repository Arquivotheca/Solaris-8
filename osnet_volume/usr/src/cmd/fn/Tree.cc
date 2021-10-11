/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Tree.cc	1.1	94/12/05 SMI"

#include <stdlib.h>
#include "List.hh"
#include "Tree.hh"


Tree::Tree()
{
	parent = NULL;
}

Tree::~Tree()
{
}

int
Tree::compare_root(const Tree *tree) const
{
	return ((int)(this - tree));
}

void
Tree::assign_root(const Tree *)
{
}

Tree *
Tree::first(void *&iter_pos)
{
	return ((Tree *)List::first(iter_pos));
}

Tree *
Tree::next(void *&iter_pos)
{
	return ((Tree *)List::next(iter_pos));
}

Tree *
Tree::insert(Tree *tree)
{
	Tree *subtree;
	void *iter;
	for (subtree = first(iter); subtree != NULL; subtree = next(iter)) {
		if (subtree->compare_root(tree) == 0) {
			subtree->assign_root(tree);
			delete tree;
			return (subtree);
		}
	}
	tree->parent = this;
	return ((append_item(tree) != 0) ? tree : NULL);
}

ListItem *
Tree::copy()
{
	exit(-1);	// not implemented
	return (NULL);
}

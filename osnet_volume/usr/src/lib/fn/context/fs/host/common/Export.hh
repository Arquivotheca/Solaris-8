/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _EXPORT_HH
#define	_EXPORT_HH

#pragma ident	"@(#)Export.hh	1.1	94/12/05 SMI"

#include <xfn/xfn.hh>
#include <rpc/rpc.h>
#include <time.h>
#include <synch.h>
#include "List.hh"


// The class ExportTree is a tree representing the directories
// exported by a single host.  Each tree has associated with it a host
// name, a creation time, and a reference count.
//
// Each directory is represented by a node of class ExportNode in the
// tree.  Associated with each directory is its atomic name and a flag
// telling whether it is exported.  If "/export" is the only exported
// directory, for example, the node for "/" would not be marked as
// being exported, but its child corresponding to "/export" would be.
// The node for "/" is labeled with the empty string.


class ExportTree;


class ExportNode : public List, public ListItem {
public:
	FN_string name;
	bool_t exported;
	ExportTree *tree;	// the tree of which this node is a part

	ExportNode(const FN_string &name, ExportTree *tree = NULL);
					// "exported" is initialized to FALSE
	~ExportNode();

	// Return the child with the given name, or NULL if there is none.
	ExportNode *find_child(const FN_string &name);

	// Create and return a new child of this node.  If a child
	// with the same name already exists, return that child
	// instead.  Return NULL on error.
	ExportNode *insert(const FN_string &name);

	// Print the contents of the node (and its children,
	// recursively) to standard output.
	void print(unsigned int depth = 0);

	ListItem *copy();	// not implemented
};


class ExportTree : public ListItem {
public:
	char *hostname;
	time_t creation_time;
	ExportNode root;

	ExportTree(const char *hostname);
	~ExportTree();

	unsigned int refcount();	// refcount is initialized to 1
	void increment_refcount();
	void decrement_refcount();

	// Print the contents of the tree to standard output.
	void print();

	ListItem *copy();	// not implemented

private:
	mutex_t refcount_lock;
	unsigned int _refcount;
};


#endif	// _EXPORT_HH

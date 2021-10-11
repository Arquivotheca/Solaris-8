/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNCREATE_FS_HH
#define	_FNCREATE_FS_HH

#pragma ident	"@(#)fncreate_fs.hh	1.2	96/03/31 SMI"


#include <stdio.h>
#include <rpc/rpc.h>
#include <xfn/xfn.hh>
#include "Tree.hh"


// The class Dir is used to build a tree representing the directory
// structure of the entire input file.  This is a tree of atomic
// names, with an optional location associated with each name.  This
// location actually contains both the options and the location for an
// entry.

class Dir : public Tree {
public:
	FN_string *name;
	char *location;

	~Dir();
	Dir(FN_string *name, char *location = NULL);	// does not copy
	int compare_root(const Tree *) const;
	void assign_root(const Tree *);			// copies
	Dir *insert(Dir *);

	// Print the contents of the tree to standard output.
	void print(unsigned int depth = 0);

	// Print the names, slash-separated, from the root of the tree to here.
	void Dir::print_name_hierarchy();
};


// Command line arguments.  cmdline_location contains both the mount
// options and mount location (eg: "-ro svr1:/export").

extern FILE	*infile;
extern char	*cmdline_location;
extern int	verbose;	// 1 for verbose, higher for debugging info


// Return a tree representing the input, or NULL on error.

extern Dir *parse_input();


// Return the context named by all but the last component of name,
// creating new intermediate file system contexts if necessary.
// Set nsid to true if this is an nsid context (the last component of
// name must in that case be "fs" or "_fs").

extern FN_ctx *penultimate_ctx(FN_composite_name *name, bool_t &nsid);


// Update the file system namespace of the given context based upon the
// contents of the input tree.  nsid is true if ctx is an nsid context.

extern void update_namespace(FN_ctx *ctx, Dir *input,
    bool_t nsid = FALSE);


// Recursively destroy all contexts and unbind all references at or
// below fullname.  ctx is the parent context of the final component
// of fullname.  nsid tells whether ctx is a nsid context, in which
// case the _fs alias is unbound as well.

extern void destroy(const FN_composite_name *fullname, FN_ctx *ctx,
    bool_t nsid);


// Given a string, return the corresponding composite name.  Delete a
// trailing empty component, if any.  If delete_empty is true, delete
// all empty components.

extern FN_composite_name *composite_name_from_str(const char *str,
    bool_t delete_empty = FALSE);


// Given an FNS composite name or string, return a C string.

extern const char *str(const FN_composite_name *name);
extern const char *str(const FN_string *string);


// Concatenate two strings, separated by "sep" if it is not '\0'.
// The result is returned in newly-allocated memory.

extern char *concat(const char *s1, const char *s2 = NULL, char sep = '\0');


// Assert that ptr is not null.

extern void mem_check(const void *ptr);


// Print a message.  Arguments are as for printf().

extern void info(const char *fmt, ...);


// Print an error message, including the status description if status is
// non-null, and exit.  Other arguments are as for printf().

extern void error(const FN_status *status, const char *fmt, ...);


#endif	/* _FNCREATE_FS_HH */

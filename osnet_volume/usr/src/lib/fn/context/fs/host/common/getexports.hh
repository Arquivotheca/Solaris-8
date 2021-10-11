/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _GETEXPORTS_HH
#define	_GETEXPORTS_HH

#pragma ident	"@(#)getexports.hh	1.1	94/12/05 SMI"


#include "Export.hh"


// Return a tree representing the directory structure exported by
// hostname, or NULL on error.  The caller must call release_export_tree()
// to release the tree when finished with it.

ExportTree *export_tree(const char *hostname);


// After calling release_export_tree(), the caller may not use
// the tree or anything derived from it.

void release_export_tree(ExportTree *tree);



#endif	// _GETEXPORTS_HH

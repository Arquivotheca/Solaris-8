/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_NAMESET_HH
#define	_XFN_FN_NAMESET_HH

#pragma ident	"@(#)FN_nameset.hh	1.3	96/03/31 SMI"

#include <xfn/FN_nameset.h>
#include <xfn/FN_string.hh>
#include <xfn/misc_codes.h>

class FN_nameset_rep;

class FN_nameset {
    public:
	FN_nameset();
	~FN_nameset();

	// copy and assignment
	FN_nameset(const FN_nameset&);
	FN_nameset& operator=(const FN_nameset&);

	// get count of names
	unsigned int count(void) const;

	// get first name (points iter_pos after name)
	const FN_string* first(void*& iter_pos) const;

	// get name following iter_pos (points iter_pos after name)
	const FN_string* next(void*& iter_pos) const;

	// add name to set (fails if name already in set and exclusive nonzero)
	int add(const FN_string& name,
		unsigned int exclusive = FN_OP_EXCLUSIVE);

	// remove name from set
	int remove(const FN_string& name);

    protected:
	FN_nameset_rep* rep;
	static FN_nameset_rep* get_rep(const FN_nameset&);
	FN_nameset(FN_nameset_rep*);
};

#endif /* _XFN_FN_NAMESET_HH */

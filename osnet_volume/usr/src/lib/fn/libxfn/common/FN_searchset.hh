/*
 * Copyright (c) 1993 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_SEARCHSET_HH
#define	_XFN_FN_SEARCHSET_HH

#pragma ident	"@(#)FN_searchset.hh	1.1	96/03/31 SMI"

#include <xfn/FN_ref.hh>
#include <xfn/FN_string.hh>
#include <xfn/FN_attrset.hh>
#include <xfn/misc_codes.h>

class FN_searchset_rep;

class FN_searchset {
public:
	FN_searchset(int refs = 0, int attrs = 0);
	~FN_searchset();

	// copy and assignment
	FN_searchset(const FN_searchset&);
	FN_searchset& operator=(const FN_searchset&);

	// get count of search items
	unsigned int count(void) const;

	// return whether entry is present in set
	int present(const FN_string &name) const;

	// get first search (points iter_pos after search)
	const FN_string* first(void*& iter_pos,
		const FN_ref** = 0, const FN_attrset** = 0,
		void **cur_pos = 0) const;

	// get search after iter_pos (points iter_pos after search)
	const FN_string* next(void*& iter_pos,
		const FN_ref** = 0, const FN_attrset** = 0,
		void **curr_pos = 0) const;

	// add search to set (fails if name already in set && exclusive nonzero)
	int add(const FN_string& name,
		const FN_ref* = 0,
		const FN_attrset* = 0,
		unsigned int exclusive = FN_OP_EXCLUSIVE);

	// remove search from set
	int remove(const FN_string& name);

	int set_ref(void *posn, FN_ref *ref);
	int set_attrs(void *posn, FN_attrset *attrs);

	int refs_filled();
	int attrs_filled();

protected:
	FN_searchset_rep* rep;
	static FN_searchset_rep* get_rep(const FN_searchset&);
	FN_searchset(FN_searchset_rep*);
};

#endif /* _XFN_FN_SEARCHSET_HH */

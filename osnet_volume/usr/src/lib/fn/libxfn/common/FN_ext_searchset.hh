/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_EXT_SEARCHSET_HH
#define	_XFN_FN_EXT_SEARCHSET_HH

#pragma ident	"@(#)FN_ext_searchset.hh	1.1	96/03/31 SMI"

#include <xfn/FN_ref.hh>
#include <xfn/FN_composite_name.hh>
#include <xfn/FN_attrset.hh>
#include <xfn/misc_codes.h>

class FN_ext_searchset_rep;

class FN_ext_searchset {
public:
	FN_ext_searchset(int refs = 0, int attrs = 0);
	~FN_ext_searchset();

	// copy and assignment
	FN_ext_searchset(const FN_ext_searchset&);
	FN_ext_searchset& operator=(const FN_ext_searchset&);

	// get count of search items
	unsigned int count(void) const;

	// return whether entry is present in set
	int present(const FN_composite_name &name) const;

	// get first search (points iter_pos after search)
	const FN_composite_name* first(void*& iter_pos,
		const FN_ref** = 0, const FN_attrset** = 0,
		unsigned int *rel = 0, void **cur_pos = 0) const;

	// get search after iter_pos (points iter_pos after search)
	const FN_composite_name* next(void*& iter_pos,
		const FN_ref ** = 0, const FN_attrset ** = 0,
		unsigned int *rel = 0,
		void **curr_pos = 0) const;

	// add search to set (fails if name already in set && exclusive nonzero)
	int add(const FN_composite_name& name,
		const FN_ref* = 0,
		const FN_attrset* = 0,
		unsigned int relative = 1,
		unsigned int exclusive = FN_OP_EXCLUSIVE);

	// remove search from set
	int remove(const FN_composite_name& name);

	int set_ref(void *posn, FN_ref *ref);
	int set_attrs(void *posn, FN_attrset *attrs);

	int refs_filled();
	int attrs_filled();

protected:
	FN_ext_searchset_rep* rep;
	static FN_ext_searchset_rep* get_rep(const FN_ext_searchset&);
	FN_ext_searchset(FN_ext_searchset_rep*);
};

#endif /* _XFN_FN_EXT_SEARCHSET_HH */

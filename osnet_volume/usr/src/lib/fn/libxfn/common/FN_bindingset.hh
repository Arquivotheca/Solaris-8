/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_BINDINGSET_HH
#define	_XFN_FN_BINDINGSET_HH

#pragma ident	"@(#)FN_bindingset.hh	1.3	96/03/31 SMI"

#include <xfn/FN_bindingset.h>
#include <xfn/FN_ref.hh>
#include <xfn/FN_string.hh>
#include <xfn/misc_codes.h>

class FN_bindingset_rep;

class FN_bindingset {
public:
	FN_bindingset();
	~FN_bindingset();

	// copy and assignment
	FN_bindingset(const FN_bindingset&);
	FN_bindingset& operator=(const FN_bindingset&);

	// get count of bindings
	unsigned int count(void) const;

	// get reference for specified name
	const FN_ref* get_ref(const FN_string& name) const;

	// get first binding (points iter_pos after binding)
	const FN_string* first(void*& iter_pos,
				const FN_ref*&) const;

	// get binding after iter_pos (points iter_pos after binding)
	const FN_string* next(void*& iter_pos, const FN_ref*&) const;

	// add binding to set (fails if name already in set && excl nonzero)
	int add(const FN_string& name,
		const FN_ref&,
		unsigned int exclusive = FN_OP_EXCLUSIVE);

	// remove binding from set
	int remove(const FN_string& name);

protected:
	FN_bindingset_rep* rep;
	static FN_bindingset_rep* get_rep(const FN_bindingset&);
	FN_bindingset(FN_bindingset_rep*);
};

#endif /* _XFN_FN_BINDINGSET_HH */

/*
 * Copyright (c) 1993-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_ATTRSET_HH
#define	_XFN_FN_ATTRSET_HH

#pragma ident	"@(#)FN_attrset.hh	1.3	96/03/31 SMI"

#include <xfn/FN_attrset.h>
#include <xfn/FN_string.hh>
#include <xfn/FN_identifier.hh>
#include <xfn/FN_attribute.hh>
#include <xfn/misc_codes.h>

class FN_attrset_rep;

class FN_attrset {
public:
	FN_attrset();
	~FN_attrset();

	// copy and assignment
	FN_attrset(const FN_attrset&);
	FN_attrset& operator=(const FN_attrset&);

	// get value for specified attr
	const FN_attribute *get(const FN_identifier &attr_id) const;

	// get count of attrs
	unsigned int count(void) const;

	// get first attr (points iter_pos after attr)
	const FN_attribute *first(void *& iter_pos) const;

	// get attr after iter_pos (points iter_pos after attr)
	const FN_attribute *next(void *&iter_pos) const;

	// add attr to set (fails if attr already in set and exclusive nonzero)
	int add(const FN_attribute &attr,
		unsigned int exclusive = FN_OP_EXCLUSIVE);

	// remove attr from set
	int remove(const FN_identifier &attr);

protected:
	FN_attrset_rep *rep;
	static FN_attrset_rep *get_rep(const FN_attrset&);
	FN_attrset(FN_attrset_rep *);
};

#endif /* _XFN_FN_ATTRSET_HH */

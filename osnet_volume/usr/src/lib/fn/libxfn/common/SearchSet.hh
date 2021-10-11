/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SEARCHSET_HH
#define	_SEARCHSET_HH

#pragma ident	"@(#)SearchSet.hh	1.1	96/03/31 SMI"

#include <xfn/FN_string.hh>
#include <xfn/FN_ref.hh>
#include <xfn/FN_attrset.hh>
#include "Set.hh"

class SearchSetItem : public SetItem {
    public:
	FN_string name;
	FN_ref *ref;
	FN_attrset *attrs;

	SearchSetItem(const FN_string &name, const FN_ref *ref,
	    const FN_attrset *attrs);
	~SearchSetItem();
	SetItem *copy();
	int key_match(const void *key, int case_sense);
	int item_match(SetItem &, int case_sense);
	unsigned long key_hash();
	unsigned long key_hash(const void *);
};

#endif /* _SEARCHSET_HH */

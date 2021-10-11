/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _EXTSEARCHSET_HH
#define	_EXTSEARCHSET_HH

#pragma ident	"@(#)ExtSearchSet.hh	1.1	96/03/31 SMI"

#include <xfn/FN_string.hh>
#include <xfn/FN_ref.hh>
#include <xfn/FN_attrset.hh>
#include "Set.hh"

class ExtSearchSetItem : public SetItem {
    public:
	FN_composite_name name;
	FN_ref *ref;
	FN_attrset *attrs;
	unsigned int relative;

	ExtSearchSetItem(const FN_composite_name &name, const FN_ref *ref,
	    const FN_attrset *attrs, unsigned int rel = 0);
	~ExtSearchSetItem();
	SetItem *copy();
	int key_match(const void *key, int case_sense);
	int item_match(SetItem &, int case_sense);
	unsigned long key_hash();
	unsigned long key_hash(const void *);
};

#endif /* _EXTSEARCHSET_HH */

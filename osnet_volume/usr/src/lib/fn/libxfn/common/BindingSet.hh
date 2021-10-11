/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 */

#ifndef _BINDINGSET_HH
#define	_BINDINGSET_HH

#pragma ident	"@(#)BindingSet.hh	1.4	96/03/31 SMI"

#include <xfn/FN_string.hh>
#include <xfn/FN_ref.hh>
#include "Set.hh"

class BindingSetItem : public SetItem {
    public:
	FN_string name;
	FN_ref ref;

	BindingSetItem(const FN_string &name, const FN_ref &ref);
	~BindingSetItem();
	SetItem *copy();
	int key_match(const void *key, int case_sense);
	int item_match(SetItem &, int case_sense);
	unsigned long key_hash();
	unsigned long key_hash(const void *);
};

#endif /* _BINDINGSET_HH */

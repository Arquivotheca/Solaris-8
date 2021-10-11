/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ATTRMODLIST_HH
#define	_ATTRMODLIST_HH

#pragma ident	"@(#)AttrModList.hh	1.3	96/03/31 SMI"

#include <xfn/FN_string.hh>
#include <xfn/FN_attribute.hh>

#include "List.hh"

class AttrModListItem : public ListItem {
    public:
	FN_attribute	attribute;	// Attribute
	unsigned int    attr_mod_op;    // operation to be performed

	AttrModListItem(const FN_attribute &,
			unsigned int);
	~AttrModListItem();
	ListItem* copy();
};

#endif /* _ATTRMODLIST_HH */

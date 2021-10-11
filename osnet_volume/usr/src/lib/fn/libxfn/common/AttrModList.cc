/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)AttrModList.cc	1.3 94/07/30 SMI"

#include "AttrModList.hh"


AttrModListItem::AttrModListItem(const FN_attribute &attr,
    unsigned int mod_op)
	: attribute(attr)
{
	attr_mod_op = mod_op;
}

AttrModListItem::~AttrModListItem()
{
}

ListItem *
AttrModListItem::copy()
{
	return (new AttrModListItem(attribute, attr_mod_op));
}

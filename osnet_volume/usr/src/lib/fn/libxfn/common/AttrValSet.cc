/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)AttrValSet.cc	1.4	96/03/31 SMI"

#include "AttrValSet.hh"
#include "hash.hh"


AttrValSetItem::AttrValSetItem(const FN_attrvalue &av)
	: attr_val(av)
{
}

AttrValSetItem::~AttrValSetItem()
{
}

SetItem *
AttrValSetItem::copy()
{
	return ((SetItem *)(new AttrValSetItem(attr_val)));
}

int
AttrValSetItem::key_match(const void *key, int /* case_sense */)
{
	return (attr_val == *(const FN_attrvalue *)key);
}

int
AttrValSetItem::item_match(SetItem &item, int /* case_sense */)
{
	return (attr_val == ((AttrValSetItem &)item).attr_val);
}

unsigned long
AttrValSetItem::key_hash()
{
	return (get_hashval(attr_val.contents(), attr_val.length()));
}

unsigned long
AttrValSetItem::key_hash(const void *key)
{
	const FN_attrvalue	*v;

	v = (const FN_attrvalue *)key;
	return (get_hashval(v->contents(), v->length()));
}

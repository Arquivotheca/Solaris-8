/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)AttrSet.cc	1.5	96/03/31 SMI"

#include "AttrSet.hh"
#include "hash.hh"


AttrSetItem::AttrSetItem(const FN_attribute &attr)
	: at(attr)
{
}

AttrSetItem::~AttrSetItem()
{
}

SetItem *
AttrSetItem::copy()
{
	return (new AttrSetItem(at));
}

int
AttrSetItem::key_match(const void *key, int /* case_sense */)
{
	return (*at.identifier() == *(const FN_identifier *)key);
}

int
AttrSetItem::item_match(SetItem &item, int /* case_sense */)
{
	return (*(at.identifier()) == (*((AttrSetItem &)item).at.identifier()));
}

unsigned long
AttrSetItem::key_hash()
{
	const FN_identifier	*id;

	id = at.identifier();
	return (get_hashval(id->info.contents, id->info.length));
}

unsigned long
AttrSetItem::key_hash(const void *p)
{
	const FN_identifier	*id;

	id = (const FN_identifier *)p;
	return (get_hashval(id->info.contents, id->info.length));
}

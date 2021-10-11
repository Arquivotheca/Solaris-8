/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)BindingSet.cc	1.4	96/03/31 SMI"

#include "BindingSet.hh"
#include "hash.hh"


BindingSetItem::BindingSetItem(const FN_string &n, const FN_ref &r)
	: name(n), ref(r)
{
}

BindingSetItem::~BindingSetItem()
{
}

SetItem *
BindingSetItem::copy()
{
	return ((SetItem *)(new BindingSetItem(name, ref)));
}

int
BindingSetItem::key_match(const void *key, int case_sense)
{
	return (name.compare(*(const FN_string *)key,
	    case_sense? FN_STRING_CASE_SENSITIVE: FN_STRING_CASE_INSENSITIVE)
	    == 0);
}

int
BindingSetItem::item_match(SetItem &item, int case_sense)
{
	return (name.compare(((BindingSetItem &)item).name,
	    case_sense? FN_STRING_CASE_SENSITIVE: FN_STRING_CASE_INSENSITIVE)
	    == 0);
}

unsigned long
BindingSetItem::key_hash()
{
	return (get_hashval_nocase((const char *)name.contents(),
	    name.bytecount()));
}

unsigned long
BindingSetItem::key_hash(const void *key)
{
	const FN_string		*n;

	n = (const FN_string *)key;
	return (get_hashval_nocase((const char *)n->contents(),
	    n->bytecount()));
}

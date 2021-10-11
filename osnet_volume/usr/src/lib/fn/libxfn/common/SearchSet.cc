/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)SearchSet.cc	1.2	96/09/07 SMI"

#include "SearchSet.hh"
#include "hash.hh"


SearchSetItem::SearchSetItem(const FN_string &n, const FN_ref *r,
	const FN_attrset *a)
	: name(n)
{
	ref = (r ? new FN_ref(*r) : 0);

	attrs = (a? new FN_attrset(*a) : 0);
}

SearchSetItem::~SearchSetItem()
{
	delete ref;
	delete attrs;
}

SetItem *
SearchSetItem::copy()
{
	return ((SetItem *)(new SearchSetItem(name, ref, attrs)));
}

int
SearchSetItem::key_match(const void *key, int case_sense)
{
	return (name.compare(*(const FN_string *)key,
	    case_sense? FN_STRING_CASE_SENSITIVE: FN_STRING_CASE_INSENSITIVE)
	    == 0);
}

int
SearchSetItem::item_match(SetItem &item, int case_sense)
{
	return (name.compare(((SearchSetItem &)item).name,
	    case_sense? FN_STRING_CASE_SENSITIVE: FN_STRING_CASE_INSENSITIVE)
	    == 0);
}

unsigned long
SearchSetItem::key_hash()
{
	return (get_hashval_nocase((const char *)name.contents(),
	    name.bytecount()));
}

unsigned long
SearchSetItem::key_hash(const void *key)
{
	const FN_string		*n;

	n = (const FN_string *)key;
	return (get_hashval_nocase((const char *)n->contents(),
	    n->bytecount()));
}

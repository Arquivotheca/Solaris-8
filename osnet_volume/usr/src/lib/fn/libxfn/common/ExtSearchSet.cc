/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ExtSearchSet.cc	1.2	96/09/09 SMI"

#include "ExtSearchSet.hh"
#include "hash.hh"


ExtSearchSetItem::ExtSearchSetItem(const FN_composite_name &n, const FN_ref *r,
	const FN_attrset *a, unsigned int rel)
: name(n), relative(rel)
{
	ref = (r ? new FN_ref(*r) : 0);

	attrs = (a? new FN_attrset(*a) : 0);
}

ExtSearchSetItem::~ExtSearchSetItem()
{
	delete ref;
	delete attrs;
}

SetItem *
ExtSearchSetItem::copy()
{
	return ((SetItem *)(new ExtSearchSetItem(name, ref, attrs)));
}

int
ExtSearchSetItem::key_match(const void *key, int /* case_sense */)
{
	return (name.is_equal(*(const FN_composite_name *)key));
}

int
ExtSearchSetItem::item_match(SetItem &item, int /* case_sense */)
{
	return (name.is_equal(((ExtSearchSetItem &)item).name));
}

unsigned long
ExtSearchSetItem::key_hash()
{
	FN_string *nstr = name.string();
	if (nstr == 0)
		return (0);
	unsigned long hashval = get_hashval_nocase(
	    (const char *)nstr->contents(), nstr->bytecount());
	delete nstr;
	return (hashval);
}

unsigned long
ExtSearchSetItem::key_hash(const void *key)
{
	const FN_composite_name	*n;
	n = (const FN_composite_name *)key;
	FN_string *nstr = n->string();
	if (nstr == 0)
		return (0);
	unsigned long hashval = get_hashval_nocase(
	    (const char *)nstr->contents(), nstr->bytecount());
	delete nstr;
	return (hashval);
}

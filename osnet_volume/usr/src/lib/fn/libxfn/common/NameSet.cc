/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)NameSet.cc	1.5	96/03/31 SMI"

#include "NameSet.hh"
#include "hash.hh"


NameSetItem::NameSetItem(const FN_string &n)
	: name(n)
{
}

NameSetItem::~NameSetItem()
{
}

SetItem *
NameSetItem::copy()
{
	return ((SetItem *)(new NameSetItem(name)));
}

int
NameSetItem::key_match(const void *key, int case_sense)
{
	return (name.compare(*(const FN_string *)key,
	    case_sense? FN_STRING_CASE_SENSITIVE: FN_STRING_CASE_INSENSITIVE)
	    == 0);
}

int
NameSetItem::item_match(SetItem &item, int case_sense)
{
	return (name.compare(((NameSetItem &)item).name,
	    case_sense? FN_STRING_CASE_SENSITIVE: FN_STRING_CASE_INSENSITIVE)
	    == 0);
}

unsigned long
NameSetItem::key_hash()
{
	return (get_hashval_nocase((const char *)name.contents(),
	    name.bytecount()));
}

unsigned long
NameSetItem::key_hash(const void *key)
{
	const FN_string		*n;

	n = (const FN_string *)key;
	return (get_hashval_nocase((const char *)n->contents(),
	    n->bytecount()));
}

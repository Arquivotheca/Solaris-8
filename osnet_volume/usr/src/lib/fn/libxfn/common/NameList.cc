/*
 * Copyright (c) 1992 - 1993 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)NameList.cc	1.2 94/07/31 SMI"

#include "NameList.hh"


NameListItem::NameListItem(const FN_string &n)
	: name(n)
{
}

NameListItem::NameListItem(const unsigned char *n)
	: name(n)
{
}

NameListItem::~NameListItem()
{
}

ListItem *
NameListItem::copy()
{
	return ((ListItem *)(new NameListItem(name)));
}

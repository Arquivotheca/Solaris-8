/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)AddressList.cc	1.2 94/07/30 SMI"

#include "AddressList.hh"

AddressListItem::AddressListItem(const FN_ref_addr &a)
	: addr(a)
{
}

AddressListItem::~AddressListItem()
{
}

ListItem *
AddressListItem::copy()
{
	return ((ListItem *)(new AddressListItem(addr)));
}

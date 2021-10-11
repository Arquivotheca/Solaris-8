/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ADDRESSLIST_HH
#define	_ADDRESSLIST_HH

#pragma ident	"@(#)AddressList.hh	1.3	96/03/31 SMI"

#include <xfn/FN_ref_addr.hh>

#include "List.hh"

class AddressListItem : public ListItem {
    public:
	FN_ref_addr addr;

	AddressListItem(const FN_ref_addr& a);
	~AddressListItem();
	ListItem* copy();
};

#endif /* _ADDRESSLIST_HH */

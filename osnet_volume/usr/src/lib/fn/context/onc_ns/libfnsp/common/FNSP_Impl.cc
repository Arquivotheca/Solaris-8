/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_Impl.cc	1.5	97/10/23 SMI"

#include <string.h>
#include "FNSP_Impl.hh"
#include "fnsp_utils.hh"
#include <xfn/FN_namelist_svc.hh>
#include <xfn/FN_bindinglist_svc.hh>

FNSP_Impl::FNSP_Impl(FNSP_Address *addr) {
	my_address = addr;
}

FNSP_Impl::~FNSP_Impl() {
	delete my_address;
}

int
FNSP_Impl::check_if_old_addr_present(const FN_ref &oref,
    const FN_ref &newref)
{
	void	*iter;
	const FN_ref_addr *a1, *a2;

	for (a2 = newref.first(iter); a2; a2 = newref.next(iter)) {
		if (is_this_address_type_p(*a2)) {
			// found a FNSP address in newref,
			// now look for one in the
			// old reference
			a1 = oref.first(iter);
			for (a1 = oref.first(iter); a1; a1 = oref.next(iter))
				if (is_this_address_type_p(*a1))
					break;
			if (!a1)
				return (0);
			// no FNSP Address in oref? should not happen
			break;
		}
	}

	// If no *this* address types present return not OK
	if (!a2)
		return (0);

	// no FNSP address in New Reference
	// now compare the two addresses to make sure they are identical
	if (a1->length() == a2->length() &&
	    (memcmp(a1->data(), a2->data(), a1->length()) == 0) &&
	    (*(a1->type()) == *(a2->type())))
		return (1);
	else
		return (0);
}

/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisFlatContext.cc	1.1	96/03/31 SMI"

#include "FNSP_nisFlatContext.hh"
#include "FNSP_nisAddress.hh"


//  A flat naming system is one in which only non-hierarchical names are bound.
//  All names in the naming system are bound in a single context.
//  These names are junctions.
//
//  The bindings of the junctions
//  are stored in the binding table of the flat context.
//  The reference bound to a flat name is the same as its nns.
//

FNSP_nisFlatContext::~FNSP_nisFlatContext()
{
	delete my_reference;
	delete ns_impl;
}

FNSP_nisFlatContext::FNSP_nisFlatContext(const FN_string &dirname,
    unsigned context_type)
{
	my_reference = FNSP_reference(FNSP_nis_address_type_name(),
	    dirname, context_type);
	nis_impl = new FNSP_nisImpl(new FNSP_nisAddress(dirname, context_type));
	ns_impl = nis_impl;
}

FNSP_nisFlatContext::FNSP_nisFlatContext(const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);
	nis_impl = new FNSP_nisImpl(new FNSP_nisAddress(from_ref));
	ns_impl = nis_impl;
}

FNSP_nisFlatContext::FNSP_nisFlatContext(const FN_ref_addr &from_addr,
    const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);
	nis_impl = new FNSP_nisImpl(new FNSP_nisAddress(from_addr));
	ns_impl = nis_impl;
}

FNSP_nisFlatContext*
FNSP_nisFlatContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    FN_status &stat)
{
	FNSP_nisFlatContext *answer = new FNSP_nisFlatContext(from_addr,
	    from_ref);

	if (answer && answer->my_reference != NULL &&
	    answer->ns_impl != NULL && answer->ns_impl->my_address != NULL)
		stat.set_success();
	else {
		if (answer) {
			delete answer;
			answer = 0;
		}
		stat.set(FN_E_INSUFFICIENT_RESOURCES);
	}
	return (answer);
}

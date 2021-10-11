/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_filesFlatContext.cc	1.2	97/10/21 SMI"

#include "FNSP_filesFlatContext.hh"
#include <FNSP_nisAddress.hh>

//  A flat naming system is one in which only non-hierarchical names are bound.
//  All names in the naming system are bound in a single context.
//  These names are junctions.
//
//  The bindings of the junctions
//  are stored in the binding table of the flat context.
//  The reference bound to a flat name is the same as its nns.
//
FNSP_filesFlatContext::~FNSP_filesFlatContext()
{
	delete my_reference;
	delete ns_impl;
}

FNSP_filesFlatContext::FNSP_filesFlatContext(const FN_string &dirname,
    unsigned context_type)
{
	my_reference = FNSP_reference(FNSP_files_address_type_name(),
	    dirname, context_type);
	files_impl = new FNSP_filesImpl(new FNSP_nisAddress(dirname,
	    context_type));
	ns_impl = files_impl;
}

FNSP_filesFlatContext::FNSP_filesFlatContext(const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);
	files_impl = new FNSP_filesImpl(new FNSP_nisAddress(from_ref));
	ns_impl = files_impl;
}

FNSP_filesFlatContext::FNSP_filesFlatContext(const FN_ref_addr &from_addr,
    const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);
	files_impl = new FNSP_filesImpl(new FNSP_nisAddress(from_addr));
	ns_impl = files_impl;
}

FNSP_filesFlatContext*
FNSP_filesFlatContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    FN_status &stat)
{
	FNSP_filesFlatContext *answer = new FNSP_filesFlatContext(from_addr,
	    from_ref);

	if (answer && answer->my_reference && answer->ns_impl &&
	    answer->ns_impl->my_address)
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

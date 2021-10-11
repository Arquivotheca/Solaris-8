/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisplusFlatContext.cc	1.11	96/04/02 SMI"

#include <sys/time.h>

#include "FNSP_nisplusFlatContext.hh"
#include "FNSP_nisplusImpl.hh"
#include "fnsp_internal.hh"

//  A flat naming system is one in which only non-hierarchical names are bound.
//  All names in the naming system are bound in a single context.
//  These names are junctions.
//
//  The bindings of the junctions
//  are stored in the binding table of the flat context.
//  The reference bound to a flat name is the same as its nns.
//

static const FN_composite_name empty_name((const unsigned char *)"");

FNSP_nisplusFlatContext::~FNSP_nisplusFlatContext()
{
	delete my_reference;
	delete ns_impl;
}

FNSP_nisplusFlatContext::FNSP_nisplusFlatContext(const FN_string &dirname,
    unsigned int auth, unsigned context_type)
: FN_ctx_svc(auth)
{
	my_reference = FNSP_reference(FNSP_nisplus_address_type_name(),
	    dirname, context_type);
	ns_impl = new FNSP_nisplusImpl(new FNSP_nisplus_address(dirname,
	    context_type, FNSP_normal_repr, auth));
}

FNSP_nisplusFlatContext::FNSP_nisplusFlatContext(const FN_ref &from_ref,
    unsigned int auth) : FN_ctx_svc(auth)
{
	my_reference = new FN_ref(from_ref);
	ns_impl = new FNSP_nisplusImpl(new FNSP_nisplus_address(from_ref,
	    auth));
}

FNSP_nisplusFlatContext::FNSP_nisplusFlatContext(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth)
: FN_ctx_svc(auth)
{
	my_reference = new FN_ref(from_ref);
	ns_impl = new FNSP_nisplusImpl(new FNSP_nisplus_address(from_addr,
	    auth));
}

FNSP_nisplusFlatContext*
FNSP_nisplusFlatContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth,
    FN_status &stat)
{
	FNSP_nisplusFlatContext *answer =
		new FNSP_nisplusFlatContext(from_addr, from_ref, auth);

	if (answer && answer->my_reference &&
	    answer->ns_impl && answer->ns_impl->my_address)
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

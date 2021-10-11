/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisWeakSlashContext.cc	1.1	96/03/31 SMI"

#include "FNSP_nisWeakSlashContext.hh"
#include "FNSP_nisImpl.hh"

//  A FNSP_nisWeakSlashContext is derived from FN_ctx_asvc_weak_dynamic class.
//  A naming system composed of FNSP_nisWeakSlashContext supports a hierarchical
//  namespace with a slash-separated left-to-right syntax.
//  By the time processing gets to a FNSP_nisWeakSlashContext,
//  it only needs to deal  with 'atomic names'.  'nns pointers' may be
//  associated with each atomic  name.
//
//  The FNSP_nisWeakSlashContext itself may have an associatd nns pointer;
//  in this case, its binding could be found under the reserved name "FNS_nns"
//  in the context.  The bindings of names associated with the atomic names
//  are stored in the binding table of the atomic context.
//

FNSP_nisWeakSlashContext::~FNSP_nisWeakSlashContext()
{
	delete my_reference;
	delete ns_impl;
}


FNSP_nisWeakSlashContext::FNSP_nisWeakSlashContext(const FN_ref_addr &from_addr,
    const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);
	ns_impl = new FNSP_nisImpl(new FNSP_nisAddress(from_addr));
}

FNSP_nisWeakSlashContext*
FNSP_nisWeakSlashContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    FN_status &stat)
{
	FNSP_nisWeakSlashContext *answer = new FNSP_nisWeakSlashContext(
	    from_addr, from_ref);

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

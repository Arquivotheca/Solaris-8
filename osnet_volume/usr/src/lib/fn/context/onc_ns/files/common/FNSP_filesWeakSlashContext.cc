/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_filesWeakSlashContext.cc	1.1	96/03/31 SMI"

#include "FNSP_filesWeakSlashContext.hh"
#include "FNSP_filesImpl.hh"

//  A FNSP_filesWeakSlashContext is derived from FN_ctx_asvc_weak_dynamic class.
//  A naming system composed of FNSP_filesWeakSlashContext
//  supports a hierarchical
//  namespace with a slash-separated left-to-right syntax.
//  By the time processing gets to a FNSP_filesWeakSlashContext,
//  it only needs to deal  with 'atomic names'.  'nns pointers' may be
//  associated with each atomic  name.
//
//  The FNSP_filesWeakSlashContext itself may have an associatd nns pointer;
//  in this case, its binding could be found under the reserved name "FNS_nns"
//  in the context.  The bindings of names associated with the atomic names
//  are stored in the binding table of the atomic context.
//

FNSP_filesWeakSlashContext::~FNSP_filesWeakSlashContext()
{
	delete my_reference;
	delete ns_impl;
}


FNSP_filesWeakSlashContext::FNSP_filesWeakSlashContext(
    const FN_ref_addr &from_addr,
    const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);
	ns_impl = new FNSP_filesImpl(new FNSP_nisAddress(from_addr));
}

FNSP_filesWeakSlashContext*
FNSP_filesWeakSlashContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    FN_status &stat)
{
	FNSP_filesWeakSlashContext *answer = new
	    FNSP_filesWeakSlashContext(from_addr, from_ref);

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

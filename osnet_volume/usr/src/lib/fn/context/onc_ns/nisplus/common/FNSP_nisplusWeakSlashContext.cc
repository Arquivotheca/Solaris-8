/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisplusWeakSlashContext.cc	1.12	96/03/31 SMI"

#include "FNSP_nisplusWeakSlashContext.hh"
#include "FNSP_nisplusImpl.hh"

FNSP_nisplusWeakSlashContext::~FNSP_nisplusWeakSlashContext()
{
	delete my_reference;
	delete ns_impl;
}


FNSP_nisplusWeakSlashContext::FNSP_nisplusWeakSlashContext(
    const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth)
: FN_ctx_svc(auth)
{
	my_reference = new FN_ref(from_ref);
	ns_impl = new FNSP_nisplusImpl(
	    new FNSP_nisplus_address(from_addr, auth));
}

FNSP_nisplusWeakSlashContext*
FNSP_nisplusWeakSlashContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth,
    FN_status &stat)
{
	FNSP_nisplusWeakSlashContext *answer =
		new FNSP_nisplusWeakSlashContext(from_addr, from_ref, auth);

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

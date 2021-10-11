/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisplusPrinternameContext.cc	1.2	96/04/04 SMI"

#include "FNSP_nisplusPrinternameContext.hh"
#include "FNSP_nisplusImpl.hh"

FNSP_nisplusPrinternameContext::~FNSP_nisplusPrinternameContext()
{
	delete ns_impl;
}

FNSP_nisplusPrinternameContext::FNSP_nisplusPrinternameContext(
    const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    unsigned int auth)
: FNSP_PrinternameContext(from_addr, from_ref, auth), FN_ctx_svc(auth)
{
	ns_impl =
	    new FNSP_nisplusImpl(new FNSP_nisplus_address(from_addr, auth));
}

FNSP_nisplusPrinternameContext*
FNSP_nisplusPrinternameContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth,
    FN_status &stat)
{
	FNSP_nisplusPrinternameContext *answer =
	    new FNSP_nisplusPrinternameContext(from_addr, from_ref, auth);

	if ((answer) && (answer->my_reference))
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

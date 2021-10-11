/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisplusPrinterObject.cc	1.2	96/04/04 SMI"

#include "FNSP_nisplusPrinterObject.hh"
#include "FNSP_nisplusImpl.hh"

FNSP_nisplusPrinterObject::~FNSP_nisplusPrinterObject()
{
	delete my_reference;
	delete ns_impl;
}

FNSP_nisplusPrinterObject::FNSP_nisplusPrinterObject(
    const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    unsigned int auth)
: FN_ctx_svc(auth)
{
	my_reference = new FN_ref(from_ref);
	ns_impl =
	    new FNSP_nisplusImpl(new FNSP_nisplus_address(from_addr, auth));
}

FNSP_nisplusPrinterObject*
FNSP_nisplusPrinterObject::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth,
    FN_status &stat)
{
	FNSP_nisplusPrinterObject *answer =
	    new FNSP_nisplusPrinterObject(from_addr, from_ref, auth);

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

/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisPrinterObject.cc	1.1	96/03/31 SMI"

#include "FNSP_nisImpl.hh"
#include "FNSP_nisPrinterObject.hh"

FNSP_nisPrinterObject::~FNSP_nisPrinterObject()
{
	delete my_reference;
	delete ns_impl;
}

#ifdef DEBUG
FNSP_nisPrinterObject::FNSP_nisPrinterObject(const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);
	ns_impl = new FNSP_nisImpl(new FNSP_nisAddress(from_ref));
}
#endif

FNSP_nisPrinterObject::FNSP_nisPrinterObject(
    const FN_ref_addr &from_addr,
    const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);
	ns_impl = new FNSP_nisImpl(new FNSP_nisAddress(from_addr));
}

FNSP_nisPrinterObject*
FNSP_nisPrinterObject::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    FN_status &stat)
{
	FNSP_nisPrinterObject *answer =
	    new FNSP_nisPrinterObject(from_addr, from_ref);

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

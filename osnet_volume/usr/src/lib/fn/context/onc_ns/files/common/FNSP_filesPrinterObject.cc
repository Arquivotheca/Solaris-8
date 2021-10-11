/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_filesPrinterObject.cc	1.1	96/03/31 SMI"

#include "FNSP_filesImpl.hh"
#include "FNSP_filesPrinterObject.hh"

FNSP_filesPrinterObject::~FNSP_filesPrinterObject()
{
	delete my_reference;
	delete ns_impl;
}

#ifdef DEBUG
FNSP_filesPrinterObject::FNSP_filesPrinterObject(const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);
	ns_impl = new FNSP_filesImpl(new FNSP_nisAddress(from_ref));
}
#endif

FNSP_filesPrinterObject::FNSP_filesPrinterObject(
    const FN_ref_addr &from_addr,
    const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);
	ns_impl = new FNSP_filesImpl(new FNSP_nisAddress(from_addr));
}

FNSP_filesPrinterObject*
FNSP_filesPrinterObject::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    FN_status &stat)
{
	FNSP_filesPrinterObject *answer =
	    new FNSP_filesPrinterObject(from_addr, from_ref);

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

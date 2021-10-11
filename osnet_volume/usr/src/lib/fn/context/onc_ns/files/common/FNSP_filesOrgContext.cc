/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_filesOrgContext.cc	1.1	96/03/31 SMI"

#include "FNSP_filesOrgContext.hh"
#include "FNSP_filesImpl.hh"
#include <xfn/fn_p.hh>

FNSP_filesOrgContext::~FNSP_filesOrgContext()
{
	delete my_reference;
	delete ns_impl;
}

FNSP_filesOrgContext::FNSP_filesOrgContext(const FN_string &dirname)
{
	my_reference = FNSP_reference(FNSP_files_address_type_name(),
	    dirname, FNSP_organization_context);

	ns_impl = new FNSP_filesImpl(new FNSP_nisAddress(dirname,
	    FNSP_organization_context));
}

FNSP_filesOrgContext::FNSP_filesOrgContext(const FN_ref &from_ref)
{
	ns_impl = new FNSP_filesImpl(new FNSP_nisAddress(from_ref));

	my_reference = new FN_ref(from_ref);
}

FNSP_filesOrgContext::FNSP_filesOrgContext(const FN_ref_addr &from_addr,
    const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);

	ns_impl = new FNSP_filesImpl(new FNSP_nisAddress(from_addr));
}

FNSP_filesOrgContext*
FNSP_filesOrgContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    FN_status &stat)
{
	FNSP_filesOrgContext *answer = new FNSP_filesOrgContext(from_addr,
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

FNSP_Impl *
FNSP_filesOrgContext::get_nns_impl(const FN_ref &ref,
	unsigned &status,
	FN_string **dirname_holder)
{
	FN_string *nnsobjname = get_nns_objname(ref, status, dirname_holder);
	if (nnsobjname) {
		FNSP_nisAddress *nnsaddr =
			new FNSP_nisAddress(*nnsobjname,
					    FNSP_nsid_context,
					    FNSP_normal_repr);
		delete nnsobjname;
		status = FN_SUCCESS;
		return (new FNSP_filesImpl(nnsaddr));
	}
	return (0);
}

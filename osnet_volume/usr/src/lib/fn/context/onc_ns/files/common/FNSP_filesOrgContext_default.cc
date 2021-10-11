/*
 * Copyright (c) 1994-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_filesOrgContext_default.cc	1.1	96/03/31 SMI"

#include "FNSP_filesOrgContext_default.hh"
#include <xfn/fn_p.hh>
#include "FNSP_filesImpl.hh"

FNSP_filesOrgContext_default::~FNSP_filesOrgContext_default()
{
}

#ifdef DEBUG
FNSP_filesOrgContext_default::FNSP_filesOrgContext_default(
    const FN_identifier *addr_type, const FN_string &dirname)
: FNSP_nisOrgContext_default(addr_type, dirname)
{
}

FNSP_filesOrgContext_default::FNSP_filesOrgContext_default(
    const FN_ref &from_ref) : FNSP_nisOrgContext_default(from_ref)
{
}
#endif /* DEBUG */

FNSP_filesOrgContext_default::FNSP_filesOrgContext_default(
    const FN_ref_addr &from_addr, const FN_ref &from_ref)
: FNSP_nisOrgContext_default(from_addr, from_ref)
{
}

FNSP_filesOrgContext_default*
FNSP_filesOrgContext_default::from_address(
    const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    FN_status &stat)
{
	FNSP_filesOrgContext_default *answer =
	    new FNSP_filesOrgContext_default(from_addr, from_ref);

	if (answer && answer->my_reference)
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

FN_ref *
FNSP_filesOrgContext_default::make_nsid_ref()
{
	FN_string* orgname =
		FNSP_reference_to_internal_name(*my_reference);

	if (orgname) {
		FN_ref *answer = FNSP_reference(FNSP_files_address_type_name(),
		    *orgname, FNSP_nsid_context);
		delete orgname;
		return (answer);
	}
	return (0);
}

FN_ref *
FNSP_filesOrgContext_default::make_service_ref()
{
	FN_string* orgname =
		FNSP_reference_to_internal_name(*my_reference);

	if (orgname) {
		FN_ref *answer = FNSP_reference(FNSP_files_address_type_name(),
		    *orgname, FNSP_service_context);
		delete orgname;
		return (answer);
	}
	return (0);
}

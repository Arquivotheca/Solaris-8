/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisOrgContext.cc	1.1	96/03/31 SMI"

#include "FNSP_nisOrgContext.hh"
#include "FNSP_nisImpl.hh"
#include <xfn/fn_p.hh>

static const FN_string empty_name((unsigned char *)"");

FNSP_nisOrgContext::~FNSP_nisOrgContext()
{
	delete my_reference;
	delete ns_impl;
}

FNSP_nisOrgContext::FNSP_nisOrgContext(const FN_string &dirname)
{
	my_reference = FNSP_reference(FNSP_nis_address_type_name(),
	    dirname, FNSP_organization_context);

	ns_impl = new FNSP_nisImpl(new FNSP_nisAddress(dirname,
	    FNSP_organization_context));
}

FNSP_nisOrgContext::FNSP_nisOrgContext(const FN_ref &from_ref)
{
	ns_impl = new FNSP_nisImpl(new FNSP_nisAddress(from_ref));

	my_reference = new FN_ref(from_ref);
}

FNSP_nisOrgContext::FNSP_nisOrgContext(const FN_ref_addr &from_addr,
    const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);

	ns_impl = new FNSP_nisImpl(new FNSP_nisAddress(from_addr));
}

FNSP_nisOrgContext*
FNSP_nisOrgContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    FN_status &stat)
{
	FNSP_nisOrgContext *answer = new FNSP_nisOrgContext(from_addr,
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

FN_ref *
FNSP_nisOrgContext::resolve(const FN_string &name, FN_status_csvc &cstat)
{
	int stat_set = 0;
	FN_status stat;
	FN_ref *answer = 0;
	FN_string *domainname = FNSP_reference_to_internal_name(*my_reference);
	if (name.is_empty() ||
	    name.compare(*domainname, FN_STRING_CASE_INSENSITIVE) == 0) {
		// No name was given; resolves to current reference of context
		answer = new FN_ref(*my_reference);
		if (answer)
			cstat.set_success();
		else
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
	} else {
		cstat.set_error(FN_E_NAME_NOT_FOUND, *my_reference, name);
	}
	delete domainname;
	return (answer);
}

// == Lookup (name/)
// %%% cannot be linked (reference generated algorithmically)
// %%% If supported, must store link somewhere and change
// %%% entire ctx implementation, which depends on non-linked repr
// %%%
FN_ref *
FNSP_nisOrgContext::c_lookup_nns(const FN_string &name,
    unsigned int, /* lookup_flags */
    FN_status_csvc& cstat)
{
	FN_ref *answer = 0;
	unsigned status;

	FN_string *domainname = FNSP_reference_to_internal_name(*my_reference);
	if (name.is_empty() ||
	    (name.compare(*domainname, FN_STRING_CASE_INSENSITIVE) == 0)) {
		FNSP_Impl *nns_impl = get_nns_impl(*my_reference, status);
		if (nns_impl != 0) {
			unsigned estatus = nns_impl->context_exists();
			switch (estatus) {
			case FN_SUCCESS:
				answer = nns_impl->get_nns_ref();
				if (answer == 0)
					cstat.set(
					    FN_E_INSUFFICIENT_RESOURCES);
				else
					cstat.set_success();
				break;
			default:
				cstat.set_error(FN_E_NAME_NOT_FOUND,
				    *my_reference, name);
			}
			delete nns_impl;
		} else
			cstat.set_error(status, *my_reference, name);
	} else
		cstat.set_error(FN_E_NAME_NOT_FOUND, *my_reference, name);

	delete domainname;
	return (answer);
}

FNSP_Impl *
FNSP_nisOrgContext::get_nns_impl(const FN_ref &ref,
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
		return (new FNSP_nisImpl(nnsaddr));
	}
	return (0);
}

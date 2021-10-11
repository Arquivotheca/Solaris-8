/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisplusOrgContext.cc	1.9	96/03/31 SMI"

#include "FNSP_nisplusOrgContext.hh"
#include "FNSP_nisplusImpl.hh"
#include "fnsp_internal.hh"
#include "FNSP_nisplus_address.hh"

static const FN_string empty_name((unsigned char *)"");

#define	CAREFUL

FNSP_nisplusOrgContext::~FNSP_nisplusOrgContext()
{
	delete my_reference;
	delete ns_impl;
}

FNSP_nisplusOrgContext::FNSP_nisplusOrgContext(const FN_string &dirname,
    unsigned int auth)
: FN_ctx_svc(auth)
{
	my_reference = FNSP_reference(FNSP_nisplus_address_type_name(),
	    dirname, FNSP_organization_context);

	ns_impl = new FNSP_nisplusImpl(new FNSP_nisplus_address(dirname,
	    FNSP_organization_context, auth));
}

FNSP_nisplusOrgContext::FNSP_nisplusOrgContext(const FN_ref &from_ref,
    unsigned int auth)
: FN_ctx_svc(auth)
{
	ns_impl = new FNSP_nisplusImpl(new FNSP_nisplus_address(from_ref,
	    auth));

	my_reference = new FN_ref(from_ref);
}

FNSP_nisplusOrgContext::FNSP_nisplusOrgContext(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth)
: FN_ctx_svc(auth)
{
	my_reference = new FN_ref(from_ref);

	ns_impl = new FNSP_nisplusImpl(new FNSP_nisplus_address(from_addr,
	    auth));
}

FNSP_nisplusOrgContext*
FNSP_nisplusOrgContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth,
    FN_status &stat)
{
	FNSP_nisplusOrgContext *answer = new
		FNSP_nisplusOrgContext(from_addr, from_ref, auth);

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
FNSP_nisplusOrgContext::resolve(const FN_string &name, FN_status_csvc &cstat)
{
	int stat_set = 0;
	unsigned status;
	FN_status stat;
	FN_ref *answer;

	if (name.is_empty()) {
		// No name was given; resolves to current reference of context
		answer = new FN_ref(*my_reference);
		if (answer)
			cstat.set_success();
		else
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
	} else {
		answer = FNSP_resolve_orgname(
		    ns_impl->my_address->get_internal_name(),
		    name, ns_impl->my_address->get_access_flags(),
		    status, stat, stat_set);
		if (status == FN_SUCCESS)
			cstat.set_success();
		else if (stat_set == 0) {
			cstat.set_error(status, *my_reference, name);
		} else {
			// convert FN_status to FN_status_csvc
			FN_string *rname;
			rname = stat.remaining_name()->string();
			cstat.set_error(stat.code(), *(stat.resolved_ref()),
			    *rname);
		}
	}
	return (answer);
}

FN_namelist*
FNSP_nisplusOrgContext::c_list_names(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *ref = resolve(name, cstat);
	unsigned status;
	FN_nameset* answer = 0;

	if (cstat.is_success()) {
		FN_string *dirname = FNSP_reference_to_internal_name(*ref);
		if (dirname) {
			answer = FNSP_list_orgnames(*dirname,
			    ns_impl->my_address->get_access_flags(), status);
			delete dirname;
			if (status != FN_SUCCESS)
				cstat.set_error(status, *ref, empty_name);
		} else // ??? resolved but not to a FNSP reference?
			cstat.set_error(status, *ref, empty_name);
		if (ref)
			delete ref;
	}

	if (answer)
		return (new FN_namelist_svc(answer));
	else
		return (0);
}

FN_bindinglist*
FNSP_nisplusOrgContext::c_list_bindings(const FN_string &name,
	FN_status_csvc &cstat)
{
	FN_ref *ref = resolve(name, cstat);
	FN_status stat;
	unsigned status;
	FN_bindingset* answer = 0;

	if (cstat.is_success()) {
		FN_string *dirname = FNSP_reference_to_internal_name(*ref);
		if (dirname) {
			answer = FNSP_list_orgbindings(*dirname,
			    ns_impl->my_address->get_access_flags(), status);
			if (status != FN_SUCCESS)
				cstat.set_error(status, *ref, empty_name);
			delete dirname;
		} else
			// ??? resolved but not to a FNSP reference?
			// probably corrupted
			cstat.set_error(FN_E_MALFORMED_REFERENCE, *ref,
			    empty_name);
		if (ref)
			delete ref;
	}

	if (answer)
		return (new FN_bindinglist_svc(answer));
	else
		return (0);
}


// == Lookup (name:)
// %%% cannot be linked (reference generated algorithmically)
// %%% If supported, must store link somewhere and change
// %%% entire ctx implementation, which depends on non-linked repr
// %%%
FN_ref *
FNSP_nisplusOrgContext::c_lookup_nns(const FN_string &name,
    unsigned int, /* lookup_flags */
    FN_status_csvc& cstat)
{
	FN_ref *ref = resolve(name, cstat);
	FN_ref *answer = 0;
	unsigned status = FN_SUCCESS;

	if (cstat.is_success()) {
		// found name; now look for nns
		FNSP_Impl *nns_impl = get_nns_impl(*ref, status);
		if (nns_impl != 0) {
#ifdef CAREFUL
			status = nns_impl->context_exists();
			switch (status) {
			case FN_SUCCESS:
				break;
			case FN_E_NOT_A_CONTEXT:
				// %%% was context_not_found
				status = FN_E_NAME_NOT_FOUND;
			default:
				cstat.set_error(status, *ref, empty_name);
			}
#endif
			if (status == FN_SUCCESS) {
				answer = nns_impl->get_nns_ref();
				if (answer == 0)
					cstat.set(FN_E_INSUFFICIENT_RESOURCES);
				else
					cstat.set_success();
			}
			delete nns_impl;
		} else {
			cstat.set_error(status, *ref, empty_name);
		}
	}
	delete ref;
	return (answer);
}

FN_bindinglist*
FNSP_nisplusOrgContext::c_list_bindings_nns(const FN_string &name,
    FN_status_csvc& cstat)
{
	FN_ref *ref = resolve(name, cstat);
	unsigned status;

	if (cstat.is_success()) {
		FNSP_Impl *nns_impl = get_nns_impl(*ref, status);
		if (nns_impl == 0) {
			cstat.set_error(status, *ref, empty_name);
		} else {
			FN_ref *nns_ref;
#ifndef CAREFUL
			status = FN_SUCCESS;
#else
			status = nns_impl->context_exists();
#endif
			switch (status) {
			case FN_SUCCESS:
				nns_ref = nns_impl->get_nns_ref();
				if (nns_ref == 0)
					cstat.set(FN_E_INSUFFICIENT_RESOURCES);
				else {
					cstat.set(FN_E_SPI_CONTINUE,
					    nns_ref, 0, 0);
					delete nns_ref;
				}
				break;
			default:
				cstat.set_error(status, *ref, empty_name);
			}
			delete nns_impl;
		}
	}
	delete ref;
	return (0);
}


FNSP_Impl *
FNSP_nisplusOrgContext::get_nns_impl(const FN_ref &ref,
	unsigned &status,
	FN_string **dirname_holder)
{
	FN_string *nnsobjname = get_nns_objname(ref, status, dirname_holder);
	if (nnsobjname) {
		FNSP_nisplus_address *nnsaddr =
			new FNSP_nisplus_address(*nnsobjname,
						    FNSP_nsid_context,
						    FNSP_normal_repr,
						    authoritative);
		delete nnsobjname;
		status = FN_SUCCESS;
		return (new FNSP_nisplusImpl(nnsaddr));
	}
	return (0);
}

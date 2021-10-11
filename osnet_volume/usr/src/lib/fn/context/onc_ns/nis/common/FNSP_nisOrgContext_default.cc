/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisOrgContext_default.cc	1.1	96/03/31 SMI"

#include "FNSP_nisOrgContext_default.hh"
#include <FNSP_Syntax.hh>
#include "xfn/fn_p.hh"

static const FN_string FNSP_Service_Name((unsigned char *)"service");
static const FN_string FNSP_Service_Name2((unsigned char *)"_service");

FNSP_nisOrgContext_default::~FNSP_nisOrgContext_default()
{
	if (my_reference) delete my_reference;
}

// check for null pointers to my_reference and my_address in constructors

#ifdef DEBUG
FNSP_nisOrgContext_default::FNSP_nisOrgContext_default(
    const FN_identifier *addr_type, const FN_string &dirname)
{
	my_reference = FNSP_reference(*addr_type,
	    dirname, FNSP_organization_context);
}

FNSP_nisOrgContext_default::FNSP_nisOrgContext_default(const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);
}
#endif /* DEBUG */

FNSP_nisOrgContext_default::FNSP_nisOrgContext_default(
    const FN_ref_addr & /* from_addr */, const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);
}

FNSP_nisOrgContext_default* FNSP_nisOrgContext_default::from_address(
    const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    FN_status &stat)
{
	FNSP_nisOrgContext_default *answer = new
	    FNSP_nisOrgContext_default(from_addr, from_ref);

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

const FN_ref*
FNSP_nisOrgContext_default::resolve(const FN_string &name,
    FN_status_csvc &cstat)
{
	const FN_ref *answer = 0;

	if (name.is_empty()) {
		// No name was given; resolves to current reference of context
		answer = my_reference;
		cstat.set_success();
	} else {
		// We do not support sub-organizations in files/nis
		cstat.set_error(FN_E_NAME_NOT_FOUND, *my_reference, name);
	}
	return (answer);
}

FN_ref*
FNSP_nisOrgContext_default::c_lookup(const FN_string &name, unsigned int,
	FN_status_csvc &stat)
{
	const FN_ref *answer = resolve(name, stat);
	if (answer)
		return (new FN_ref(*answer));
	else
		return (0);
}

FN_namelist*
FNSP_nisOrgContext_default::c_list_names(const FN_string &name,
    FN_status_csvc &cstat)
{
	if (resolve(name, cstat)) {
		FN_nameset *answer = new FN_nameset;
		if (answer == 0)
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
		else {
			cstat.set_success();
			return (new FN_namelist_svc(answer));
		}
	}
	return (0);
}

FN_bindinglist*
FNSP_nisOrgContext_default::c_list_bindings(const FN_string &name,
    FN_status_csvc &cstat)
{
	if (resolve(name, cstat)) {
		FN_bindingset* answer = new FN_bindingset;
		if (answer == 0)
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
		else {
			cstat.set_success();
			return (new FN_bindinglist_svc(answer));
		}
	}
	return (0);
}

FN_attrset*
FNSP_nisOrgContext_default::c_get_syntax_attrs(const FN_string &name,
	FN_status_csvc &cstat)
{
	const FN_ref *ref = resolve(name, cstat);

	if (cstat.is_success()) {
		// No suborganizations allowed;
		// Flat context
		FN_attrset* answer =
		    FNSP_Syntax(FNSP_nsid_context)->get_syntax_attrs();
		if (answer)
			return (answer);
		cstat.set(FN_E_INSUFFICIENT_RESOURCES);
	}
	return (0);
}

// lookup("/")
FN_ref*
FNSP_nisOrgContext_default::c_lookup_nns(const FN_string &name,
	unsigned int, /* lookup_flags */ FN_status_csvc &cstat)
{
	if (resolve(name, cstat)) {
		// if we are here, name can only be ""
		// found name; now look for nns
		FN_ref *answer = make_nsid_ref();
		if (answer == 0)
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
		else
			return (answer);
	}
	return (0);
}

// Return names of bindings associated with org//
FN_namelist *
FNSP_nisOrgContext_default::c_list_names_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	if (resolve(name, cstat)) {
		// if we are here, name can only be ""
		FN_nameset* answer = new FN_nameset;
		if (answer == 0) {
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
			return (0);
		}
		answer->add(FNSP_Service_Name);
		answer->add(FNSP_Service_Name2);
		return (new FN_namelist_svc(answer));
	}

	return (0);
}

// Return bindings, in reality only "service" associated with org//
FN_bindinglist*
FNSP_nisOrgContext_default::c_list_bindings_nns(const FN_string &name,
			FN_status_csvc &cstat)
{
	if (resolve(name, cstat)) {
		// if we are here, name can only be ""
		FN_bindingset *bs = new FN_bindingset;

		if (bs == 0) {
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
			return (0);
		}

		FN_ref *ref = make_service_ref();
		if (ref == 0) {
			delete bs;
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
			return (0);
		}

		bs->add(FNSP_Service_Name, *ref);
		bs->add(FNSP_Service_Name2, *ref);
		delete ref;
		return (new FN_bindinglist_svc(bs));
	}
	return (0);
}

FN_attrset*
FNSP_nisOrgContext_default::c_get_syntax_attrs_nns(const FN_string &name,
	FN_status_csvc &cstat)
{
	FN_ref* nns_ref = c_lookup_nns(name, 0, cstat);

	if (cstat.is_success()) {
		FN_attrset* answer =
		    FNSP_Syntax(FNSP_nsid_context)->get_syntax_attrs();
		delete nns_ref;
		if (answer)
			return (answer);
		cstat.set(FN_E_INSUFFICIENT_RESOURCES);
	}
	return (0);
}

static const FN_identifier
FNSP_default_nis_address_type((unsigned char *) "onc_fn_nis");

FN_ref *
FNSP_nisOrgContext_default::make_nsid_ref()
{
	FN_string* orgname =
	    FNSP_reference_to_internal_name(*my_reference);

	if (orgname) {
		FN_ref *answer = FNSP_reference(FNSP_default_nis_address_type,
		    *orgname, FNSP_nsid_context);
		delete orgname;
		return (answer);
	}
	return (0);
}

FN_ref *
FNSP_nisOrgContext_default::make_service_ref()
{
	FN_string* orgname =
	    FNSP_reference_to_internal_name(*my_reference);

	if (orgname) {
		FN_ref *answer = FNSP_reference(FNSP_default_nis_address_type,
		    *orgname, FNSP_service_context);
		delete orgname;
		return (answer);
	}
	return (0);
}

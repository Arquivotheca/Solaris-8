/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisPrinternameContext.cc	1.8	99/05/05 SMI"

#include <sys/types.h>
#include <rpcsvc/ypclnt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <xfn/fn_p.hh>
#include "FNSP_nisPrinternameContext.hh"
#include "FNSP_nisImpl.hh"
#include "fnsp_nis_internal.hh"
#include "fnsp_internal_common.hh"


static FN_string null_name((unsigned char *) "");
static FN_string internal_name((unsigned char *) "printers");

FNSP_nisPrinternameContext::~FNSP_nisPrinternameContext()
{
	delete domain_name;
	delete ns_impl;
}

FNSP_nisPrinternameContext::FNSP_nisPrinternameContext(
    const FN_ref_addr &from_addr, const FN_ref &from_ref)
	: FNSP_PrinternameContext(from_addr, from_ref, 0)
{
	// check if fns is installed
	FNSP_nisAddress *my_address = new FNSP_nisAddress(from_addr);
	if (FNSP_is_fns_installed(&from_addr)) {
		FNSP_nisImpl *nis_impl = new FNSP_nisImpl(my_address);
		ns_impl = nis_impl;
		FN_string *map;
		// Check if it a context or binding
		char *domain;
		yp_get_default_domain(&domain);
		if (strcmp(domain, (char *)
		    (my_address->get_internal_name()).str())
		    == 0) {
			domain_name = new FN_string((unsigned char *)
			    domain);
			fns_org_context = 1;
		} else {
			FNSP_nis_split_internal_name(my_address->get_table_name(),
			    &map, &domain_name);
			delete map;
			fns_org_context = nis_impl->is_org_context();
		}
	} else {
		domain_name = new FN_string(my_address->get_internal_name());
		delete my_address;
		ns_impl = 0;
		fns_org_context = 1;
	}
}

FNSP_nisPrinternameContext*
FNSP_nisPrinternameContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, FN_status &stat)
{
	FNSP_nisPrinternameContext *answer =
	    new FNSP_nisPrinternameContext(from_addr, from_ref);

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

FN_ref*
FNSP_nisPrinternameContext::resolve(const FN_string &aname,
    FN_status_csvc& cs)
{
	FN_ref *ref_ns = 0;

	// Lookup in NIS tables if FNS is installed
	if (fns_installed()) {
		unsigned status;

		ref_ns = ns_impl->lookup_binding(aname, status);
		if (status == FN_SUCCESS)
			cs.set_success();
	}

	if (!ref_ns) {
		cs.set_error(FN_E_NAME_NOT_FOUND, *my_reference, aname);
		return (0);
	}

	return (ref_ns);
}

FN_nameset*
FNSP_nisPrinternameContext::list(FN_status_csvc &cs)
{
	FN_nameset *ns;

	ns = new FN_nameset;
	cs.set_error(FN_E_PARTIAL_RESULT, *my_reference, null_name);

	return (ns);
}

FN_bindingset*
FNSP_nisPrinternameContext::list_bs(FN_status_csvc &cs)
{
	FN_bindingset *bs;

	bs = new FN_bindingset;
	cs.set_error(FN_E_PARTIAL_RESULT, *my_reference, null_name);

	return (bs);
}

int
FNSP_nisPrinternameContext::c_bind(const FN_string &name,
    const FN_ref &ref, unsigned excl, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED,
		    *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_bind(name, ref, excl, cs));
}

int
FNSP_nisPrinternameContext::c_unbind(const FN_string &name,
    FN_status_csvc& cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED,
		    *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_unbind(name, cs));
}

int
FNSP_nisPrinternameContext::c_rename(const FN_string &name,
    const FN_composite_name &newname, unsigned rflags,
    FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED,
		    *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_rename(name, newname,
	    rflags, cs));
}

FN_ref*
FNSP_nisPrinternameContext::c_create_subcontext(const FN_string &name,
    FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_create_subcontext(name, cs));
}

int
FNSP_nisPrinternameContext::c_destroy_subcontext(const FN_string &name,
    FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_destroy_subcontext(name, cs));
}


FN_attribute*
FNSP_nisPrinternameContext::c_attr_get(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_get(name, id, follow_link, cs));
}

int
FNSP_nisPrinternameContext::c_attr_modify(const FN_string &aname,
    unsigned int flags, const FN_attribute &attr,
    unsigned int follow_link, FN_status_csvc& cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED,
		    *my_reference, aname);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_modify(aname,
	    flags, attr, follow_link, cs));
}

FN_valuelist*
FNSP_nisPrinternameContext::c_attr_get_values(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_get_values(name,
	    id, follow_link, cs));
}

FN_attrset*
FNSP_nisPrinternameContext::c_attr_get_ids(const FN_string &name,
    unsigned int follow_link, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_get_ids(name,
	    follow_link, cs));
}

FN_multigetlist*
FNSP_nisPrinternameContext::c_attr_multi_get(const FN_string &name,
    const FN_attrset *attrset, unsigned int follow_link, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_multi_get(name,
	    attrset, follow_link, cs));
}

int
FNSP_nisPrinternameContext::c_attr_multi_modify(const FN_string &name,
    const FN_attrmodlist &modlist, unsigned int follow_link,
    FN_attrmodlist **un_modlist,
    FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_multi_modify(name,
	    modlist, follow_link, un_modlist, cs));
}


// Extended attibute operations
int
FNSP_nisPrinternameContext::c_attr_bind(const FN_string &name,
    const FN_ref &ref,
    const FN_attrset *attrs,
    unsigned BindFlags, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_bind(name, ref, attrs,
	    BindFlags, cs));
}

FN_ref *
FNSP_nisPrinternameContext::c_attr_create_subcontext(const FN_string &name,
    const FN_attrset *attrs, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_create_subcontext(
	    name, attrs, cs));
}

FN_searchlist *
FNSP_nisPrinternameContext::c_attr_search(const FN_string &name,
    const FN_attrset *match_attrs,
    unsigned int return_ref,
    const FN_attrset *return_attr_ids,
    FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_search(name,
	    match_attrs, return_ref, return_attr_ids, cs));
}

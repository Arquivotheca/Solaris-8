/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_filesPrinternameContext.cc	1.2	96/06/20 SMI"

#include "FNSP_filesPrinternameContext.hh"
#include "FNSP_filesImpl.hh"
#include "fnsp_files_internal.hh"

static FN_string null_name((unsigned char *) "");

FNSP_filesPrinternameContext::~FNSP_filesPrinternameContext()
{
	delete ns_impl;
}

FNSP_filesPrinternameContext::FNSP_filesPrinternameContext(
    const FN_ref_addr &from_addr, const FN_ref &from_ref)
	: FNSP_PrinternameContext(from_addr, from_ref, 0)
{
	// check if fns is installed
	if (FNSP_files_is_fns_installed(&from_addr)) {
		ns_impl = new FNSP_filesImpl(new FNSP_nisAddress(from_addr));
	} else {
		ns_impl = 0;
	}
}

FNSP_filesPrinternameContext*
FNSP_filesPrinternameContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, FN_status &stat)
{
	FNSP_filesPrinternameContext *answer =
	    new FNSP_filesPrinternameContext(from_addr, from_ref);

	if ((answer) && (answer->my_reference))
		stat.set_success();
	else {
		if (answer) {
			delete answer;
			answer = 0;
		}
		FN_composite_name empty_name(null_name);
		stat.set(FN_E_INSUFFICIENT_RESOURCES);
	}
	return (answer);
}

FN_ref*
FNSP_filesPrinternameContext::resolve(const FN_string &aname,
    FN_status_csvc& cs)
{
	return (FNSP_PrinternameContext::resolve(aname, cs));
}

int
FNSP_filesPrinternameContext::c_bind(const FN_string &name,
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
FNSP_filesPrinternameContext::c_unbind(const FN_string &name,
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
FNSP_filesPrinternameContext::c_rename(const FN_string &name,
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
FNSP_filesPrinternameContext::c_create_subcontext(const FN_string &name,
    FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED,
		    *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_create_subcontext(name, cs));
}

int
FNSP_filesPrinternameContext::c_destroy_subcontext(const FN_string &name,
    FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED,
		    *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_destroy_subcontext(name, cs));
}

FN_attribute *
FNSP_filesPrinternameContext::c_attr_get(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_get(name, id,
	    follow_link, cs));
}

int
FNSP_filesPrinternameContext::c_attr_modify(const FN_string &aname,
    unsigned int flags, const FN_attribute &attr,
    unsigned int follow_link, FN_status_csvc& cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference,
		    aname);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_modify(aname, flags,
	    attr, follow_link, cs));
}

FN_valuelist*
FNSP_filesPrinternameContext::c_attr_get_values(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (c_attr_get_values(name, id, follow_link, cs));
}

FN_attrset*
FNSP_filesPrinternameContext::c_attr_get_ids(const FN_string &name,
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
FNSP_filesPrinternameContext::c_attr_multi_get(const FN_string &name,
    const FN_attrset *attrset, unsigned int follow_link, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_multi_get(name, attrset,
	    follow_link, cs));
}

int
FNSP_filesPrinternameContext::c_attr_multi_modify(const FN_string &name,
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
FNSP_filesPrinternameContext::c_attr_bind(const FN_string &name,
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
FNSP_filesPrinternameContext::c_attr_create_subcontext(const FN_string &name,
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
FNSP_filesPrinternameContext::c_attr_search(const FN_string &name,
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

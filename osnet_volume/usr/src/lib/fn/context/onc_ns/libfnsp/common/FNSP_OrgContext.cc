/*
 * Copyright (c) 1992 - 1994-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_OrgContext.cc	1.4	97/09/02 SMI"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/systeminfo.h>
#include <xfn/fn_p.hh>
#include "FNSP_OrgContext.hh"
#include "FNSP_Impl.hh"
#include "FNSP_Syntax.hh"
#include "fnsp_utils.hh"

static const FN_string empty_name((unsigned char *)"");

FNSP_OrgContext::~FNSP_OrgContext()
{
	// clean up done by subclasses
}

FN_ref *
FNSP_OrgContext::get_ref(FN_status &stat) const
{
	stat.set_success();

	return (new FN_ref(*my_reference));
}

FN_composite_name *
FNSP_OrgContext::equivalent_name(const FN_composite_name &name,
    const FN_string &, FN_status &status)
{
	status.set(FN_E_OPERATION_NOT_SUPPORTED, my_reference, &name);
	return (0);
}

// Given reference for an organization,
// extract the directory name of the org from the reference,
// and construct the object name for the nns context associated
// with the org.
// If 'dirname_holder' is supplied, use it to return the directory name of org.
FN_string *
FNSP_OrgContext::get_nns_objname(const FN_ref &ref,
	unsigned &status,
	FN_string **dirname_holder)
{
	FN_string *dirname = FNSP_reference_to_internal_name(ref);
	FN_string *nnsobjname = 0;

	if (dirname) {
		nnsobjname = ns_impl->get_nns_objname(dirname);
		status = FN_SUCCESS;
	} else
		status = FN_E_MALFORMED_REFERENCE;
	if (dirname_holder)
		*dirname_holder = dirname;
	else
		delete dirname;
	return (nnsobjname);
}

// Attributes for Org context.
// Build the LDAP DN attribute
static FN_attrset *
get_attrset_for_org(const FN_ref &ref)
{
	FN_attrset *answer;
	FN_string *internal_name_string;
	char internal_name[1024];
	char dn_name[1024], *temp, *ptr;
	FN_attrvalue *attrvalue;
	FN_identifier attrid((unsigned char *) "onc_distinguished_name");
	FN_identifier attrsyntax((unsigned char *) "fn_attr_syntax_ascii");
	FN_attribute attribute(attrid, attrsyntax);

	// Construct the onc_distinguished_name attribute
	internal_name_string = FNSP_reference_to_internal_name(ref);
	if (internal_name_string)
		strcpy(internal_name, (char *) internal_name_string->str());
	if ((!internal_name_string) || (strcmp(internal_name, "") == 0))
		// No internal name, get from the domainname
		sysinfo(SI_SRPC_DOMAIN, internal_name, 1024);
	delete internal_name_string;
	for (int i = 0; i < strlen(internal_name); i++)
		internal_name[i] = toupper(internal_name[i]);

	// Construct the DN
	ptr = strtok_r(internal_name, ".", &temp);
	strcpy(dn_name, "DC=");
	strcat(dn_name, ptr);
	while (ptr = strtok_r(0, ".", &temp)) {
		strcat(dn_name, ", DC=");
		strcat(dn_name, ptr);
	}
	// Finally add O=INTERNET
	strcat(dn_name, ", O=INTERNET");

	// Construct the DN attrvalue
	attrvalue = new FN_attrvalue(dn_name, strlen(dn_name));
	attribute.add(*attrvalue);
	delete attrvalue;

	// Construct the attrset
	answer = new FN_attrset;
	answer->add(attribute);
	return (answer);
}

FN_ref *
FNSP_OrgContext::resolve(const FN_string &name, FN_status_csvc &cstat)
{
	int stat_set = 0;
	FN_status stat;
	FN_ref *answer = 0;

	if (name.is_empty()) {
		// No name was given; resolves to current reference of context
		answer = new FN_ref(*my_reference);
		if (answer)
			cstat.set_success();
		else
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
	} else {
		cstat.set_error(FN_E_NAME_NOT_FOUND, *my_reference, name);
	}
	return (answer);
}

FN_ref *
FNSP_OrgContext::c_lookup(const FN_string &name, unsigned int,
    FN_status_csvc &stat)
{
	return (resolve(name, stat));
}

FN_namelist*
FNSP_OrgContext::c_list_names(const FN_string &name, FN_status_csvc &cstat)
{
	// default implementation is to list the empty set
	FN_nameset* answer = 0;
	FN_ref *ref = resolve(name, cstat);
	if (cstat.is_success()) {
		answer = new FN_nameset;
		if (answer == 0)
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
		else
			cstat.set_success();
	}
	delete ref;
	if (answer)
		return (new FN_namelist_svc(answer));
	else
		return (0);
}

FN_bindinglist*
FNSP_OrgContext::c_list_bindings(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *ref = resolve(name, cstat);
	FN_bindingset* answer = 0;
	if (cstat.is_success()) {
		answer = new FN_bindingset;
		if (answer == 0)
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
		else
			cstat.set_success();
	}
	delete ref;
	if (answer)
		return (new FN_bindinglist_svc(answer));
	else
		return (0);
}

int
FNSP_OrgContext::c_bind(const FN_string &name, const FN_ref &,
    unsigned, FN_status_csvc &cstat)
{
	/* not supported for ORG */
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_OrgContext::c_unbind(const FN_string &name, FN_status_csvc &cstat)
{
	/* not supported for ORG */
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


int
FNSP_OrgContext::c_rename(const FN_string &name, const FN_composite_name &,
    unsigned, FN_status_csvc &cstat)
{
	/* not supported for ORG */
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


FN_ref *
FNSP_OrgContext::c_create_subcontext(const FN_string &name,
    FN_status_csvc& cstat)
{
	// Not supported for ORG, cannot be done in FILES
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


int
FNSP_OrgContext::c_destroy_subcontext(const FN_string &name,
    FN_status_csvc &cstat)
{
	// Should not be supported.  Rather dangerous.
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_attrset*
FNSP_OrgContext::c_get_syntax_attrs(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *ref = resolve(name, cstat);

	if (cstat.is_success()) {
		FN_attrset* answer =
		    FNSP_Syntax(FNSP_organization_context)->get_syntax_attrs();
		delete ref;
		if (answer) {
			return (answer);
		}
		cstat.set_error(FN_E_INSUFFICIENT_RESOURCES, *my_reference,
		    name);
		return (0);
	}
	return (0);
}

FN_attribute*
FNSP_OrgContext::c_attr_get(const FN_string &name,
    const FN_identifier &id, unsigned int,
    FN_status_csvc &cs)
{
	if (!name.is_empty()) {
		// resolve name and have attr_get be performed there
		FN_ref *ref = resolve(name, cs);
		if (cs.is_success()) {
			cs.set_error(FN_E_SPI_CONTINUE, *ref, empty_name);
			delete ref;
		}
		return (0);
	}

	const FN_attribute *attribute;
	FN_attrset *attrset = get_attrset_for_org(*my_reference);
	attribute = attrset->get(id);
	if (attribute) {
		cs.set_success();
		return (new FN_attribute(*attribute));
	} else {
		cs.set_error(FN_E_NO_SUCH_ATTRIBUTE, *my_reference, name);
		return (0);
	}
}

int
FNSP_OrgContext::c_attr_modify(const FN_string &name,
    unsigned int,
    const FN_attribute&, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_valuelist*
FNSP_OrgContext::c_attr_get_values(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_csvc &cs)
{
	FN_valuelist_svc *answer = 0;
	FN_attribute *attribute = c_attr_get(name, id, follow_link, cs);
	if (cs.is_success())
		answer = new FN_valuelist_svc(attribute);

	return (answer);
}

FN_attrset*
FNSP_OrgContext::c_attr_get_ids(const FN_string &name, unsigned int follow_link,
    FN_status_csvc &cs)
{
	if (!name.is_empty()) {
		// resolve name and have attr_get be performed there
		FN_ref *ref = resolve(name, cs);
		if (cs.is_success()) {
			cs.set_error(FN_E_SPI_CONTINUE, *ref, empty_name);
			delete ref;
		}
		return (0);
	}

	cs.set_success();
	return (get_attrset_for_org(*my_reference));
}

FN_multigetlist*
FNSP_OrgContext::c_attr_multi_get(const FN_string &name,
    const FN_attrset *attrset, unsigned int follow_link,
    FN_status_csvc &cs)
{
	FN_multigetlist_svc *answer;
	FN_attrset *all = c_attr_get_ids(name, follow_link, cs);
	if (all == NULL)
		return (NULL);

	if (attrset == NULL) {
		answer = new FN_multigetlist_svc(all);
		return (answer);
	}

	FN_attrset *selection = FNSP_get_selected_attrset(*all, *attrset);
	delete all;
	if (selection)
		return (new FN_multigetlist_svc(selection));
	else
		return (NULL);
}

int
FNSP_OrgContext::c_attr_multi_modify(const FN_string &name,
    const FN_attrmodlist&, unsigned int,
    FN_attrmodlist**,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

// == Lookup (name:)
// %%% cannot be linked (reference generated algorithmically)
// %%% If supported, must store link somewhere and change
// %%% entire ctx implementation, which depends on non-linked repr
// %%%
FN_ref *
FNSP_OrgContext::c_lookup_nns(const FN_string &name,
    unsigned int, /* lookup_flags */
    FN_status_csvc& cstat)
{
	FN_ref *answer = 0;
	unsigned status;

	if (name.is_empty()) {
		FNSP_Impl *nns_impl = get_nns_impl(*my_reference, status);
		if (nns_impl != 0) {
			unsigned estatus = nns_impl->context_exists();
			switch (estatus) {
			case FN_SUCCESS:
				answer = nns_impl->get_nns_ref();
				if (answer == 0)
					cstat.set(
					    FN_E_INSUFFICIENT_RESOURCES);
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
	return (answer);
}

FN_namelist*
FNSP_OrgContext::c_list_names_nns(const FN_string &name,
    FN_status_csvc& cstat)
{
	unsigned status;
	FN_namelist* answer = 0;
	FN_ref *ref = resolve(name, cstat);

	if (cstat.is_success()) {
		FNSP_Impl *nns_impl = get_nns_impl(*ref, status);
		if (nns_impl) {
			answer = nns_impl->list_names(status);
			delete nns_impl;
			if (status == FN_E_NOT_A_CONTEXT)
				// %%% was CONTEXT_NOT_FOUND
				status = FN_E_NAME_NOT_FOUND;
			if (status != FN_SUCCESS)
				cstat.set_error(status, *ref, empty_name);
		} else
			cstat.set_error(status, *ref, empty_name);
	}

	delete ref;
	if (answer)
		return (answer);
	else
		return (0);
}

FN_bindinglist*
FNSP_OrgContext::c_list_bindings_nns(const FN_string &name,
    FN_status_csvc& cstat)
{
	unsigned status;
	FN_bindinglist *answer = 0;
	FN_ref *ref = resolve(name, cstat);
	if (cstat.is_success()) {
		FNSP_Impl *nns_impl = get_nns_impl(*ref, status);
		if (nns_impl) {
			answer = nns_impl->list_bindings(status);
			delete nns_impl;
			if (status != FN_SUCCESS)
				cstat.set_error(status, *ref, empty_name);
		} else
			cstat.set_error(status, *ref, empty_name);
	}

	delete ref;
	if (answer)
		return (answer);
	else
		return (0);
}


// Does it make sense to allow bind_nns, given that we hardwire
// where its contexts are stored (in file organization.fns)?  Probably not.
int
FNSP_OrgContext::c_bind_nns(const FN_string &name,
    const FN_ref &,
    unsigned,
    FN_status_csvc &cstat)
{
	// should we do a lookup first so that we can return
	// NotAContext when appropriate?

	FN_ref *nameref = resolve(name, cstat);

	if (cstat.is_success())
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *nameref,
		    empty_name);
	// else keep cstat from resolve

	delete nameref;
	return (0);
}

int
FNSP_OrgContext::c_unbind_nns(const FN_string &name, FN_status_csvc &cstat)
{
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_OrgContext::c_rename_nns(const FN_string &name,
    const FN_composite_name &,
    unsigned,
    FN_status_csvc &cstat)
{
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_ref *
FNSP_OrgContext::c_create_subcontext_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *orgref = resolve(name, cstat);
	FN_ref *ref = 0;
	unsigned status;

	if (cstat.is_success() && orgref) {
		FN_string *dirname = 0;
		FNSP_Impl *nns_impl = get_nns_impl(*orgref, status, &dirname);
		if (nns_impl && dirname) {
			unsigned estatus = nns_impl->context_exists();
			switch (estatus) {
			case FN_SUCCESS:
				status = FN_E_NAME_IN_USE;
				// must destroy explicitly first
				break;
			case FN_E_NOT_A_CONTEXT:
				// %%% was context_not_found
				ref = nns_impl->create_context(status, dirname);
				break;
			default:
				// cannot determine state of subcontext
				status = estatus;
			}
			delete nns_impl;
			delete dirname;
			if (status != FN_SUCCESS)
				cstat.set_error(status, *orgref, empty_name);
		} else
			cstat.set_error(status, *ref, empty_name);
	}
	delete orgref;
	return (ref);
}

int
FNSP_OrgContext::c_destroy_subcontext_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *orgref = resolve(name, cstat);

	if (cstat.is_success()) {
		FN_string *dirname = 0;
		unsigned status;
		FNSP_Impl *nns_impl = get_nns_impl(*orgref, status, &dirname);
		if (nns_impl && dirname) {
			status = nns_impl->destroy_context(dirname);
			delete nns_impl;
			delete dirname;
			if (status != FN_SUCCESS)
				cstat.set_error(status, *orgref, empty_name);
		} else {
			delete nns_impl;
			delete dirname;
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}

	delete orgref;
	return (cstat.is_success());
}

FN_attrset*
FNSP_OrgContext::c_get_syntax_attrs_nns(const FN_string &name,
    FN_status_csvc& cstat)
{
	FN_ref *nns_ref = c_lookup_nns(name, 0, cstat);

	if (cstat.is_success()) {
		FN_attrset* answer =
		    FNSP_Syntax(FNSP_nsid_context)->get_syntax_attrs();
		delete nns_ref;
		if (answer) {
			return (answer);
		}
		cstat.set(FN_E_INSUFFICIENT_RESOURCES);
		return (0);
	}
	return (0);
}

FN_attribute*
FNSP_OrgContext::c_attr_get_nns(const FN_string &name,
    const FN_identifier &, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_OrgContext::c_attr_modify_nns(const FN_string &name,
    unsigned int,
    const FN_attribute&, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_valuelist*
FNSP_OrgContext::c_attr_get_values_nns(const FN_string &name,
    const FN_identifier &, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_attrset*
FNSP_OrgContext::c_attr_get_ids_nns(const FN_string &name, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_multigetlist*
FNSP_OrgContext::c_attr_multi_get_nns(const FN_string &name,
    const FN_attrset *, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_OrgContext::c_attr_multi_modify_nns(const FN_string &name,
    const FN_attrmodlist&, unsigned int,
    FN_attrmodlist**,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

// Extended attribute operations
int
FNSP_OrgContext::c_attr_bind(const FN_string &name,
    const FN_ref & /* ref */, const FN_attrset * /* attrs */,
    unsigned int /* exclusive */, FN_status_csvc &status)
{
	// Not supported for ORG context
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_ref *
FNSP_OrgContext::c_attr_create_subcontext(const FN_string &name,
    const FN_attrset * /* attr */, FN_status_csvc &status)
{
	// Not supported for org context
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_searchlist *
FNSP_OrgContext::c_attr_search(const FN_string &sname,
    const FN_attrset *match_attrs, unsigned int return_ref,
    const FN_attrset *return_attr_ids, FN_status_csvc &cstat)
{
	if (!sname.is_empty()) {
		// resolve name and have search be performed there
		FN_ref *ref = resolve(sname, cstat);
		if (cstat.is_success()) {
			cstat.set_error(FN_E_SPI_CONTINUE, *ref, empty_name);
			delete ref;
		}
		return (0);
	}

	// Obtain the searchset
	FN_attrset *attrset;
	if (!match_attrs)
		attrset = new FN_attrset;
	else
		attrset = new FN_attrset(*match_attrs);

	FN_namelist *ns = c_list_names(empty_name, cstat);
	if (!cstat.is_success()) {
		delete attrset;
		return (0);
	}

	FN_searchset *ss = new FN_searchset;
	FN_attrset *name_attrset;
	FN_ref *ref;
	FN_attrset *request_attrset;
	FN_status status;
	FN_string *name;
	while (name = ns->next(status)) {
		name_attrset = attr_get_ids(*name, 1, status);
		if (status.code() == FN_E_NO_SUCH_ATTRIBUTE) {
			status.set_success();
			delete (name);
			continue;
		}
		if (!status.is_success()) {
			delete (name);
			break;
		}
		if (FNSP_is_attrset_subset(*name_attrset, *attrset)) {
			if (return_ref)
				ref = lookup(*name, status);
			else
				ref = 0;
			if (return_attr_ids)
				request_attrset = FNSP_get_selected_attrset(
				    *name_attrset, *return_attr_ids);
			else
				request_attrset = new
				    FN_attrset(*name_attrset);
			if (!status.is_success()) {
				delete name_attrset;
				break;
			}
			ss->add(*name, ref, request_attrset);
			delete ref;
			delete request_attrset;
		}
		delete name_attrset;
		delete name;
	}
	delete attrset;
	delete ns;
	if (!status.is_success()) {
		delete ss;
		cstat.set_error(status.code(), *my_reference, empty_name);
		return (0);
	} else
		cstat.set_success();
	return (new FN_searchlist_svc(ss));
}

FN_ext_searchlist *
FNSP_OrgContext::c_attr_ext_search(const FN_string &name,
    const FN_search_control * /* control */,
    const FN_search_filter * /* filter */,
    FN_status_csvc &status)
{
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_OrgContext::c_attr_bind_nns(const FN_string &name,
    const FN_ref & /* ref */, const FN_attrset * /* attrs */,
    unsigned int /* exclusive */, FN_status_csvc &status)
{
	FN_ref *nameref = resolve(name, status);

	if (status.is_success())
		status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *nameref,
		    empty_name);

	delete nameref;
	return (0);
}

FN_ref *
FNSP_OrgContext::c_attr_create_subcontext_nns(const FN_string &name,
    const FN_attrset *attrs, FN_status_csvc &status)
{
	unsigned context_type;
	unsigned repr_type;
	FN_identifier *ref_type;
	FN_attrset *rest_attr = FNSP_get_create_params_from_attrs(
	    attrs, context_type, repr_type, &ref_type, FNSP_nsid_context);

	if (context_type != FNSP_nsid_context ||
	    repr_type != FNSP_normal_repr ||
	    ref_type != 0 ||
	    (rest_attr != 0 && rest_attr->count() > 0)) {
		status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference,
		    name);
		delete ref_type;
		delete rest_attr;
		return (0);
	}

	FN_ref *answer = c_create_subcontext_nns(name, status);

	// Cannot add attributes
	delete rest_attr;
	return (answer);
}

FN_searchlist *
FNSP_OrgContext::c_attr_search_nns(const FN_string &name,
    const FN_attrset *match_attrs, unsigned int return_ref,
    const FN_attrset *return_attr_ids, FN_status_csvc &cstat)
{
	FN_ref *ref = resolve(name, cstat);
	unsigned status;
	FN_searchlist* matches = NULL;

	if (cstat.is_success()) {
		FNSP_Impl *nns_impl = get_nns_impl(*ref, status);
		if (nns_impl) {
			matches = nns_impl->search_attrset(
			    match_attrs, return_ref, return_attr_ids,
			    status);
			delete nns_impl;
			// nns context not there -> '/' not found
			if (status == FN_E_NOT_A_CONTEXT)
				// %%% was CONTEXT_NOT_FOUND
				status = FN_E_NAME_NOT_FOUND;
			if (status != FN_SUCCESS)
				cstat.set_error(status, *ref, empty_name);
			else
				cstat.set_success();
		} else
			cstat.set_error(status, *ref, empty_name);
	}

	delete ref;
	if (matches)
		return (matches);
	else
		return (0);
}

FN_ext_searchlist *
FNSP_OrgContext::c_attr_ext_search_nns(const FN_string &name,
    const FN_search_control * /* control */,
    const FN_search_filter * /* filter */,
    FN_status_csvc &status)
{
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

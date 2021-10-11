/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_FlatContext.cc	1.7	97/08/15 SMI"


#include "FNSP_FlatContext.hh"
#include "FNSP_Syntax.hh"
#include "fnsp_utils.hh"

//  A flat naming system is one in which only non-hierarchical names are bound.
//  All names in the naming system are bound in a single context.
//  These names are junctions.
//
//  The bindings of the junctions
//  are stored in the binding table of the flat context.
//  The reference bound to a flat name is the same as its nns.
//

static const FN_composite_name empty_name((const unsigned char *)"");

FNSP_FlatContext::~FNSP_FlatContext()
{
	// subclasses clean up after themselves
}

FN_ref *
FNSP_FlatContext::get_ref(FN_status &stat) const
{
	FN_ref *answer = new FN_ref(*my_reference);

	if (answer)
		stat.set_success();
	else
		stat.set(FN_E_INSUFFICIENT_RESOURCES);

	return (answer);
}

FN_composite_name *
FNSP_FlatContext::equivalent_name(
    const FN_composite_name &name,
    const FN_string &,
    FN_status &status)
{
	status.set(FN_E_OPERATION_NOT_SUPPORTED, my_reference, &name);
	return (0);
}

// Return reference bound to name (junction)

FN_ref *
FNSP_FlatContext::resolve(const FN_string &name,
    unsigned int lookup_flags, FN_status_csvc &cstat)
{
	unsigned status;
	FN_ref *answer;

	answer = ns_impl->lookup_binding(name, status);
	if (status == FN_SUCCESS) {
		if (!(lookup_flags&FN_SPI_LEAVE_TERMINAL_LINK) &&
		    answer->is_link()) {
			cstat.set_continue(*answer, *my_reference);
			delete answer;
			answer = 0;
		} else
			cstat.set_success();
	} else
		cstat.set_error(status, *my_reference, name);

	return (answer);
}

FN_ref *
FNSP_FlatContext::c_lookup(const FN_string &name,
    unsigned int lookup_flags,
    FN_status_csvc& cstat)
{
	FN_ref *answer = 0;
	if (name.is_empty()) {
		// Return reference of current context
		answer = new FN_ref(*my_reference);
		if (answer)
			cstat.set_success();
		else
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
	} else
		answer = resolve(name, lookup_flags, cstat);

	return (answer);
}


// When the given name is null, return names bound in current context.
// When the given name is not null, resolve the name and set continue
// status for list operation to be performed on that reference

FN_namelist*
FNSP_FlatContext::c_list_names(const FN_string &name, FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		// listing names bound in current context
		FN_namelist *answer = 0;
		unsigned status;

		answer = ns_impl->list_names(status);
		if (status == FN_SUCCESS && answer) {
			cstat.set_success();
			return (answer);
		} else
			cstat.set_error(status, *my_reference, name);
	} else {
		// resolve name and have list be performed there
		FN_ref *ref = resolve(name, 0, cstat);

		if (cstat.is_success()) {
			cstat.set(FN_E_SPI_CONTINUE, ref, 0, &empty_name);
			delete ref;
		}
	}
	return (0);
}

// When the given name is null, return bindings bound in current context.
// When the given name is not null, resolve the name and if successful,
// set continue status for operation to occur in that context.

FN_bindinglist*
FNSP_FlatContext::c_list_bindings(const FN_string &name, FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		// list names in current (flat) context
		FN_bindinglist *answer = 0;
		unsigned status;

		answer = ns_impl->list_bindings(status);
		if (status == FN_SUCCESS && answer) {
			cstat.set_success();
			return (answer);
		} else
			cstat.set_error(status, *my_reference, name);
	} else {
		// resolve name and set continue status
		FN_ref *ref = resolve(name, 0, cstat);

		if (cstat.is_success()) {
			cstat.set(FN_E_SPI_CONTINUE, ref, 0, &empty_name);
			delete ref;
		}
	}
	return (0);
}

int
FNSP_FlatContext::c_bind(const FN_string &name, const FN_ref &ref,
    unsigned int bind_flags, FN_status_csvc &cstat)
{
	FN_attrset empty_attrs;

	// use empty_attrs to denote overwriting of any existing attrs

	return (c_attr_bind(name, ref, &empty_attrs, bind_flags, cstat));
}

int
FNSP_FlatContext::c_unbind(const FN_string &name, FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference,
		    name);
		return (0);
	} else {
		unsigned status;
		status = ns_impl->remove_binding(name);

		if (status == FN_SUCCESS)
			cstat.set_success();
		else
			cstat.set_error(status, *my_reference, name);
		return (status == FN_SUCCESS);
	}
}

int
FNSP_FlatContext::c_rename(const FN_string &name,
    const FN_composite_name &newname,
    unsigned rflags, FN_status_csvc &cstat)
{
	unsigned status;

	if (name.is_empty()) {
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference,
		    name);
		return (0);
	}

	void *p;
	const FN_string *fn = newname.first(p);
	if (fn == 0) {
		cstat.set_error(FN_E_ILLEGAL_NAME, *my_reference, name);
		return (0);
	}

	FN_composite_name *rn = newname.suffix(p);
	if (rn) {
		// support only atomic name to be renamed
		delete rn;
		FN_string *newname_string = newname.string();
		cstat.set_error(FN_E_ILLEGAL_NAME, *my_reference,
		    *newname_string);
		delete newname_string;
		return (0);
	}

	status = ns_impl->rename_binding(name, *fn, rflags);
	if (status == FN_E_NOT_A_CONTEXT) {
		// %%% was context_not_found
		// bindings table did not even exist
		status = FN_E_CONFIGURATION_ERROR;
	}

	if (status == FN_SUCCESS)
		cstat.set_success();
	else
		cstat.set_error(status, *my_reference, name);

	return (status == FN_SUCCESS);
}

// Flat context has no subcontexts
FN_ref *
FNSP_FlatContext::c_create_subcontext(const FN_string &name,
    FN_status_csvc &cstat)
{
	unsigned status;
	unsigned context_type;

	if (name.is_empty()) {
		// cannot create explicit nns or empty name
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference,
		    name);
		return (0);
	}

	switch (ns_impl->my_address->get_context_type()) {
	case FNSP_username_context:
		context_type = FNSP_user_context;
		break;
	case FNSP_hostname_context:
		context_type = FNSP_host_context;
		break;
	case FNSP_host_context:
	case FNSP_user_context:
		context_type = FNSP_service_context;
		break;
	default:
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	FN_ref *newref = ns_impl->create_and_bind(
	    name, context_type, FNSP_normal_repr,
	    status, FNSP_CHECK_NAME);
	if (status == FN_SUCCESS)
		cstat.set_success();
	else
		cstat.set_error(status, *my_reference, name);
	return (newref);
}

int
FNSP_FlatContext::c_destroy_subcontext(const FN_string &name,
    FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference,
		    name);
		return (0);
	}

	unsigned status;
	status = ns_impl->destroy_and_unbind(name);

	if (status == FN_SUCCESS)
		cstat.set_success();
	else
		cstat.set_error(status, *my_reference, name);

	return (status == FN_SUCCESS);
}

FN_attrset*
FNSP_FlatContext::c_get_syntax_attrs(const FN_string &name,
    FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		// return syntax of current (flat) context
		FN_attrset *answer = 0;
		answer = FNSP_Syntax(ns_impl->my_address->
		    get_context_type())->get_syntax_attrs();
		if (answer)
			cstat.set_success();
		else
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
		return (answer);
	} else {
		// resolve name and set continue status
		FN_ref *ref = resolve(name, 0, cstat);
		if (cstat.is_success())
			cstat.set(FN_E_SPI_CONTINUE, ref, 0, &empty_name);

		if (ref)
			delete ref;
	}
	return (0);
}

int
FNSP_FlatContext::is_following_link(
    const FN_string &name, FN_status_csvc &cstat)
{
	FN_ref *ref;
	unsigned int status;

	if (name.is_empty())
		// own reference can never be a link
		return (0);

	ref = ns_impl->lookup_binding(name, status);
	if (status == FN_SUCCESS && ref->is_link()) {
		cstat.set_continue(*ref, *my_reference);
		delete ref;
		return (1);
	}
	delete ref;
	return (0);
}


FN_attribute*
FNSP_FlatContext::c_attr_get(const FN_string &name,
    const FN_identifier &id,
    unsigned int follow_link,
    FN_status_csvc &cs)
{
	if (follow_link && is_following_link(name, cs)) {
		return (0);
	}

	// %%% need to handle name.is_empty() case

	unsigned status;
	FN_attrset *attrset = ns_impl->get_attrset(name, status);

	if (attrset == 0)
		status = FN_E_NO_SUCH_ATTRIBUTE;

	if (status != FN_SUCCESS) {
		cs.set_error(status, *my_reference, name);
		return (0);
	}

	// Get the required attribute
	FN_attribute *attr = 0;
	const FN_attribute *old_attr = attrset->get(id);
	if (old_attr) {
		attr = new FN_attribute(*old_attr);
		cs.set_success();
	} else
		cs.set_error(FN_E_NO_SUCH_ATTRIBUTE, *my_reference, name);

	delete attrset;
	return (attr);
}

int
FNSP_FlatContext::c_attr_modify(const FN_string &aname,
    unsigned int flags,
    const FN_attribute& attr,
    unsigned int follow_link,
    FN_status_csvc& cs)
{
	if (follow_link && is_following_link(aname, cs)) {
		return (0);
	}

	// %%% need to handle name.is_empty() case

	unsigned status = ns_impl->modify_attribute(aname, attr, flags);

	if (status == FN_SUCCESS) {
		cs.set_success();
		return (1);
	} else {
		cs.set_error(status, *my_reference, aname);
		return (0);
	}
}


FN_valuelist*
FNSP_FlatContext::c_attr_get_values(const FN_string &name,
    const FN_identifier &id,
    unsigned int follow_link,
    FN_status_csvc &cs)
{
	// follow_link is handled by c_attr_get()

	FN_valuelist_svc *answer = 0;
	FN_attribute *attribute = c_attr_get(name, id, follow_link, cs);
	if (cs.is_success())
		answer = new FN_valuelist_svc(attribute);

	return (answer);
}

FN_attrset*
FNSP_FlatContext::c_attr_get_ids(const FN_string &name,
    unsigned int follow_link,
    FN_status_csvc &cs)
{
	if (follow_link && is_following_link(name, cs)) {
		return (0);
	}

	// %%% need to handle name.is_empty() case

	unsigned status;
	FN_attrset *attrset = ns_impl->get_attrset(name, status);
	if (status != FN_SUCCESS) {
		cs.set_error(status, *my_reference, name);
		return (0);
	}

	if (attrset == 0)
		attrset = new FN_attrset;

	cs.set_success();
	return (attrset);
}

FN_multigetlist*
FNSP_FlatContext::c_attr_multi_get(const FN_string &name,
    const FN_attrset *attrset,
    unsigned int follow_link,
    FN_status_csvc &cs)
{
	// follow_link is handled by c_attr_get_ids()

	// %%% need to handle name.is_empty() case

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
FNSP_FlatContext::c_attr_multi_modify(const FN_string &name,
    const FN_attrmodlist &modlist,
    unsigned int follow_link,
    FN_attrmodlist **un_modlist,
    FN_status_csvc &cs)
{
	if (follow_link && is_following_link(name, cs)) {
		return (0);
	}

	// %%% need to handle name.is_empty() case

	void *ip;
	unsigned int mod_op, status;
	const FN_attribute *attribute;
	int num_mods = 0;

	for (attribute = modlist.first(ip, mod_op);
	    attribute != NULL;
	    attribute = modlist.next(ip, mod_op), ++num_mods) {
		// use 0 for follow_link to avoid repeated checking
		if (!(status = c_attr_modify(name, mod_op, *attribute, 0, cs)))
			break;
	}
	if (attribute == NULL || num_mods == 0)
		return (status);

	if (un_modlist) {
		for ((*un_modlist) = new FN_attrmodlist;
		    attribute != NULL;
		    attribute = modlist.next(ip, mod_op)) {
			(*un_modlist)->add(mod_op, *attribute);
		}
	}
	return (status);
}

int
FNSP_FlatContext::c_attr_bind(const FN_string &name,
			    const FN_ref &ref,
			    const FN_attrset *attrs,
			    unsigned int exclusive,
			    FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference,
		    name);
		return (0);
	} else {
		unsigned status;

		status = ns_impl->add_binding(name, ref, attrs, exclusive);
		if (status == FN_SUCCESS)
			cstat.set_success();
		else
			cstat.set_error(status, *my_reference, name);

		return (status == FN_SUCCESS);
	}
}


FN_ref *
FNSP_FlatContext::c_attr_create_subcontext(const FN_string &name,
    const FN_attrset *attrs,
    FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		// cannot create explicit nns or empty name
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference,
		    name);
		return (0);
	}

	unsigned context_type;
	unsigned int status = FN_SUCCESS;
	unsigned representation_type;
	FN_identifier *ref_type;
	FN_attrset *rest_attrs = FNSP_get_create_params_from_attrs(attrs,
	    context_type, representation_type, &ref_type);

	switch (ns_impl->my_address->get_context_type()) {
	case FNSP_username_context:
		if ((context_type != FNSP_user_context) ||
		    ((ref_type != 0) && (*ref_type !=
		    *FNSP_reftype_from_ctxtype(FNSP_user_context)))) {
			cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED,
			    *my_reference, name);
			return (0);
		}
		break;
	case FNSP_hostname_context:
		if ((context_type != FNSP_host_context) ||
		    ((ref_type != 0) && (*ref_type !=
		    *FNSP_reftype_from_ctxtype(FNSP_host_context)))) {
			cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED,
			    *my_reference, name);
			return (0);
		}
	default:
		break;
	}

	FN_ref *newref = ns_impl->create_and_bind(
		    name,
		    context_type,
		    representation_type,
		    status,
		    FNSP_CHECK_NAME,
		    ref_type,
		    rest_attrs);
	if (status == FN_SUCCESS)
		cstat.set_success();
	else
		cstat.set_error(status, *my_reference, name);
	delete rest_attrs;
	delete ref_type;

	return (newref);
}

FN_searchlist *
FNSP_FlatContext::c_attr_search(const FN_string &name,
    const FN_attrset *match_attrs,
    unsigned int return_ref,
    const FN_attrset *return_attr_ids,
    FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		unsigned int status;
		FN_searchlist *matches = ns_impl->search_attrset(
		    match_attrs, return_ref, return_attr_ids, status);
		if (status == FN_SUCCESS) {
			cstat.set_success();
		} else {
			cstat.set_error(status, *my_reference,
			    (const unsigned char *)"");
		}
		if (matches)
			return (matches);
		else
			return (NULL);
	} else {
		// resolve name and have search be performed there
		FN_ref *ref = resolve(name, 0, cstat);

		if (cstat.is_success()) {
			cstat.set(FN_E_SPI_CONTINUE, ref, 0, &empty_name);
			delete ref;
		}
	}
	return (0);
}

FN_ext_searchlist *
FNSP_FlatContext::c_attr_ext_search(
    const FN_string &name,
    const FN_search_control *control,
    const FN_search_filter *filter,
    FN_status_csvc &cstat)
{
	FN_ext_searchset *answer;
	FN_ref *ref;
	FN_attrset *attrset;
	const FN_attrset *req_attr_ids;

	// First check for follow links
	if (control && (control->follow_links()) &&
	    is_following_link(name, cstat))
		return (0);

	// Check for scope
	if (control && ((control->scope() == FN_SEARCH_SUBTREE) ||
	    (control->scope() == FN_SEARCH_CONSTRAINED_SUBTREE))) {
		// %%% needs work
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}

	if (control && (control->scope() == FN_SEARCH_NAMED_OBJECT)) {
		FN_attrset *all = c_attr_get_ids(name,
		    control->follow_links(), cstat);
		if (!cstat.is_success())
			return (0);
		answer = new FN_ext_searchset();
		if (FNSP_is_statisfying_search_filter(all, filter)) {
			if (control->return_ref())
				ref = resolve(name, 0, cstat);
			else
				ref = 0;
			if (req_attr_ids = control->return_attr_ids())
				attrset = FNSP_get_selected_attrset(
				    *all, *req_attr_ids);
			else
				attrset = new FN_attrset(*all);
			answer->add(name, ref, attrset);
		}
		delete all;
		return (new FN_ext_searchlist_svc(answer));
	}

	// scope is FN_SEARCH_ONE_CONTEXT
	// Try to convert the filter expression to a simple search
	FN_attrset *match_attrs = FNSP_to_simple_search_attrset(filter);
	if (match_attrs == NULL) {
		// %%% needs work
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}

	// Perform simple search
	FN_attrset no_attrs;
	FN_searchlist *results = c_attr_search(name, match_attrs,
	    control ? control->return_ref() : 0,
	    control ? control->return_attr_ids() : &no_attrs, cstat);
	if (!results)
		return (0);

	// Copy the results
	int count = 0;
	FN_string *search_name;
	FN_status status;
	answer = new FN_ext_searchset();
	while (search_name = results->next(&ref, &attrset, status)) {
		if (control) 
			if (count < control->max_names())
				answer->add(*search_name, ref, attrset);
			else
				break;
		else
			answer->add(*search_name, ref, attrset);
	}
	return (new FN_ext_searchlist_svc(answer));
}
			


// Return binding of given name.
// If given name is null, return not found because
// there can be no nns associated with these types of flat contexts
FN_ref *
FNSP_FlatContext::c_lookup_nns(const FN_string &name,
    unsigned int lookup_flags,
    FN_status_csvc& cstat)
{
	FN_ref *answer = 0;

	if (name.is_empty()) {
		cstat.set_error(FN_E_NAME_NOT_FOUND, *my_reference, name);
	} else
		answer = c_lookup(name, lookup_flags, cstat);

	return (answer);
}

// If given name is null, return names bound under nns of current context.
// If given name is not null, return names bound under nns of given name.
// In both cases, resolve to desired context first, then return
// FN_E_SPI_CONTINUE.

FN_namelist*
FNSP_FlatContext::c_list_names_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *ref = c_lookup_nns(name, 0, cstat);

	if (cstat.is_success())
		cstat.set(FN_E_SPI_CONTINUE, ref, 0, 0);

	if (ref)
		delete ref;
	return (0);
}

// If given name is null, return names bound under nns of current context.
// If given name is not null, return names bound under nns of given name.
// In both cases, resolve to desired context first,
// then return FN_E_SPI_CONTINUE.
// In other words, do exactly the same thing as c_list_names_nns.
FN_bindinglist*
FNSP_FlatContext::c_list_bindings_nns(const FN_string &name,
    FN_status_csvc& cstat)
{
	FN_ref *ref = c_lookup_nns(name, 0, cstat);

	if (cstat.is_success())
		cstat.set(FN_E_SPI_CONTINUE, ref, 0, 0);

	if (ref)
		delete ref;
	return (0);
}


int
FNSP_FlatContext::c_bind_nns(const FN_string &name, const FN_ref &ref,
    unsigned bind_flags, FN_status_csvc &cstat)
{
	return (c_bind(name, ref, bind_flags, cstat));
}

int
FNSP_FlatContext::c_unbind_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	return (c_unbind(name, cstat));
}

int
FNSP_FlatContext::c_rename_nns(const FN_string &name,
    const FN_composite_name &newname,
    unsigned flags, FN_status_csvc &cstat)
{
	return (c_rename(name, newname, flags, cstat));
}

FN_ref *
FNSP_FlatContext::c_create_subcontext_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	return (c_create_subcontext(name, cstat));
}

int
FNSP_FlatContext::c_destroy_subcontext_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	return (c_destroy_subcontext(name, cstat));
}

FN_attrset*
FNSP_FlatContext::c_get_syntax_attrs_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *ref = c_lookup_nns(name, 0, cstat);
	if (cstat.is_success())
		cstat.set(FN_E_SPI_CONTINUE, ref, 0, 0);

	if (ref)
		delete ref;
	return (0);
}

FN_attribute*
FNSP_FlatContext::c_attr_get_nns(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_csvc &cs)
{
	return (c_attr_get(name, id, follow_link, cs));
}

int
FNSP_FlatContext::c_attr_modify_nns(const FN_string &name,
    unsigned int mod_op,
    const FN_attribute &id,
    unsigned int follow_link,
    FN_status_csvc &cs)
{
	return (c_attr_modify(name, mod_op, id, follow_link, cs));
}

FN_valuelist*
FNSP_FlatContext::c_attr_get_values_nns(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_csvc &cs)
{
	return (c_attr_get_values(name, id, follow_link, cs));
}

FN_attrset*
FNSP_FlatContext::c_attr_get_ids_nns(const FN_string &name,
    unsigned int follow_link,
    FN_status_csvc &cs)
{
	return (c_attr_get_ids(name, follow_link, cs));
}

FN_multigetlist*
FNSP_FlatContext::c_attr_multi_get_nns(const FN_string &name,
    const FN_attrset *attrset, unsigned int follow_link,
    FN_status_csvc &cs)
{
	return (c_attr_multi_get(name, attrset, follow_link, cs));
}

int
FNSP_FlatContext::c_attr_multi_modify_nns(const FN_string &name,
    const FN_attrmodlist &modlist,
    unsigned int follow_link,
    FN_attrmodlist **mod,
    FN_status_csvc &cs)
{
	return (c_attr_multi_modify(name, modlist, follow_link, mod, cs));
}


int
FNSP_FlatContext::c_attr_bind_nns(const FN_string &name,
    const FN_ref &ref,
    const FN_attrset *attrs,
    unsigned int exclusive,
    FN_status_csvc &cs)
{
	return (c_attr_bind(name, ref, attrs, exclusive, cs));
}

FN_ref *
FNSP_FlatContext::c_attr_create_subcontext_nns(const FN_string &name,
    const FN_attrset *attrs, FN_status_csvc &cstat)
{

	return (c_attr_create_subcontext(name, attrs, cstat));
}

FN_searchlist *
FNSP_FlatContext::c_attr_search_nns(
    const FN_string &name,
    const FN_attrset *match_attrs,
    unsigned int ret_ref,
    const FN_attrset *ret_attr_ids,
    FN_status_csvc &cs)
{
	return (c_attr_search(name, match_attrs, ret_ref, ret_attr_ids, cs));

}

FN_ext_searchlist *
FNSP_FlatContext::c_attr_ext_search_nns(
    const FN_string &name,
    const FN_search_control *control,
    const FN_search_filter *filter,
    FN_status_csvc &cs)
{
	return (c_attr_ext_search(name, control, filter, cs));
}

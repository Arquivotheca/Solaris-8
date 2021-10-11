/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_PrinterObject.cc	1.6	97/08/15 SMI"

#include <xfn/fn_p.hh>
#include "FNSP_Syntax.hh"
#include "fnsp_utils.hh"
#include "FNSP_PrinterObject.hh"

static const FN_string empty_string((const unsigned char *) "");
static const FN_composite_name empty_name((const unsigned char *)"");

FNSP_PrinterObject::~FNSP_PrinterObject()
{
	// subclasses will clean up themselves
}

FN_ref*
FNSP_PrinterObject::get_ref(FN_status &stat) const
{
	stat.set_success();

	return (new FN_ref(*my_reference));
}

int
FNSP_PrinterObject::is_following_link(const FN_string &name,
    FN_status_csvc &cstat)
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

FN_ref*
FNSP_PrinterObject::resolve(const FN_string &aname,
    FN_status_csvc &cstat)
{
	FN_ref *ref = 0;
	unsigned status;

	ref = ns_impl->lookup_binding(aname, status);
	if (status == FN_SUCCESS)
		cstat.set_success();
	else
		cstat.set_error(status, *my_reference, aname);
	return (ref);
}

FN_composite_name *
FNSP_PrinterObject::equivalent_name(
    const FN_composite_name &name,
    const FN_string &,
    FN_status &status)
{
	status.set(FN_E_OPERATION_NOT_SUPPORTED, my_reference, &name);
	return (0);
}

FN_ref*
FNSP_PrinterObject::c_lookup(const FN_string& name, unsigned int,
			FN_status_csvc& cstat)
{
	if (name.is_empty()) {
		FN_ref *answer = 0;

		// No name was given; resolves to current reference of context
		answer = new FN_ref(*my_reference);
		if (answer)
			cstat.set_success();
		else
			cstat.set_error(FN_E_INSUFFICIENT_RESOURCES,
			    *my_reference, name);
		return (answer);
	} else
		return (resolve(name, cstat));
}

FN_namelist*
FNSP_PrinterObject::c_list_names(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_namelist *ns = 0;
	if (name.is_empty()) {
		// Obtain listing from naming service
		unsigned status;
		ns = ns_impl->list_names(status);
		if (status == FN_SUCCESS) {
			cstat.set_success();
			return (ns);
		} else {
			cstat.set_error(status, *my_reference, name);
			return (0);
		}
	} else {
		// If name is not empty, get the reference
		// and set continue context
		FN_ref *next_ref = resolve(name, cstat);
		if (cstat.is_success())
			cstat.set_continue(*next_ref, *my_reference,
			    &empty_string);
		delete next_ref;
		return (0);
	}
}

FN_bindinglist*
FNSP_PrinterObject::c_list_bindings(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_bindinglist *bs = 0;
	if (name.is_empty()) {
		// Obtain listing from naming service
		unsigned status;
		bs = ns_impl->list_bindings(status);
		if (status == FN_SUCCESS) {
			cstat.set_success();
			return (bs);
		} else {
			cstat.set_error(status, *my_reference, name);
			return (0);
		}
	} else {
		// If name is not empty, get the reference
		// and set continue context
		FN_ref *next_ref = resolve(name, cstat);
		if (cstat.is_success())
			cstat.set_continue(*next_ref, *my_reference,
			    &empty_string);
		delete next_ref;
		return (0);
	}
}

int
FNSP_PrinterObject::c_bind(const FN_string &name, const FN_ref &ref,
    unsigned BindFlags, FN_status_csvc &cstat)
{
	FN_attrset empty_attrs;

	return (c_attr_bind(name, ref, &empty_attrs, BindFlags, cstat));
}

int
FNSP_PrinterObject::c_unbind(const FN_string &name,
    FN_status_csvc &cstat)
{
	unsigned status;
	if (name.is_empty())
		status = FN_E_OPERATION_NOT_SUPPORTED;
	else
		status = ns_impl->remove_binding(name);

	if (status == FN_SUCCESS)
		cstat.set_success();
	else
		cstat.set_error(status, *my_reference, name);
	return (status == FN_SUCCESS);
}

int
FNSP_PrinterObject::c_rename(const FN_string &name,
    const FN_composite_name &newname, unsigned rflags, FN_status_csvc &cstat)
{
	unsigned status;

	if (name.is_empty()) {
		cstat.set_error(FN_E_NAME_IN_USE, *my_reference,
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

FN_ref*
FNSP_PrinterObject::c_create_subcontext(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *newref = 0;
	unsigned status;
	if (name.is_empty())
		status = FN_E_OPERATION_NOT_SUPPORTED;
	else {
		newref = ns_impl->create_and_bind(name,
		    FNSP_printer_object,
		    FNSP_normal_repr, status, 1,
		    FNSP_reftype_from_ctxtype(FNSP_printer_object));
	}

	if (status == FN_SUCCESS)
		cstat.set_success();
	else
		cstat.set_error(status, *my_reference, name);
	return (newref);
}

int
FNSP_PrinterObject::c_destroy_subcontext(const FN_string &name,
    FN_status_csvc &cstat)
{
	unsigned status;
	if (name.is_empty())
		status = FN_E_OPERATION_NOT_SUPPORTED;
	else
		status = ns_impl->destroy_and_unbind(name);

	if (status == FN_SUCCESS)
		cstat.set_success();
	else
		cstat.set_error(status, *my_reference, name);
	return (status == FN_SUCCESS);
}

FN_attrset*
FNSP_PrinterObject::c_get_syntax_attrs(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_attrset* answer;
	if (name.is_empty()) {
		answer =
		    FNSP_Syntax(FNSP_printer_object)->get_syntax_attrs();
		if (answer) {
			cstat.set_success();
			return (answer);
		} else
			cstat.set_error(FN_E_INSUFFICIENT_RESOURCES,
			    *my_reference, name);
	} else {
		FN_ref *ref = resolve(name, cstat);
		if (cstat.is_success()) {
			answer = FNSP_Syntax(
			    FNSP_printer_object)->get_syntax_attrs();
			delete ref;
			if (answer)
				return (answer);
			else
				cstat.set_error(FN_E_INSUFFICIENT_RESOURCES,
						*my_reference, name);
		}
	}
	return (0);
}

FN_attribute*
FNSP_PrinterObject::c_attr_get(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_csvc &cs)
{
	if (follow_link && is_following_link(name, cs)) {
		return (0);
	}

	unsigned status;
	FN_attrset *attrset = ns_impl->get_attrset(name, status);
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
FNSP_PrinterObject::c_attr_modify(const FN_string &aname,
    unsigned int flags, const FN_attribute &attr,
    unsigned int follow_link, FN_status_csvc &cs)
{
	if (follow_link && is_following_link(aname, cs)) {
		return (0);
	}

	unsigned status = ns_impl->modify_attribute(
	    aname, attr, flags);

	if (status == FN_SUCCESS) {
		cs.set_success();
		return (1);
	} else {
		cs.set_error(status, *my_reference, aname);
		return (0);
	}
}

FN_valuelist*
FNSP_PrinterObject::c_attr_get_values(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
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
FNSP_PrinterObject::c_attr_get_ids(const FN_string &name,
    unsigned int follow_link, FN_status_csvc &cs)
{
	if (follow_link && is_following_link(name, cs)) {
		return (0);
	}

	unsigned status;
	FN_attrset *attrset = ns_impl->get_attrset(name, status);
	if (status != FN_SUCCESS) {
		cs.set_error(status, *my_reference, name);
		return (0);
	}
	cs.set_success();
	return (attrset);
}

FN_multigetlist*
FNSP_PrinterObject::c_attr_multi_get(const FN_string &name,
    const FN_attrset *attrset, unsigned int follow_link, FN_status_csvc &cs)
{
	// follow_link is handled by get_ids()

	FN_multigetlist_svc *answer;

	FN_attrset *result = c_attr_get_ids(name, follow_link, cs);
	if (!result)
		return (0);

	if (attrset == 0) {
		answer = new FN_multigetlist_svc(result);
		return (answer);
	}

	FN_attrset *selection = FNSP_get_selected_attrset(*result,
	    *attrset);
	delete result;
	if (selection)
		return (new FN_multigetlist_svc(selection));
	else
		return (0);
}

int
FNSP_PrinterObject::c_attr_multi_modify(const FN_string &name,
    const FN_attrmodlist &modlist, unsigned int follow_link,
    FN_attrmodlist **un_modlist,
    FN_status_csvc &cs)
{
	if (follow_link && is_following_link(name, cs)) {
		return (0);
	}

	void *ip;
	unsigned int mod_op, status;
	const FN_attribute *attribute;
	int num_mods = 0;

	for (attribute = modlist.first(ip, mod_op);
	    attribute != NULL;
	    attribute = modlist.next(ip, mod_op), ++num_mods) {
		if (!(status = c_attr_modify(name, mod_op, *attribute,
					    follow_link, cs)))
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
FNSP_PrinterObject::c_attr_bind(const FN_string &name,
    const FN_ref &ref,
    const FN_attrset *attrs,
    unsigned BindFlags, FN_status_csvc &astat)
{
	unsigned status;
	if (name.is_empty())
		status = FN_E_OPERATION_NOT_SUPPORTED;
	// cannot bind to self
	else
		status = ns_impl->add_binding(name, ref,
		    attrs, BindFlags);
	if (status == FN_SUCCESS)
		astat.set_success();
	else
		astat.set_error(status, *my_reference, name);
	return (status == FN_SUCCESS);
}

FN_searchlist *
FNSP_PrinterObject::c_attr_search(const FN_string &name,
    const FN_attrset *match_attrs,
    unsigned int return_ref,
    const FN_attrset *return_attr_ids,
    FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		unsigned int status;
		FN_searchlist *matches = ns_impl->search_attrset(
		    match_attrs, return_ref,
		    return_attr_ids, status);
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
		FN_ref *ref = resolve(name, cstat);

		if (cstat.is_success()) {
			cstat.set(FN_E_SPI_CONTINUE, ref, 0, &empty_name);
			delete ref;
		}
	}
	return (0);
}

FN_ext_searchlist *
FNSP_PrinterObject::c_attr_ext_search(
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
				ref = resolve(name, cstat);
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

// == Lookup (name/)
FN_ref*
FNSP_PrinterObject::c_lookup_nns(const FN_string &name,
    unsigned int stat, FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		cstat.set_error(FN_E_NAME_NOT_FOUND, *my_reference, name);
		return (0);
	}
	return (c_lookup(name, stat, cstat));
}

FN_namelist*
FNSP_PrinterObject::c_list_names_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	// Get the reference and set continue context
	FN_ref *next_ref = c_lookup_nns(name, 0, cstat);
	if (cstat.is_success())
		cstat.set_continue(*next_ref, *my_reference, 0);
	delete next_ref;
	return (0);
}

FN_bindinglist *
FNSP_PrinterObject::c_list_bindings_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	// Get the reference and set continue context
	FN_ref *next_ref = c_lookup_nns(name, 0, cstat);
	if (cstat.is_success())
		cstat.set_continue(*next_ref, *my_reference, 0);
	delete next_ref;
	return (0);
}


int
FNSP_PrinterObject::c_bind_nns(const FN_string &name,
    const FN_ref &ref, unsigned bind, FN_status_csvc &cstat)
{
	return (c_bind(name, ref, bind, cstat));
}

int
FNSP_PrinterObject::c_unbind_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	return (c_unbind(name, cstat));
}

int
FNSP_PrinterObject::c_rename_nns(const FN_string &name,
    const FN_composite_name &aname, unsigned bind, FN_status_csvc &cstat)
{
	return (c_rename(name, aname, bind, cstat));
}

FN_ref*
FNSP_PrinterObject::c_create_subcontext_nns(const FN_string &name,
    FN_status_csvc &cstat)

{
	return (c_create_subcontext(name, cstat));
}


int
FNSP_PrinterObject::c_destroy_subcontext_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference,
		    name);
		return (0);
	}
	return (c_destroy_subcontext(name, cstat));
}

FN_attrset*
FNSP_PrinterObject::c_get_syntax_attrs_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *nns_ref = c_lookup_nns(name, 0, cstat);

	if (cstat.is_success()) {
		FN_attrset *answer = FNSP_Syntax(
		    FNSP_printer_object)->get_syntax_attrs();
		delete nns_ref;
		if (answer)
			return (answer);
		cstat.set_error(FN_E_INSUFFICIENT_RESOURCES, *my_reference,
		    name);
	}
	return (0);
}

FN_attribute*
FNSP_PrinterObject::c_attr_get_nns(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link, FN_status_csvc &cs)
{
	return (c_attr_get(name, id, follow_link, cs));
}

int
FNSP_PrinterObject::c_attr_modify_nns(const FN_string &name,
    unsigned int mod, const FN_attribute &attr,
    unsigned int follow_link, FN_status_csvc &cs)
{
	return (c_attr_modify(name, mod, attr, follow_link, cs));
}

FN_valuelist*
FNSP_PrinterObject::c_attr_get_values_nns(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link, FN_status_csvc &cs)
{
	return (c_attr_get_values(name, id, follow_link, cs));
}

FN_attrset*
FNSP_PrinterObject::c_attr_get_ids_nns(const FN_string& name,
    unsigned int follow_link, FN_status_csvc &cs)
{
	return (c_attr_get_ids(name, follow_link, cs));
}

FN_multigetlist*
FNSP_PrinterObject::c_attr_multi_get_nns(const FN_string &name,
    const FN_attrset *attr, unsigned int follow_link, FN_status_csvc &cs)
{
	return (c_attr_multi_get(name, attr, follow_link, cs));
}

int
FNSP_PrinterObject::c_attr_multi_modify_nns(const FN_string &name,
    const FN_attrmodlist &modl, unsigned int follow_link,
    FN_attrmodlist ** unmod, FN_status_csvc &cs)
{
	return (c_attr_multi_modify(name, modl, follow_link, unmod, cs));
}

int
FNSP_PrinterObject::c_attr_bind_nns(const FN_string &name,
    const FN_ref &ref,
    const FN_attrset *attrs,
    unsigned BindFlags, FN_status_csvc &astat)
{
	return (c_attr_bind(name, ref, attrs, BindFlags, astat));
}

FN_searchlist *
FNSP_PrinterObject::c_attr_search_nns(const FN_string &name,
    const FN_attrset *match_attrs,
    unsigned int return_ref,
    const FN_attrset *return_attr_ids,
    FN_status_csvc &astat)
{
	return (c_attr_search(name, match_attrs, return_ref, return_attr_ids,
	    astat));
}

FN_ext_searchlist *
FNSP_PrinterObject::c_attr_ext_search_nns(
    const FN_string &name,
    const FN_search_control *control,
    const FN_search_filter *filter,
    FN_status_csvc &astat)
{
	return (c_attr_ext_search(name, control, filter, astat));
}


FN_ref *
FNSP_PrinterObject::c_attr_create_subcontext(const FN_string &name,
    const FN_attrset *attrs, FN_status_csvc &cs)
{
	unsigned context_type;
	unsigned representation_type;
	FN_identifier *ref_type = 0;
	// %%% supply 0 for ref_type to ignore it, if any
	FN_attrset *rest_attrs = FNSP_get_create_params_from_attrs(attrs,
	    context_type, representation_type, 0,
	    ns_impl->my_address->get_context_type());
	FN_ref *newref = 0;
	unsigned status;

	if ((name.is_empty()) || (representation_type != FNSP_normal_repr) ||
	    (context_type != FNSP_printer_object)) {
		status = FN_E_OPERATION_NOT_SUPPORTED;
	} else {
		// Child context inherits Printer Object reference type
		ref_type = new FN_identifier(
		    *(FNSP_reftype_from_ctxtype(FNSP_printer_object)));
		newref = ns_impl->create_and_bind(name,
		    context_type, representation_type, status, 1, ref_type,
		    rest_attrs);
	}
	if (status == FN_SUCCESS) {
		cs.set_success();
	} else {
		cs.set_error(status, *my_reference, name);
	}
	delete ref_type;
	delete rest_attrs;
	return (newref);
}

FN_ref *
FNSP_PrinterObject::c_attr_create_subcontext_nns(const FN_string &name,
    const FN_attrset *attrs,
    FN_status_csvc &stat)
{
	return (c_attr_create_subcontext(name, attrs, stat));
}

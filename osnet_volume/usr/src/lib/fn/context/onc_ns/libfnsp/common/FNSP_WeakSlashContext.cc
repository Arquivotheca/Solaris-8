/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_WeakSlashContext.cc	1.8	97/10/23 SMI"

#include "FNSP_WeakSlashContext.hh"
#include "FNSP_Syntax.hh"
#include "fnsp_utils.hh"
#include <xfn/fn_p.hh>

//  A FNSP_WeakSlashContext is derived from FN_ctx_asvc_weak_dynamic class.
//  A naming system composed of FNSP_WeakSlashContext supports a hierarchical
//  namespace with a slash-separated left-to-right syntax.
//  By the time processing gets to a FNSP_WeakSlashContext,
//  it only needs to deal  with 'atomic names'.  'nns pointers' may be
//  associated with each atomic  name.
//
//  The FNSP_WeakSlashContext itself may have an associatd nns pointer;
//  in this case, its binding could be found under the reserved name "FNS_nns"
//  in the context.  The bindings of names associated with the atomic names
//  are stored in the binding table of the atomic context.
//

#define	AUTHORITATIVE 1
static const FN_composite_name empty_name((const unsigned char *)"");

static
const FN_syntax_standard *my_syntax = FNSP_Syntax(FNSP_service_context);

FNSP_WeakSlashContext::~FNSP_WeakSlashContext()
{
	// subclasses clean up themselves
}

FN_ref *
FNSP_WeakSlashContext::get_ref(FN_status &stat) const
{
	FN_ref *answer = new FN_ref(*my_reference);

	if (answer)
		stat.set_success();
	else
		stat.set(FN_E_INSUFFICIENT_RESOURCES);

	return (answer);
}

FN_composite_name *
FNSP_WeakSlashContext::equivalent_name(
    const FN_composite_name &name,
    const FN_string &,
    FN_status &status)
{
	status.set(FN_E_OPERATION_NOT_SUPPORTED, my_reference, &name);
	return (0);
}


FN_ref *
FNSP_WeakSlashContext::resolve(const FN_string &name,
    unsigned int /* lookup_flags */, FN_status_asvc &astat)
{
	FN_ref *answer = 0;
	unsigned status;
	answer = ns_impl->lookup_binding(name, status);
	if (status == FN_SUCCESS)
		astat.set_success();
	else
		astat.set_error(status, my_reference, &name);
	return (answer);
}

FN_ref *
FNSP_WeakSlashContext::a_lookup(const FN_string &name,
    unsigned int lookup_flags,
    FN_status_asvc &astat)
{
	FN_ref *answer = 0;
	if (name.is_empty()) {
		// Return reference of current context
		answer = new FN_ref(*my_reference);
		if (answer)
			astat.set_success();
		else
			astat.set_error(FN_E_INSUFFICIENT_RESOURCES);
	} else
		answer = resolve(name, lookup_flags, astat);

	return (answer);
}

FN_namelist*
FNSP_WeakSlashContext::a_list_names(FN_status_asvc &astat)
{
	FN_namelist *answer = 0;
	unsigned status;
	// listing all names bound in current context
	answer = ns_impl->list_names(status);
	if (status == FN_SUCCESS) {
		astat.set_success();
	} else
		astat.set_error(status, my_reference);

	if (answer)
		return (answer);
	else
		return (0);
}

FN_bindinglist*
FNSP_WeakSlashContext::a_list_bindings(FN_status_asvc &astat)
{
	FN_bindinglist *answer = 0;
	unsigned status;
	// Get all bindings
	answer = ns_impl->list_bindings(status);

	if (status == FN_SUCCESS) {
		astat.set_success();
	} else
		astat.set_error(status, my_reference);

	if (answer)
		return (answer);
	else
		return (0);
}

int
FNSP_WeakSlashContext::a_bind(const FN_string &name, const FN_ref &ref,
    unsigned BindFlags, FN_status_asvc &astat)
{
	FN_attrset empty_attrs;

	return (a_attr_bind(name, ref, &empty_attrs, BindFlags, astat));
}

int
FNSP_WeakSlashContext::a_unbind(const FN_string &name, FN_status_asvc &astat)
{
	unsigned status;
	if (name.is_empty())
		status = FN_E_OPERATION_NOT_SUPPORTED; 	// cannot unbind self
	else
		status = ns_impl->remove_binding(name);

	if (status == FN_SUCCESS)
		astat.set_success();
	else
		astat.set_error(status, my_reference, &name);
	return (status == FN_SUCCESS);
}

int
FNSP_WeakSlashContext::a_rename(const FN_string &name,
    const FN_composite_name &newname,
    unsigned BindFlags, FN_status_asvc &astat)
{
	unsigned status;
	const FN_string *newtarget = 0;

	if (name.is_empty())
		status = FN_E_OPERATION_NOT_SUPPORTED;
	// cannot rename self
	else {
		// get string form of new name and check for legal name
		if (newname.count() == 1) {
			void *ip;
			newtarget = newname.first(ip);
			if (newtarget == 0) {
				astat.set_error(FN_E_ILLEGAL_NAME,
						my_reference,
						newtarget);
				return (0);
			}
		} else {
			// do not support renaming to a composite name
			FN_string *newstr = newname.string();
			if (newstr) {
				astat.set_error(FN_E_ILLEGAL_NAME,
						my_reference,
						newstr);
				delete newstr;
			} else
				astat.set_error(FN_E_ILLEGAL_NAME,
				    my_reference);
			return (0);
		}

		status = ns_impl->rename_binding(name, *newtarget,
		    BindFlags);
	}
	if (status != FN_SUCCESS)
		astat.set_error(status, my_reference, &name);
	else
		astat.set_success();
	return (status == FN_SUCCESS);
}

FN_ref *
FNSP_WeakSlashContext::a_create_subcontext(const FN_string &name,
    FN_status_asvc &astat)
{
	FN_ref *newref = 0;
	unsigned status;
	if (name.is_empty())
		// cannot create self
		status = FN_E_OPERATION_NOT_SUPPORTED;
	else {
		newref = ns_impl->create_and_bind(
			    name,
			    ns_impl->my_address->get_context_type(),
			    FNSP_normal_repr,
			    status,
			    FNSP_CHECK_NAME,
			    // Child context inherits parent's reference type.
			    my_reference->type());
	}
	if (status != FN_SUCCESS)
		astat.set_error(status, my_reference, &name);
	else
		astat.set_success();
	return (newref);
}

int
FNSP_WeakSlashContext::a_destroy_subcontext(const FN_string &name,
    FN_status_asvc &astat)
{
	unsigned status;
	if (name.is_empty())
		status = FN_E_OPERATION_NOT_SUPPORTED;
	else
		status = ns_impl->destroy_and_unbind(name);

	if (status == FN_SUCCESS) {
		astat.set_success();
		return (1);
	} else if (status == FN_E_MALFORMED_REFERENCE) {
		// Try to destroy the child context with empty name
		FN_ref *child_ref = a_lookup(name, 0, astat);
		if (astat.code() == FN_E_NAME_NOT_FOUND) {
			astat.set_success();
			return (1);
		} else if ((!child_ref) || (!astat.is_success()))
			return (0);

		FN_ctx_svc *child_ctx = FN_ctx_svc::from_ref(*child_ref,
		    AUTHORITATIVE, astat);
		if ((!child_ctx) || (!astat.is_success())) {
			astat.set_error(FN_E_NOT_A_CONTEXT, my_reference,
			    &name);
			delete child_ref;
			return (0);
		}

		delete child_ref;
		child_ctx->destroy_subcontext((unsigned char *) "", astat);
		delete child_ctx;
		if (astat.is_success()) {
			a_unbind(name, astat);
			if (astat.is_success())
				return (1);
		}
		return (0);
	} else
		astat.set_error(status, my_reference, &name);
	return (status == FN_SUCCESS);
}

FN_attrset*
FNSP_WeakSlashContext::a_get_syntax_attrs(FN_status_asvc &astat)
{
	FN_attrset* answer = my_syntax->get_syntax_attrs();

	if (answer)
		astat.set_success();
	else
		astat.set_error(FN_E_INSUFFICIENT_RESOURCES);
	return (answer);
}

int
FNSP_WeakSlashContext::is_following_link(
    const FN_string &name, FN_status_asvc &as)
{
	FN_ref *ref;
	unsigned int status;

	if (name.is_empty())
		// own reference can never be a link
		return (0);

	ref = ns_impl->lookup_binding(name, status);
	if (status == FN_SUCCESS && ref->is_link()) {
		as.set_continue(*ref, *my_reference);
		delete ref;
		return (1);
	}
	delete ref;
	return (0);
}


FN_attribute*
FNSP_WeakSlashContext::a_attr_get(const FN_string &aname,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_asvc &as)
{
	if (follow_link && is_following_link(aname, as)) {
		return (0);
	}

	unsigned status;
	FN_attribute *attr = 0;

	FN_attrset *attrset = ns_impl->get_attrset(aname, status);
	if (attrset == 0)
		status = FN_E_NO_SUCH_ATTRIBUTE;

	if (status != FN_SUCCESS) {
		as.set_error(status, my_reference, &aname);
		return (0);
	}

	// Get the required attribute
	const FN_attribute *old_attr = attrset->get(id);
	if (old_attr) {
		attr = new FN_attribute(*old_attr);
		as.set_success();
	} else
		as.set_error(FN_E_NO_SUCH_ATTRIBUTE, my_reference, &aname);

	delete attrset;
	return (attr);
}

int
FNSP_WeakSlashContext::a_attr_modify(const FN_string &aname,
    unsigned int flags,
    const FN_attribute &attr, unsigned int follow_link,
    FN_status_asvc &as)
{
	if (follow_link && is_following_link(aname, as)) {
		return (0);
	}

	unsigned status = ns_impl->modify_attribute(aname,
	    attr, flags);
	if (status == FN_SUCCESS) {
		as.set_success();
		return (1);
	} else
		as.set_error(status, my_reference, &aname);
	return (0);
}

FN_valuelist*
FNSP_WeakSlashContext::a_attr_get_values(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_asvc &as)
{
	// follow_link is handled by a_attr_get()
	FN_valuelist_svc *answer = 0;
	FN_attribute *attribute = a_attr_get(name, id, follow_link, as);
	if (as.is_success())
		answer = new FN_valuelist_svc(attribute);

	// Varaible "attribute" should not be deleted
	return (answer);
}

FN_attrset*
FNSP_WeakSlashContext::a_attr_get_ids(const FN_string &aname,
    unsigned int follow_link, FN_status_asvc &as)
{
	if (follow_link && is_following_link(aname, as)) {
		return (0);
	}

	unsigned status;

	// FNSP_get_attrset returns the values too, and the
	// multi_get function is dependent on this feature (or bug?)
	FN_attrset *attrset = ns_impl->get_attrset(aname, status);

	if (status != FN_SUCCESS) {
		as.set_error(status, my_reference, &aname);
		return (0);
	}

	if (attrset == 0)
		attrset = new FN_attrset;

	as.set_success();
	return (attrset);
}

FN_multigetlist*
FNSP_WeakSlashContext::a_attr_multi_get(const FN_string &name,
    const FN_attrset *attrset, unsigned int follow_link,
    FN_status_asvc &as)
{
	// follow_link is handled by a_attr_get_ids()

	FN_multigetlist_svc *answer;

	// The assumption is that attr_get_ids returns the values too
	FN_attrset *all = a_attr_get_ids(name, follow_link, as);
	if (all == NULL)
		return (NULL);

	// If the request is to return all the attributues
	if (attrset == NULL) {
		answer = new FN_multigetlist_svc(all);
		// The variable "all" should not be deleted
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
FNSP_WeakSlashContext::a_attr_multi_modify(const FN_string &name,
    const FN_attrmodlist &modlist, unsigned int follow_link,
    FN_attrmodlist **un_modlist,
    FN_status_asvc &as)
{
	if (follow_link && is_following_link(name, as)) {
		return (0);
	}

	void *ip;
	unsigned int mod_op, status;
	const FN_attribute *attribute;
	int num_mods = 0;

	for (attribute = modlist.first(ip, mod_op);
	    attribute != NULL;
	    attribute = modlist.next(ip, mod_op), ++num_mods) {
		// use 0 for follow_link to avoid repeated checking
		if (!(status = a_attr_modify(name, mod_op, *attribute, 0, as)))
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
FNSP_WeakSlashContext::a_attr_bind(const FN_string &name, const FN_ref &ref,
    const FN_attrset *attrs,
    unsigned BindFlags, FN_status_asvc &astat)
{
	unsigned status;
	if (name.is_empty())
		status = FN_E_OPERATION_NOT_SUPPORTED;
	// cannot bind to self
	else
		status = ns_impl->add_binding(name, ref, attrs, BindFlags);
	if (status == FN_SUCCESS)
		astat.set_success();
	else
		astat.set_error(status, my_reference, &name);
	return (status == FN_SUCCESS);
}

FN_searchlist *
FNSP_WeakSlashContext::a_attr_search(
    const FN_attrset *match_attrs,
    unsigned int return_ref,
    const FN_attrset *return_attr_ids,
    FN_status_asvc &astat)
{
	unsigned int status;
	FN_searchlist *matches = ns_impl->search_attrset(
	    match_attrs, return_ref, return_attr_ids, status);

	if (status != FN_SUCCESS) {
		astat.set_error(status, my_reference);
		return (0);
	}

	astat.set_success();
	return (matches);
}

FN_ext_searchlist *
FNSP_WeakSlashContext::a_attr_ext_search(
    const FN_string &name,
    const FN_search_control *control,
    const FN_search_filter *filter,
    FN_status_asvc &cstat)
{
	FN_ext_searchset *answer;
	FN_ref *ref;
	FN_attrset *attrset;
	const FN_attrset *req_attr_ids;

	// First check for follow links
	if (control && (control->follow_links()) &&
	    is_following_link(name, cstat))
		return (0);

	if (!name.is_empty()) {
		FN_ref *name_ref = resolve(name, 0, cstat);
		if (cstat.is_success()) {
			cstat.set(FN_E_SPI_CONTINUE, name_ref, &empty_name);
			delete name_ref;
			return (0);
		}
	}


	// Check for scope
	if (control && ((control->scope() == FN_SEARCH_SUBTREE) ||
	    (control->scope() == FN_SEARCH_CONSTRAINED_SUBTREE))) {
		// %%% needs work
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, my_reference, &name);
		return (0);
	}

	if (control && (control->scope() == FN_SEARCH_NAMED_OBJECT)) {
		FN_attrset *all = a_attr_get_ids(name,
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
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, my_reference, &name);
		return (0);
	}

	// Perform simple search
	FN_attrset no_attrs;
	FN_searchlist *results = a_attr_search(match_attrs,
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



FN_ref *
FNSP_WeakSlashContext::a_lookup_nns(const FN_string& name,
    unsigned int lookup_flags, FN_status_asvc &astat)
{
	if (name.is_empty()) {
		astat.set_error(FN_E_NAME_NOT_FOUND, my_reference, &name);
		return (0);
	} else
		return (a_lookup(name, lookup_flags, astat));
}

int
FNSP_WeakSlashContext::a_bind_nns(const FN_string& name, const FN_ref &ref,
    unsigned bind_flags, FN_status_asvc &astat)
{
	return (a_bind(name, ref, bind_flags, astat));
}

int
FNSP_WeakSlashContext::a_unbind_nns(const FN_string& name,
    FN_status_asvc &astat)
{
	return (a_unbind(name, astat));
}

int
FNSP_WeakSlashContext::a_rename_nns(const FN_string &name,
    const FN_composite_name &newname, unsigned rflags, FN_status_asvc &astat)
{
	return (a_rename(name, newname, rflags, astat));
}

FN_ref *
FNSP_WeakSlashContext::a_create_subcontext_nns(const FN_string& name,
    FN_status_asvc &astat)
{
	return (a_create_subcontext(name, astat));
}


int
FNSP_WeakSlashContext::a_destroy_subcontext_nns(const FN_string& name,
    FN_status_asvc &astat)
{
	return (a_destroy_subcontext(name, astat));
}

FN_attribute*
FNSP_WeakSlashContext::a_attr_get_nns(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_asvc &as)
{
	return (a_attr_get(name, id, follow_link, as));
}

int
FNSP_WeakSlashContext::a_attr_modify_nns(const FN_string &name,
    unsigned int flags,
    const FN_attribute &id, unsigned int follow_link,
    FN_status_asvc &as)
{
	return (a_attr_modify(name, flags, id, follow_link, as));
}

FN_valuelist*
FNSP_WeakSlashContext::a_attr_get_values_nns(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_asvc &as)
{
	return (a_attr_get_values(name, id, follow_link, as));
}

FN_attrset*
FNSP_WeakSlashContext::a_attr_get_ids_nns(const FN_string &name,
    unsigned int follow_link, FN_status_asvc &as)
{
	return (a_attr_get_ids(name, follow_link, as));
}

FN_multigetlist*
FNSP_WeakSlashContext::a_attr_multi_get_nns(const FN_string &name,
    const FN_attrset *attrset, unsigned int follow_link,
    FN_status_asvc &as)
{
	return (a_attr_multi_get(name, attrset, follow_link, as));
}

int
FNSP_WeakSlashContext::a_attr_multi_modify_nns(const FN_string &name,
    const FN_attrmodlist &modl, unsigned int follow_link,
    FN_attrmodlist **mod,
    FN_status_asvc &as)
{
	return (a_attr_multi_modify(name, modl, follow_link, mod, as));
}

int
FNSP_WeakSlashContext::a_attr_bind_nns(const FN_string &name,
    const FN_ref &ref, const FN_attrset *attrs,
    unsigned int exclusive, FN_status_asvc &status)
{
	return (a_attr_bind(name, ref, attrs, exclusive, status));
}

FN_ext_searchlist *
FNSP_WeakSlashContext::a_attr_ext_search_nns(const FN_string &name,
    const FN_search_control *control,
    const FN_search_filter *filter,
    FN_status_asvc &status)
{
	return (a_attr_ext_search(name, control, filter, status));
}


FN_ref *
FNSP_WeakSlashContext::a_attr_create_subcontext(const FN_string &name,
    const FN_attrset *attrs, FN_status_asvc &as)
{
	unsigned context_type, current_context_type;
	unsigned representation_type;
	FN_identifier *ref_type;

	current_context_type = ns_impl->my_address->get_context_type();
	FN_attrset *rest_attrs = FNSP_get_create_params_from_attrs(attrs,
	    context_type, representation_type, &ref_type,
	    current_context_type);
	FN_ref *newref = 0;
	unsigned status;

	if (name.is_empty() || (representation_type != FNSP_normal_repr)) {
		status = FN_E_OPERATION_NOT_SUPPORTED;
	} else {
		// Child context inherits parent's reference type by default.
		if (ref_type == 0 && context_type == current_context_type) {
			ref_type = new FN_identifier(*(my_reference->type()));
		}
		newref = ns_impl->create_and_bind(name, context_type,
		    representation_type, status, FNSP_CHECK_NAME, ref_type,
		    rest_attrs);
	}
	if (status == FN_SUCCESS) {
		as.set_success();
	} else {
		as.set_error(status, my_reference, &name);
	}
	delete ref_type;
	delete rest_attrs;
	return (newref);
}

FN_ref *
FNSP_WeakSlashContext::a_attr_create_subcontext_nns(const FN_string &name,
    const FN_attrset *attrs,
    FN_status_asvc &stat)
{
	return (a_attr_create_subcontext(name, attrs, stat));
}

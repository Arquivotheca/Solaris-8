/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_HierContext.cc	1.4	97/08/15 SMI"

#include "FNSP_HierContext.hh"
#include "fnsp_utils.hh"
#include <xfn/fn_p.hh>

//  A FNSP_HierContext is derived from NS_ServiceContextAtomic.
//  A naming system composed of FNSP_HierContext supports a hierarchical
//  name space.  By the time processing gets to a FNSP_HierContext,
//  it only needs to deal  with 'atomic names'.  'nns pointers' may be
//  associated with each atomic  name.
//
//  The FNSP_HierContext itself may have an associatd nns pointer;
//  in this case, its binding could be found under the reserved name "FNS_nns"
//  in the context.  The bindings of names associated with the atomic names
//  are stored in the binding table of the atomic context.
//

static const FN_string empty_name((const unsigned char *)"");
static const FN_string FNSP_nns_name((unsigned char *)"_FNS_nns_");

FNSP_HierContext::~FNSP_HierContext()
{
	// subclasses cleanup
}

FN_ref *
FNSP_HierContext::get_ref(FN_status &stat) const
{
	FN_ref *answer = new FN_ref(*my_reference);

	if (answer)
		stat.set_success();
	else
		stat.set(FN_E_INSUFFICIENT_RESOURCES);

	return (answer);
}

FN_composite_name *
FNSP_HierContext::equivalent_name(
    const FN_composite_name &name,
    const FN_string &,
    FN_status &status)
{
	status.set(FN_E_OPERATION_NOT_SUPPORTED, my_reference, &name);
	return (0);
}

FN_ref *
FNSP_HierContext::resolve(const FN_string &name, FN_status_asvc &astat)
{
	FN_ref *answer = 0;
	unsigned status;
	answer = ns_impl->lookup_binding(name, status);
	if (status == FN_SUCCESS)
		astat.set_success();
	else if (name.compare(FNSP_nns_name) == 0)
		astat.set_error(status, my_reference);
	else
		astat.set_error(status, my_reference, &name);
	return (answer);
}

FN_string *
FNSP_HierContext::c_component_parser(const FN_string &n,
    FN_string **rest,
    FN_status_csvc& stat)
{
	FN_compound_name* parsed_name =  new
	    FN_compound_name_standard(*my_syntax, n);
	FN_string *answer = 0;

	if (parsed_name) {
		void *iter_pos;
		const FN_string *fst = parsed_name->first(iter_pos);
		if (rest) {
			FN_compound_name* rc = parsed_name->suffix(iter_pos);
			if (rc) {
				*rest = rc->string();
				delete rc;
			} else
				*rest = 0;
		}
		answer = new FN_string(*fst);
		delete parsed_name;
		if (answer == NULL)
			stat.set(FN_E_INSUFFICIENT_RESOURCES);
		else
			stat.set_success();
	} else {
		stat.set(FN_E_ILLEGAL_NAME);
	}

	return (answer);
}

FN_ref *
FNSP_HierContext::a_lookup(const FN_string &name,
    unsigned int /* lookup_flags */,
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
		answer = resolve(name, astat);

	return (answer);
}

FN_namelist*
FNSP_HierContext::a_list_names(FN_status_asvc &astat)
{
	FN_namelist *answer = 0;
	unsigned status;
	// listing all names bound in current context
	answer = ns_impl->list_names(status);
	if (status == FN_SUCCESS) {
		// if (answer)
		// answer->remove(FNSP_nns_name);
		astat.set_success();
	} else
		astat.set_error(status, my_reference);

	if (answer)
		return (answer);
	else
		return (0);
}

FN_bindinglist*
FNSP_HierContext::a_list_bindings(FN_status_asvc &astat)
{
	FN_bindinglist *answer = 0;
	unsigned status;
	// Get all bindings
	answer = ns_impl->list_bindings(status);

	if (status == FN_SUCCESS) {
		// if (answer)
		// answer->remove(FNSP_nns_name);
		astat.set_success();
	} else
		astat.set_error(status, my_reference);

	if (answer)
		return (answer);
	else
		return (0);
}

int
FNSP_HierContext::a_bind(const FN_string &name, const FN_ref &ref,
    unsigned BindFlags, FN_status_asvc &astat)
{
	FN_attrset empty_attrs;

	return (a_attr_bind(name, ref, &empty_attrs, BindFlags, astat));
}

int
FNSP_HierContext::a_unbind(const FN_string &name, FN_status_asvc &astat)
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
FNSP_HierContext::a_rename(const FN_string &name,
    const FN_composite_name &newname,
    unsigned BindFlags, FN_status_asvc &astat)
{
	// should do some checks against FNSP_nns_name ???
	unsigned status;
	FN_string *newtarget = 0;

	if (name.is_empty())
		status = FN_E_OPERATION_NOT_SUPPORTED; 	// cannot rename self
	else {
		// get string form of new name and check for legal name
		if (newname.count() == 1) {
			void *ip;
			const FN_string *newstr = newname.first(ip);
			FN_status_csvc cstat;
			FN_string**rest = NULL;

			newtarget = (newstr ? c_component_parser(*newstr,
			    rest, cstat) : 0);

			if (newtarget && rest == NULL) {
				/* do nothing */
				;
			} else {
				// do not support renaming to a compound name
				// or an empty name,
				// or an otherwise illegal name
				if (newtarget) {
					delete newtarget;
					newtarget = 0;
				}

				if (rest) {
					delete rest;
					rest = 0;
				}
				FN_string *newname_str = newname.string();
				if (newname_str) {
					astat.set_error(FN_E_ILLEGAL_NAME,
							my_reference,
							newname_str);
					delete newname_str;
				} else
					astat.set_error(FN_E_ILLEGAL_NAME,
							my_reference);
				return (0);
			}
		} else {
			// do not support renaming to a composite name
			FN_string *newname_str = newname.string();
			if (newname_str) {
				astat.set_error(FN_E_ILLEGAL_NAME,
						my_reference,
						newname_str);
				delete newname_str;
			} else
				astat.set_error(FN_E_ILLEGAL_NAME,
						my_reference);
			return (0);
		}

		status = ns_impl->rename_binding(name, *newtarget,
		    BindFlags);
	}
	if (status == FN_SUCCESS)
		astat.set_success();
	else
		astat.set_error(status, my_reference, &name);
	if (newtarget)
		delete newtarget;
	return (status == FN_SUCCESS);
}

FN_ref *
FNSP_HierContext::a_create_subcontext(const FN_string &name,
    FN_status_asvc &astat)
{
	return (a_attr_create_subcontext(name, NULL, astat));
}

int
FNSP_HierContext::a_destroy_subcontext(const FN_string &name,
    FN_status_asvc &astat)
{
	unsigned status;
	if (name.is_empty())
		status = FN_E_OPERATION_NOT_SUPPORTED;
	else
		status = ns_impl->destroy_and_unbind(name);

	if (status == FN_SUCCESS)
		astat.set_success();
	else
		astat.set_error(status, my_reference, &name);
	return (status == FN_SUCCESS);
}

FN_attrset*
FNSP_HierContext::a_get_syntax_attrs(FN_status_asvc& astat)
{
	FN_attrset* answer = my_syntax->get_syntax_attrs();

	if (answer)
		astat.set_success();
	else
		astat.set_error(FN_E_INSUFFICIENT_RESOURCES);
	return (answer);
}

int
FNSP_HierContext::is_following_link(
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
FNSP_HierContext::a_attr_get(const FN_string &aname,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_asvc &as)
{
	if (follow_link && is_following_link(aname, as)) {
		return (0);
	}

	// %%% need to handle empty name case

	unsigned status;
	FN_attrset *all = ns_impl->get_attrset(aname, status);
	FN_attribute *answer = NULL;

	if (all == NULL)
		status = FN_E_NO_SUCH_ATTRIBUTE;

	if (status != FN_SUCCESS) {
		as.set_error(status, my_reference, &aname);
	} else {
		// Get the required attribute
		const FN_attribute *stored_attr = all->get(id);
		if (stored_attr) {
			answer = new FN_attribute(*stored_attr);
			as.set_success();
		} else
			as.set_error(FN_E_NO_SUCH_ATTRIBUTE, my_reference,
			    &aname);
	}

	delete all;
	return (answer);
}

int
FNSP_HierContext::a_attr_modify(const FN_string &aname,
    unsigned int mod_op,
    const FN_attribute &attr, unsigned int follow_link,
    FN_status_asvc& as)
{
	if (follow_link && is_following_link(aname, as)) {
		return (0);
	}

	// %%% need to handle empty name case

	unsigned status = ns_impl->modify_attribute(aname, attr, mod_op);
	if (status == FN_SUCCESS) {
		as.set_success();
		return (1);
	}
	as.set_error(status, my_reference, &aname);
	return (0);
}

FN_valuelist*
FNSP_HierContext::a_attr_get_values(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_asvc &as)
{
	// follow_link handled by a_attr_get()

	// %%% need to handle empty name case

	FN_valuelist_svc *answer = 0;
	FN_attribute *attribute = a_attr_get(name, id, follow_link, as);
	if (as.is_success() && attribute != NULL)
		answer = new FN_valuelist_svc(attribute);

	return (answer);
}

FN_attrset*
FNSP_HierContext::a_attr_get_ids(const FN_string &aname,
    unsigned int follow_link, FN_status_asvc &as)
{
	if (follow_link && is_following_link(aname, as)) {
		return (0);
	}

	// %%% need to handle empty name case

	unsigned status;
	FN_attrset *all = ns_impl->get_attrset(aname, status);

	if (status != FN_SUCCESS) {
		as.set_error(status, my_reference, &aname);
		return (NULL);
	}
	if (all == NULL)
		all = new FN_attrset;

	as.set_success();
	return (all);
}

FN_multigetlist*
FNSP_HierContext::a_attr_multi_get(const FN_string &name,
    const FN_attrset *attrset, unsigned int follow_link,
    FN_status_asvc &as)
{
	// follow_link handled by get_ids()

	// %%% need to handle empty name case

	FN_attrset *all = a_attr_get_ids(name, follow_link, as);

	if (all == NULL)
		return (NULL);

	if (attrset == NULL) {
		return (new FN_multigetlist_svc(all));
	}

	FN_attrset *selection = FNSP_get_selected_attrset(*all, *attrset);
	delete all;
	if (selection)
		return (new FN_multigetlist_svc(selection));
	else
		return (NULL);
}

int
FNSP_HierContext::a_attr_multi_modify(const FN_string &name,
    const FN_attrmodlist &modlist, unsigned int follow_link,
    FN_attrmodlist **un_modlist,
    FN_status_asvc &as)
{
	if (follow_link && is_following_link(name, as)) {
		return (0);
	}

	// %%% need to handle empty name case

	void *ip;
	unsigned int mod_op, status;
	const FN_attribute *attribute;
	int num_mods = 0;

	for (attribute = modlist.first(ip, mod_op);
	    attribute != NULL;
	    attribute = modlist.next(ip, mod_op), ++num_mods) {
		// set follow_link to 0 to avoid repeated checking
		if (!(status = a_attr_modify(name, mod_op, *attribute, 0, as)))
			break;
	}
	if (attribute == NULL || num_mods == 0)	// modlist has been consumed
		return (status);

	if (un_modlist) {
		// copy unexecuted modifications to caller
		for ((*un_modlist) = new FN_attrmodlist;
		    attribute != NULL;
		    attribute = modlist.next(ip, mod_op)) {
			(*un_modlist)->add(mod_op, *attribute);
		}
	}
	return (status);
}

int
FNSP_HierContext::a_attr_bind(const FN_string &name, const FN_ref &ref,
    const FN_attrset *attrs, unsigned BindFlags, FN_status_asvc &astat)
{
	// should do some checks against FNSP_nns_name ???
	unsigned status;
	if (name.is_empty())
		status = FN_E_OPERATION_NOT_SUPPORTED; 	// cannot bind to self
	else
		status = ns_impl->add_binding(name, ref, attrs, BindFlags);

	if (status == FN_SUCCESS)
		astat.set_success();
	else
		astat.set_error(status, my_reference, &name);
	return (status == FN_SUCCESS);
}


FN_searchlist *
FNSP_HierContext::a_attr_search(
    const FN_attrset *match_attrs,
    unsigned int return_ref,
    const FN_attrset *return_attr_ids,
    FN_status_asvc &astat)
{
	unsigned int status;
	FN_searchlist *matches = ns_impl->search_attrset(
	    match_attrs, return_ref, return_attr_ids, status);

	if (status == FN_SUCCESS) {
		astat.set_success();
	} else {
		astat.set_error(status, my_reference);
	}

	if (matches)
		return (matches);
	else
		return (NULL);
}

FN_ext_searchlist *
FNSP_HierContext::a_attr_ext_search(
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

	// Check for scope
	if (control && ((control->scope() == FN_SEARCH_SUBTREE) ||
	    (control->scope() == FN_SEARCH_CONSTRAINED_SUBTREE))) {
		// %%% needs work
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, my_reference, &name);
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
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, my_reference, &name);
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


FN_ref *
FNSP_HierContext::a_lookup_nns(const FN_string &name,
    unsigned int /* lookup_flags */, FN_status_asvc &astat)
{
	if (name.is_empty()) {
		FN_ref *answer = resolve(FNSP_nns_name, astat);
		return (answer);
	} else {
		// resolve name first and then operate on its nns
		FN_ref *name_ref = resolve(name, astat);
		if (astat.is_success()) {
			astat.set_continue(*name_ref, *my_reference,
			    &empty_name);
			delete name_ref;
		}
		return (0);
	}
}

int
FNSP_HierContext::a_bind_nns(const FN_string &name,
    const FN_ref &ref, unsigned bind_flags, FN_status_asvc &astat)
{
	FN_attrset empty_attrs;

	return (a_attr_bind_nns(name, ref, &empty_attrs, bind_flags, astat));
}

int
FNSP_HierContext::a_unbind_nns(const FN_string &name, FN_status_asvc &astat)
{
	if (name.is_empty()) {
		unsigned status;
		status = ns_impl->remove_binding(FNSP_nns_name);

		if (status == FN_SUCCESS)
			astat.set_success();
		else
			astat.set_error(status, my_reference);
		return (status == FN_SUCCESS);
	} else {
		// resolve name first and then operate on its nns
		FN_ref *name_ref = resolve(name, astat);
		if (astat.is_success()) {
			astat.set_continue(*name_ref, *my_reference,
			    &empty_name);
			delete name_ref;
		}
		return (0);
	}
}

int
FNSP_HierContext::a_rename_nns(const FN_string &name,
    const FN_composite_name &, unsigned, FN_status_asvc& astat)
{
	if (name.is_empty()) {
		astat.set_error(FN_E_OPERATION_NOT_SUPPORTED);
		return (0);
	} else {
		// resolve name first and then operate on its nns
		// even though rename of nns is not supported, this
		// resolution step could point out other errors
		FN_ref *name_ref = resolve(name, astat);
		if (astat.is_success()) {
			astat.set_continue(*name_ref, *my_reference,
			    &empty_name);
			delete name_ref;
		}
		return (0);
	}
}

FN_ref *
FNSP_HierContext::a_create_subcontext_nns(const FN_string &name,
    FN_status_asvc &astat)
{
	return (a_attr_create_subcontext_nns(name, NULL, astat));
}

int
FNSP_HierContext::a_destroy_subcontext_nns(const FN_string &name,
    FN_status_asvc &astat)
{
	if (name.is_empty()) {
		unsigned status;
		status = ns_impl->destroy_and_unbind(FNSP_nns_name);

		if (status == FN_SUCCESS)
			astat.set_success();
		else
			astat.set_error(status, my_reference);
		return (status == FN_SUCCESS);
	} else {
		// resolve 'name'
		FN_ref *name_ref = resolve(name, astat);
		if (astat.is_success())
			astat.set_continue(*name_ref, *my_reference,
			    &empty_name);
		return (0);
	}
}

FN_attribute*
FNSP_HierContext::a_attr_get_nns(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_asvc &as)
{
	if (name.is_empty()) {
		if (follow_link && is_following_link(FNSP_nns_name, as)) {
			return (NULL);
		}

		unsigned status;
		FN_attrset *all = ns_impl->get_attrset(FNSP_nns_name,
		    status);
		FN_attribute *answer = NULL;

		if (all == NULL)
			status = FN_E_NO_SUCH_ATTRIBUTE;

		if (status != FN_SUCCESS) {
			as.set_error(status, my_reference);
		} else {
			// Get the required attribute
			const FN_attribute *stored_attr = all->get(id);
			if (stored_attr != NULL) {
				as.set_success();
				answer = new FN_attribute(*stored_attr);
			} else
				as.set_error(FN_E_NO_SUCH_ATTRIBUTE,
				    my_reference);
		}
		delete all;
		return (answer);
	} else {
		// resolve name first and then operate on its nns
		FN_ref *name_ref = resolve(name, as);
		if (as.is_success()) {
			as.set_continue(*name_ref, *my_reference, &empty_name);
			delete name_ref;
		}
	}
	return (NULL);
}


int
FNSP_HierContext::a_attr_modify_nns(const FN_string &name,
    unsigned int mod_op,
    const FN_attribute &attr, unsigned int follow_link,
    FN_status_asvc& as)
{
	if (name.is_empty()) {
		if (follow_link && is_following_link(FNSP_nns_name, as)) {
			return (0);
		}

		unsigned status = ns_impl->modify_attribute(
			FNSP_nns_name, attr, mod_op);
		if (status == FN_SUCCESS) {
			as.set_success();
			return (1);
		}
		as.set_error(status, my_reference);
		return (0);
	} else {
		// resolve name first and then operate on its nns
		FN_ref *name_ref = resolve(name, as);
		if (as.is_success()) {
			as.set_continue(*name_ref, *my_reference, &empty_name);
			delete name_ref;
		}
		return (0);
	}
}


FN_valuelist*
FNSP_HierContext::a_attr_get_values_nns(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_asvc &as)
{
	// follow link and name.is_empty() is handled by a_attr_get_nns
	FN_valuelist_svc *answer = 0;
	FN_attribute *attribute = a_attr_get_nns(name, id,
	    follow_link, as);
	if (as.is_success() && attribute != NULL)
		answer = new FN_valuelist_svc(attribute);
	return (answer);
}

FN_attrset*
FNSP_HierContext::a_attr_get_ids_nns(const FN_string &name,
    unsigned int follow_link, FN_status_asvc &as)
{
	if (name.is_empty()) {
		if (follow_link && is_following_link(FNSP_nns_name, as)) {
			return (0);
		}

		unsigned status;
		FN_attrset *all = ns_impl->get_attrset(FNSP_nns_name, status);

		if (status != FN_SUCCESS) {
			as.set_error(status, my_reference);
			return (0);
		}
		if (all == NULL)
			all = new FN_attrset;

		as.set_success();
		return (all);
	} else {
		// resolve name first and then operate on its nns
		FN_ref *name_ref = resolve(name, as);
		if (as.is_success()) {
			as.set_continue(*name_ref, *my_reference, &empty_name);
			delete name_ref;
		}
	}
	return (NULL);
}

FN_multigetlist*
FNSP_HierContext::a_attr_multi_get_nns(const FN_string &name,
    const FN_attrset *attrset, unsigned int follow_link,
    FN_status_asvc &as)
{
	// follow_link and empty name case handled by get_ids_nns()

	FN_attrset *all = a_attr_get_ids_nns(name, follow_link, as);
	if (all == NULL)
		return (NULL);

	if (attrset == NULL) {
		return (new FN_multigetlist_svc(all));
	}

	FN_attrset *selection = FNSP_get_selected_attrset(*all, *attrset);
	delete all;
	if (selection)
		return (new FN_multigetlist_svc(selection));
	else
		return (NULL);
}

int
FNSP_HierContext::a_attr_multi_modify_nns(const FN_string &name,
    const FN_attrmodlist &modlist, unsigned int follow_link,
    FN_attrmodlist **un_modlist,
    FN_status_asvc &as)
{
	if (name.is_empty()) {
		if (follow_link && is_following_link(FNSP_nns_name, as))
			return (0);

		void *ip;
		unsigned int mod_op, status;
		const FN_attribute *attribute;
		int num_mods = 0;

		for (attribute = modlist.first(ip, mod_op);
		    attribute != NULL;
		    attribute = modlist.next(ip, mod_op), ++num_mods) {
			// set follow_link to 0 to avoid repeated checking
			if (!(status = a_attr_modify_nns(name, mod_op,
			    *attribute, 0, as)))
				break;
		}
		if (attribute == NULL || num_mods == 0)
			return (status);

		if (un_modlist) {
			for ((*un_modlist) = new FN_attrmodlist;
			    attribute != NULL;
			    attribute = modlist.next(ip, mod_op))
				(*un_modlist)->add(mod_op, *attribute);
		}
		return (status);
	} else {
		// resolve name first and then operate on its nns
		FN_ref *name_ref = resolve(name, as);
		if (as.is_success()) {
			as.set_continue(*name_ref, *my_reference, &empty_name);
			delete name_ref;
		}
	}
	return (0);
}

FN_ref *
FNSP_HierContext::a_attr_create_subcontext(const FN_string &name,
    const FN_attrset *attrs,
    FN_status_asvc &as)
{
	unsigned context_type, current_context_type;
	unsigned representation_type;
	FN_identifier *ref_type;

	current_context_type = ns_impl->my_address->get_context_type();

	FN_attrset *rest_attrs = FNSP_get_create_params_from_attrs(attrs,
	    context_type, representation_type, &ref_type,
	    current_context_type);

	if (name.is_empty() ||
	    context_type != current_context_type ||
	    representation_type != FNSP_normal_repr ||
	    ref_type != 0) {
		as.set_error(FN_E_OPERATION_NOT_SUPPORTED,
		    my_reference, &name);
		delete ref_type;
		delete rest_attrs;
		return (0);
	}

	unsigned int status;
	FN_ref *newref = ns_impl->create_and_bind(
	    name,
	    current_context_type,
	    representation_type,
	    status,
	    FNSP_CHECK_NAME,
	    NULL, /* ref type */
	    rest_attrs);

	delete rest_attrs;
	return (newref);
}

FN_ref *
FNSP_HierContext::a_attr_create_subcontext_nns(const FN_string &name,
    const FN_attrset *attrs,
    FN_status_asvc &astat)
{
	unsigned context_type;
	unsigned representation_type;
	FN_identifier *ref_type;
	FN_attrset *rest_attrs = FNSP_get_create_params_from_attrs(attrs,
	    context_type, representation_type, &ref_type,
	    FNSP_nsid_context);

	if (context_type != FNSP_nsid_context ||
	    representation_type != FNSP_normal_repr ||
	    ref_type != 0) {
		astat.set(FN_E_OPERATION_NOT_SUPPORTED, my_reference);
		delete ref_type;
		delete rest_attrs;
		return (0);
	}

	if (name.is_empty()) {
		unsigned status;
		FN_ref *newref =
			ns_impl->create_and_bind(FNSP_nns_name,
			    FNSP_nsid_context, FNSP_normal_repr, status,
			    FNSP_DONT_CHECK_NAME, NULL, rest_attrs);
		delete rest_attrs;
		if (status == FN_SUCCESS)
			astat.set_success();
		else
			astat.set_error(status, my_reference);
		return (newref);
	} else {
		delete rest_attrs;
		// resolve name first and then operate on its nns
		FN_ref *name_ref = resolve(name, astat);
		if (astat.is_success()) {
			astat.set_continue(*name_ref, *my_reference,
			    &empty_name);
			delete name_ref;
		}
		return (0);
	}
}

int
FNSP_HierContext::a_attr_bind_nns(const FN_string &name,
    const FN_ref &ref, const FN_attrset *attrs,
    unsigned bind_flags, FN_status_asvc &astat)
{
	if (name.is_empty()) {
		unsigned status;
		status = ns_impl->add_binding(FNSP_nns_name,
		    ref, attrs, bind_flags);

		if (status == FN_SUCCESS)
			astat.set_success();
		else
			astat.set_error(status, my_reference);
		return (status == FN_SUCCESS);
	} else {
		// resolve name first and then operate on nns there
		FN_ref *name_ref = resolve(name, astat);
		if (astat.is_success()) {
			astat.set_continue(*name_ref, *my_reference,
			    &empty_name);
			delete name_ref;
		}
		return (0);
	}
}

FN_ext_searchlist *
FNSP_HierContext::a_attr_ext_search_nns(const FN_string &name,
    const FN_search_control * /* control */,
    const FN_search_filter * /* filter */,
    FN_status_asvc &status)
{
	// %%% needs work
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, my_reference, &name);
	return (0);
}

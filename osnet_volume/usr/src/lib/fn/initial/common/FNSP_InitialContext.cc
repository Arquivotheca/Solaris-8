/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)FNSP_InitialContext.cc	1.9	97/08/09 SMI"

#include "FNSP_InitialContext.hh"
#include <xfn/fn_p.hh>

// This file contains the code implementing the context service operations
// of FNSP_InitialContext.  That is, those functions that are defined as
// virtual member functions in FN_ctx_single_component_static_svc and
// re-defined non-virtual in FNSP_InitialContext.

static const FN_string empty_string((unsigned char *)"");

static inline int
not_supported(const FN_string &name, FN_status_csvc &status)
{
	status.set(FN_E_OPERATION_NOT_SUPPORTED);
	status.set_remaining_name(&name);
	return (0);
}

#if 0
static inline int
continue_in_nns(const FN_string &name, FN_status_csvc &status)
{
	FN_ref *ref = c_lookup(name, 0, status);
	if (status.is_success()) {
		status.set_error(FN_E_CONTINUE, *ref, empty_string);
		if (ref) delete ref;
	}
	// Note: if the status was not Success, we just pass up the status
	// returned from the lookup.
	return (0);
}
#endif

// If first two components are "/..." (i.e. { "", "..."}), treat that
// equivalent as "...".  Otherwise, do same as for strong separation
// (i.e. returns copy of first component).
// 'rn' is set to remaining components.
FN_composite_name *
FNSP_InitialContext::p_component_parser(const FN_composite_name &n,
    FN_composite_name **rn,
    FN_status_psvc &ps)
{
	void *p;
	const FN_string *first_comp = n.first(p);
	ps.set_success();

	if (first_comp) {
		if (first_comp->is_empty()) {
			// check second component to see if it is "..."
			const FN_string *second_comp = n.next(p);
			if (second_comp &&
			    second_comp->compare(
			    (unsigned char *)"...") == 0)
				// skip empty component
				first_comp = second_comp;
			else {
				// encountered something other than '...'
				if (rn)
					// need to do this to reset 'p'
					first_comp = n.first(p);
			}
		}

		if (rn)
			*rn = n.suffix(p);
		// separate construction of answer (instead of making
		// one call to
		// FN_composite_name(const FN_string &)constructor)
		// so that the constructor try to parse it and eat up any
		// quotes/escapes inside the component.
		FN_composite_name *answer = new FN_composite_name();
		if (answer == 0)
			ps.set_code(FN_E_INSUFFICIENT_RESOURCES);
		else
			answer->append_comp(*first_comp);
		return (answer);
	} else {
		ps.set_code(FN_E_ILLEGAL_NAME);
		return (0);
	}
}

FNSP_InitialContext::Entry *
FNSP_InitialContext::find_entry(const FN_string &name)
{
	Table* table = 0;
	Entry *e = 0;
	int i;

	for (i = 0; e == 0 && i < FNSP_NUMBER_TABLES; i++) {
		if ((table = tables[i]) == 0)
			continue;
		e = table->find(name);
	}
	return (e);
}

FN_ref *
FNSP_InitialContext::c_lookup_nns(const FN_string &name,
    unsigned int /* lookup_flags */, FN_status_csvc &status)
{
	FN_ref *ref = 0;
	unsigned status_code;
	Entry *e;

	if (name.is_empty()) {
		// There can be no nns associated with IC
		status.set(FN_E_NAME_NOT_FOUND);
		status.set_remaining_name(&name);
		return (0);
	}
	if (e = find_entry(name)) {
		ref = e->reference(authoritative, status_code);
		status.set(status_code);
		status.set_remaining_name(&name);
	} else {
		status.set(FN_E_NAME_NOT_FOUND);
		status.set_remaining_name(&name);
	}

	// Names in the initial context should never be links

	return (ref);
}


FN_ref *
FNSP_InitialContext::c_lookup(const FN_string &name,
    unsigned int lookup_flags, FN_status_csvc &status)
{
	if (name.is_empty()) {
		// lookup of the empty name would normally return
		// a reference to this context.
		// The initial context has no reference.
		// lookup of the empty name is not supported.
		// note however that lookup_nns of the empty name
		// just returns
		// "not found", because that is effectively looking up "/".
		return ((FN_ref *)not_supported(name, status));
	}

	return (c_lookup_nns(name, lookup_flags, status));
}

FN_namelist*
FNSP_InitialContext::c_list_names(const FN_string &name,
    FN_status_csvc &status)
{
	if (!name.is_empty()) {
		// if the name is not empty, we must look it up and pass
		// FN_E_SPI_CONTINUE up with the reference

		FN_ref *ref = c_lookup(name, 0, status);
		if (status.is_success()) {
			status.set_error(FN_E_SPI_CONTINUE, *ref,
					    empty_string);
			delete ref;
		}

		// Note: if the status was not Success, we just pass
		// up the status returned from the lookup.
		return (0);
	}

	// Name was empty, we are  listing names in the IC

	FN_nameset *bound_names = new FN_nameset;
	if (bound_names == 0) {
		status.set(FN_E_INSUFFICIENT_RESOURCES);
		return (0);
	}
	IterationPosition iter_pos;
	Table* table = 0;
	Entry *e = 0;
	int i;

	for (i = 0; i < FNSP_NUMBER_TABLES; i++) {
		if ((table = tables[i]) == 0)
			continue;
		for (e = table->first(iter_pos);
		    e != NULL;
		    e = table->next(iter_pos)) {
			FN_ref *ref;
			unsigned status_code;
			// if the name in the entry is bound
			if (ref = e->reference(authoritative, status_code)) {

				// add it to the set of bound names
				void *iter_pos;
				const FN_string *nn;

				for (nn = e->first_name(iter_pos);
				    nn != NULL;
				    nn = e->next_name(iter_pos)) {
					if (!(bound_names->add(*nn))) {
						status.set(
						FN_E_INSUFFICIENT_RESOURCES);
						delete bound_names;
						delete ref;
						return (0);
					}
				}
				delete ref;
			}
		}
	}

	status.set_success();
	return (new FN_namelist_svc(bound_names));
}

FN_bindinglist *
FNSP_InitialContext::c_list_bindings(const FN_string &name,
    FN_status_csvc &status)
{
	if (!name.is_empty()) {
		// if the name is not empty, we must look it up and
		// pass FN_E_SPI_CONTINUE up with the reference

		FN_ref *ref = c_lookup(name, 0, status);
		if (status.is_success()) {
			status.set_error(FN_E_SPI_CONTINUE,
					 *ref, empty_string);
			delete ref;
		}
		return (0);
	}

	// Name was empty, listing bindings of IC
	FN_bindingset *bindings = new FN_bindingset;
	if (bindings == 0) {
		status.set(FN_E_INSUFFICIENT_RESOURCES);
		return (0);
	}

	IterationPosition iter_pos;
	FN_ref *ref;
	unsigned status_code;
	Table* table = 0;
	Entry *e = 0;
	int i;

	for (i = 0; i < FNSP_NUMBER_TABLES; i++) {
		if ((table = tables[i]) == 0)
			continue;

		for (e = table->first(iter_pos);
		    e != NULL;
		    e = table->next(iter_pos)) {
			// if the name in the entry is bound
			if (ref = e->reference(authoritative, status_code)) {
				void *iter_pos;
				unsigned int couldnotadd = 0;
				const FN_string *nn;

				for (nn = e->first_name(iter_pos);
				    nn != NULL;
				    nn = e->next_name(iter_pos)) {
					// add all alias names
					if (!(bindings->add(*nn, *ref))) {
						status.set(
						FN_E_INSUFFICIENT_RESOURCES);
						delete ref;
						delete bindings;
						break;
					}
				}
				delete ref;
			}
		}
	}

	if (!status.is_success()) {
		delete bindings;
		return (0);
	}

	status.set_success();
	return (new FN_bindinglist_svc(bindings));
}


// Flat, case-insensitive.
static const FN_syntax_standard
FNSP_InitialContext_syntax(FN_SYNTAX_STANDARD_DIRECTION_FLAT,
    FN_STRING_CASE_INSENSITIVE);

// not yet supported, but should be
FN_attrset*
FNSP_InitialContext::c_get_syntax_attrs(const FN_string &name,
    FN_status_csvc &status)
{
	FN_attrset* answer = 0;
	if (name.is_empty()) {
		// Asking for syntax of initial context itself
		answer = FNSP_InitialContext_syntax.get_syntax_attrs();
		status.set_success();
	} else {
		// Asking for syntax of contexts of names bound in IC
		FN_ref *ref = c_lookup_nns(name, 0, status);
		if (status.is_success()) {
			status.set_error(FN_E_SPI_CONTINUE, *ref,
					    empty_string);
			delete ref;
		}
	}

	if (answer == 0 && status.is_success())
		status.set(FN_E_INSUFFICIENT_RESOURCES);

	return (answer);
}

FN_attribute *
FNSP_InitialContext::c_attr_get(const FN_string &name,
    const FN_identifier &,
    unsigned int,
    FN_status_csvc &status)
{
	if (name.is_empty())
		return ((FN_attribute *)not_supported(name, status));

	FN_ref *ref = c_lookup(name, 0, status);
	if (status.is_success()) {
		status.set_error(FN_E_SPI_CONTINUE, *ref,
				 empty_string);
		delete ref;
	}
	return (0);
}

int
FNSP_InitialContext::c_attr_modify(const FN_string &name,
    unsigned int,
    const FN_attribute &,
    unsigned int,
    FN_status_csvc &status)
{
	if (name.is_empty())
		return (not_supported(name, status));

	FN_ref *ref = c_lookup(name, 0, status);
	if (status.is_success()) {
		status.set_error(FN_E_SPI_CONTINUE, *ref,
				 empty_string);
		delete ref;
	}
	return (0);
}

FN_valuelist*
FNSP_InitialContext::c_attr_get_values(const FN_string &name,
    const FN_identifier &,
    unsigned int,
    FN_status_csvc &status)
{
	if (name.is_empty())
		return ((FN_valuelist *)not_supported(name, status));

	FN_ref *ref = c_lookup(name, 0, status);
	if (status.is_success()) {
		status.set_error(FN_E_SPI_CONTINUE, *ref,
				 empty_string);
		delete ref;
	}
	return (0);
}

FN_attrset*
FNSP_InitialContext::c_attr_get_ids(const FN_string &name,
    unsigned int,
    FN_status_csvc &status)
{
	if (name.is_empty())
		return ((FN_attrset *)not_supported(name, status));

	FN_ref *ref = c_lookup(name, 0, status);
	if (status.is_success()) {
		status.set_error(FN_E_SPI_CONTINUE, *ref,
				 empty_string);
		delete ref;
	}
	return (0);
}

FN_multigetlist*
FNSP_InitialContext::c_attr_multi_get(const FN_string &name,
    const FN_attrset *,
    unsigned int,
    FN_status_csvc &status)
{
	if (name.is_empty())
		return ((FN_multigetlist *)not_supported(name, status));

	FN_ref *ref = c_lookup(name, 0, status);
	if (status.is_success()) {
		status.set_error(FN_E_SPI_CONTINUE, *ref,
				 empty_string);
		delete ref;
	}
	return (0);
}

int
FNSP_InitialContext::c_attr_multi_modify(const FN_string &name,
    const FN_attrmodlist&,
    unsigned int,
    FN_attrmodlist**,
    FN_status_csvc &status)
{
	if (name.is_empty())
		return (not_supported(name, status));

	FN_ref *ref = c_lookup(name, 0, status);
	if (status.is_success()) {
		status.set_error(FN_E_SPI_CONTINUE, *ref,
				 empty_string);
		delete ref;
	}
	return (0);
}


FN_namelist*
FNSP_InitialContext::c_list_names_nns(const FN_string &name,
    FN_status_csvc &status)
{
	FN_ref *ref = c_lookup_nns(name, 0, status);
	if (status.is_success()) {
		status.set(FN_E_SPI_CONTINUE, ref);
		if (ref) delete ref;
	}
	// Note: if the status was not Success, we just pass up the status
	// returned from the lookup.
	return (0);
}


FN_bindinglist*
FNSP_InitialContext::c_list_bindings_nns(const FN_string &name,
    FN_status_csvc &status)
{
	FN_ref *ref = c_lookup_nns(name, 0, status);
	if (status.is_success()) {
		status.set(FN_E_SPI_CONTINUE, ref);
		if (ref) delete ref;
	}
	// Note: if the status was not Success, we just pass up the status
	// returned from the lookup.
	return (0);
}

int
FNSP_InitialContext::c_bind_nns(const FN_string &name, const FN_ref &,
    unsigned /* BindFlags */, FN_status_csvc &status)
{
	return (not_supported(name, status));
}

int
FNSP_InitialContext::c_unbind_nns(const FN_string &name, FN_status_csvc &status)
{
	return (not_supported(name, status));
}


int
FNSP_InitialContext::c_rename_nns(const FN_string &name,
    const FN_composite_name &,
    unsigned /* BindFlags */, FN_status_csvc &status)
{
	return (not_supported(name, status));
}


FN_ref *
FNSP_InitialContext::c_create_subcontext_nns(const FN_string &name,
    FN_status_csvc &status)
{
	return ((FN_ref *)not_supported(name, status));
}


int
FNSP_InitialContext::c_destroy_subcontext_nns(const FN_string &name,
    FN_status_csvc &status)
{
	return (not_supported(name, status));
}

FN_attrset*
FNSP_InitialContext::c_get_syntax_attrs_nns(const FN_string &name,
    FN_status_csvc &status)
{
	FN_ref *ref = c_lookup_nns(name, 0, status);
	if (status.is_success()) {
		status.set(FN_E_SPI_CONTINUE, ref);
		if (ref) delete ref;
	}
	// Note: if the status was not Success, we just pass up the status
	// returned from the lookup.
	return (0);
}



FN_attribute *
FNSP_InitialContext::c_attr_get_nns(const FN_string &name,
    const FN_identifier &,
    unsigned int,
    FN_status_csvc &cs)
{
	return ((FN_attribute *)not_supported(name, cs));
}

int
FNSP_InitialContext::c_attr_modify_nns(const FN_string &name,
    unsigned int,
    const FN_attribute &,
    unsigned int,
    FN_status_csvc &cs)
{
	return (not_supported(name, cs));
}

FN_valuelist*
FNSP_InitialContext::c_attr_get_values_nns(const FN_string &name,
    const FN_identifier &,
    unsigned int,
    FN_status_csvc &cs)
{
	return ((FN_valuelist *)not_supported(name, cs));
}

FN_attrset*
FNSP_InitialContext::c_attr_get_ids_nns(const FN_string &name,
    unsigned int,
    FN_status_csvc &cs)
{
	return ((FN_attrset *)not_supported(name, cs));
}

FN_multigetlist*
FNSP_InitialContext::c_attr_multi_get_nns(const FN_string &name,
    const FN_attrset *,
    unsigned int,
    FN_status_csvc &cs)
{
	return ((FN_multigetlist *)not_supported(name, cs));
}

int
FNSP_InitialContext::c_attr_multi_modify_nns(const FN_string &name,
    const FN_attrmodlist&,
    unsigned int,
    FN_attrmodlist**,
    FN_status_csvc &cs)
{
	return (not_supported(name, cs));
}

int
FNSP_InitialContext::c_attr_bind_nns(const FN_string &name,
			    const FN_ref &,
			    const FN_attrset *,
			    unsigned int,
			    FN_status_csvc &status)
{
	return (not_supported(name, status));
}

FN_ref *
FNSP_InitialContext::c_attr_create_subcontext_nns(const FN_string &name,
					    const FN_attrset *,
					    FN_status_csvc &status)
{
	return ((FN_ref *)not_supported(name, status));
}

FN_searchlist *
FNSP_InitialContext::c_attr_search_nns(const FN_string &name,
				    const FN_attrset *,
				    unsigned int,
				    const FN_attrset *,
				    FN_status_csvc &status)
{
	FN_ref *ref = c_lookup_nns(name, 0, status);
	if (status.is_success()) {
		status.set(FN_E_SPI_CONTINUE, ref);
		if (ref) delete ref;
	}
	// Note: if the status was not Success, we just pass up the status
	// returned from the lookup.
	return (0);
}

FN_ext_searchlist *
FNSP_InitialContext::c_attr_ext_search_nns(const FN_string &name,
    const FN_search_control *,
    const FN_search_filter *,
    FN_status_csvc &status)
{
	FN_ref *ref = c_lookup_nns(name, 0, status);
	if (status.is_success()) {
		status.set(FN_E_SPI_CONTINUE, ref);
		if (ref) delete ref;
	}
	// Note: if the status was not Success, we just pass up the status
	// returned from the lookup.
	return (0);
}

int
FNSP_InitialContext::c_bind(const FN_string &name, const FN_ref &,
    unsigned /* BindFlags */, FN_status_csvc &status)
{
	return (not_supported(name, status));
}


int
FNSP_InitialContext::c_unbind(const FN_string &name, FN_status_csvc &status)
{
	return (not_supported(name, status));
}

int
FNSP_InitialContext::c_rename(const FN_string &name,
    const FN_composite_name &,
    unsigned /* BindFlags */, FN_status_csvc &status)
{
	return (not_supported(name, status));
}


FN_ref *
FNSP_InitialContext::c_create_subcontext(const FN_string &name,
    FN_status_csvc &status)
{
	return ((FN_ref *)not_supported(name, status));
}

int
FNSP_InitialContext::c_destroy_subcontext(const FN_string &name,
    FN_status_csvc &status)
{
	return (not_supported(name, status));
}

int
FNSP_InitialContext::c_attr_bind(const FN_string &name,
			    const FN_ref &,
			    const FN_attrset *,
			    unsigned int,
			    FN_status_csvc &status)
{
	return (not_supported(name, status));
}

FN_ref *
FNSP_InitialContext::c_attr_create_subcontext(const FN_string &name,
					    const FN_attrset *,
					    FN_status_csvc &status)
{
	return ((FN_ref *)not_supported(name, status));
}

FN_searchlist *
FNSP_InitialContext::c_attr_search(const FN_string &name,
				    const FN_attrset *,
				    unsigned int,
				    const FN_attrset *,
				    FN_status_csvc &status)
{
	if (!name.is_empty()) {
		// if the name is not empty, we must look it up and pass
		// FN_E_SPI_CONTINUE up with the reference

		FN_ref *ref = c_lookup(name, 0, status);
		if (status.is_success()) {
			status.set_error(FN_E_SPI_CONTINUE, *ref,
					    empty_string);
			delete ref;
		}

		// Note: if the status was not Success, we just pass
		// up the status returned from the lookup.
		return (0);
	}

	/* otherwise cannot search within Initial Context */
	return ((FN_searchlist *)not_supported(name, status));
}

FN_ext_searchlist *
FNSP_InitialContext::c_attr_ext_search(const FN_string &name,
    const FN_search_control *,
    const FN_search_filter *,
    FN_status_csvc &status)
{
	if (!name.is_empty()) {
		// if the name is not empty, we must look it up and pass
		// FN_E_SPI_CONTINUE up with the reference

		FN_ref *ref = c_lookup(name, 0, status);
		if (status.is_success()) {
			status.set_error(FN_E_SPI_CONTINUE, *ref,
					    empty_string);
			delete ref;
		}

		// Note: if the status was not Success, we just pass
		// up the status returned from the lookup.
		return (0);
	}

	/* otherwise, cannot search within Initial Context */
	return ((FN_ext_searchlist *)not_supported(name, status));
}


FN_ref *
FNSP_InitialContext::get_ref(FN_status &status) const
{
	status.set(FN_E_OPERATION_NOT_SUPPORTED);
	return (0);
}

// ******************* Equivalent name routines

// return prefix/suffix
static inline FN_composite_name *
concat_cname(const FN_string &prefix, FN_composite_name *suffix)
{
	if (suffix == NULL)
		suffix = new FN_composite_name;
	suffix->prepend_comp(prefix);
	return (suffix);
}

static inline int
is_host_token(const FN_string *tk)
{
	if (tk == NULL)
		return (0);

	// %%% should really refer to 'stored_names' of 'host' Entry
	return (
	tk->compare((unsigned char *)"host", FN_STRING_CASE_INSENSITIVE) == 0 ||
	tk->compare((unsigned char *)"_host", FN_STRING_CASE_INSENSITIVE) == 0);
}

static inline int
is_org_token(const FN_string *tk)
{
	if (tk == NULL)
		return (0);

	// %%% should really refer to 'stored_names' of 'org' Entry
	return (
	    tk->compare((unsigned char *)"org",
		FN_STRING_CASE_INSENSITIVE) == 0 ||
	    tk->compare((unsigned char *)"orgunit",
		FN_STRING_CASE_INSENSITIVE) == 0 ||
	    tk->compare((unsigned char *)"_orgunit",
		FN_STRING_CASE_INSENSITIVE) == 0);
}

static inline int
is_user_token(const FN_string *tk)
{
	if (tk == NULL)
		return (0);

	// %%% should really refer to 'stored_names' of 'user' Entry
	return (
	tk->compare((unsigned char *)"user", FN_STRING_CASE_INSENSITIVE) == 0 ||
	tk->compare((unsigned char *)"_user", FN_STRING_CASE_INSENSITIVE) == 0);
}



FN_composite_name *
FNSP_InitialContext::equivalent_name(const FN_composite_name &name,
				    const FN_string &leading_name,
				    FN_status &status)
{
	void *ip;
	FN_composite_name *answer = NULL;
	const FN_string *orig_head = name.first(ip);

	if (orig_head == NULL) {
		status.set(FN_E_NO_EQUIVALENT_NAME, 0, 0, &name);
		return (NULL);  // given name not bound in IC
	}

	Entry *orig_entry = find_entry(*orig_head);
	Entry *lead_entry = NULL;

	if (orig_entry == NULL)  {
		status.set(FN_E_NAME_NOT_FOUND, 0, 0, &name);
		return (NULL);  // given name not bound in IC
	}

	lead_entry = find_entry(leading_name);
	if (lead_entry == NULL) {
		// given leader does not exist in IC, so cannot
		// generate equivalent form
		goto cleanup;
	}

	FNSP_IC_name_type orig_type, lead_type;
	orig_type = orig_entry->name_type();
	lead_type = lead_entry->name_type();

	if (orig_type == lead_type) {
		// leaders are equivalent; simply replace leader
		status.set_success();
		answer = concat_cname(leading_name, name.suffix(ip));
		return (answer);
	}

	switch (orig_type) {
	case FNSP_THISORGUNIT:
		answer = thisorgunit_equivs(lead_type, leading_name,
		    lead_entry, orig_entry, name, ip);
		break;
	case FNSP_THISHOST:
		answer = thishost_equivs(lead_type, leading_name,
		    lead_entry, orig_entry, name, ip);
		break;
	case FNSP_THISENS:
		answer = thisens_equivs(lead_type, leading_name,
		    lead_entry, orig_entry, name, ip);
		break;
	case FNSP_ORGUNIT:
		answer = orgunit_equivs(lead_type, leading_name,
		    lead_entry, orig_entry, name, ip);
		break;
	case FNSP_SITE:
		answer = site_equivs(lead_type, leading_name,
		    lead_entry, orig_entry, name, ip);
		break;
	case FNSP_USER:
		answer = user_equivs(lead_type, leading_name,
		    lead_entry, orig_entry, name, ip);
		break;
	case FNSP_HOST:
		answer = host_equivs(lead_type, leading_name,
		    lead_entry, orig_entry, name, ip);
		break;
	case FNSP_MYSELF:
		answer = myself_equivs(lead_type, leading_name,
		    lead_entry, orig_entry, name, ip);
		break;
	case FNSP_MYORGUNIT:
		answer = myorgunit_equivs(lead_type, leading_name,
		    lead_entry, orig_entry, name, ip);
		break;
	case FNSP_MYENS:
		answer = myens_equivs(lead_type, leading_name,
		    lead_entry, orig_entry, name, ip);
		break;
	case FNSP_GLOBAL:
		answer = global_equivs(lead_type, leading_name,
		    lead_entry, orig_entry, name, ip);
	case FNSP_DNS:
	case FNSP_X500:
		// maybe able to do something sensible with these global
		// bindings if we know what the ens global name is
		// (i.e. some thing for thisens and myens)
	case FNSP_MYSITE:
	case FNSP_THISSITE:
	default:
		// equivalence not support for these types
		break;
	}

cleanup:
	if (answer == NULL) {
		status.set(FN_E_NO_EQUIVALENT_NAME, 0, 0, &name);
	}
	return (answer);
}

FN_composite_name *
FNSP_InitialContext::thisorgunit_equivs(FNSP_IC_name_type lead_type,
					const FN_string &leading_name,
					Entry *lead_entry,
					Entry *orig_entry,
					const FN_composite_name &orig_name,
					void *ip)
{
	FN_composite_name *answer = NULL;
	Entry *thisensentry, *myensentry, *myorgentry;

	switch (lead_type) {
	case FNSP_GLOBAL:
	case FNSP_THISENS:
	case FNSP_MYENS:
	case FNSP_ORGUNIT:
		// <orgname>/suffix
		if (orig_entry->equiv_name() == NULL)
			return (NULL);
		answer = concat_cname(*(orig_entry->equiv_name()),
				    orig_name.suffix(ip));
		if (lead_type == FNSP_ORGUNIT) {
			// thisorgunit -> orgunit/<orgname>
			answer->prepend_comp(leading_name);
			break;
		}
		answer->prepend_comp((unsigned char *)"_orgunit");
		if (lead_type == FNSP_THISENS) {
			// thisorgunit -> thisens/orgunit/<orgname>
			answer->prepend_comp(leading_name);
			break;
		}

		thisensentry = find_entry((unsigned char *)"_thisens");
		if (thisensentry == NULL) {
			delete answer;
			answer = NULL;
			break;
		}

		if (lead_type == FNSP_MYENS) {
			// thisorgunit -> myens/orgunit/<orgname>
			// 	if <myensname> == <thisensname>
			if (lead_entry->is_equiv_name(
			    thisensentry->equiv_name()))
				answer->prepend_comp(leading_name);
			else {
				delete answer;
				answer = NULL;
			}
			break;
		}

		// global case:
		// 	thisorgunit -> .../<ensname>/orgunit/<orgname>
		if (thisensentry->equiv_name() != NULL) {
			answer->prepend_comp(*(thisensentry->equiv_name()));
			answer->prepend_comp(leading_name);
		} else {
			delete answer;
			answer = NULL;
		}
		break;
	case FNSP_HOST:
		// thisorgunit/host -> host
		if (is_host_token(orig_name.next(ip)))
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;
	case FNSP_USER:
		// thisorgunit/user -> user
		if (is_user_token(orig_name.next(ip)))
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;
	case FNSP_THISHOST:
		// thisorgunit/host/<hostname> -> thishost
		if (is_host_token(orig_name.next(ip)) &&
		    lead_entry->is_equiv_name(orig_name.next(ip)))
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;
	case FNSP_MYSELF:
		// if myensname == thisensname && thisorgname == myorgname

		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    myensentry->is_equiv_name(thisensentry->equiv_name()) &&
		    (myorgentry = find_entry((unsigned char *)"_myorgunit")) &&
		    myorgentry->is_equiv_name(orig_entry->equiv_name()) &&
		    is_user_token(orig_name.next(ip)) &&
		    lead_entry->is_equiv_name(orig_name.next(ip)))
			// thisorgunit/user/<username> -> myself
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;
	case FNSP_MYORGUNIT:
		// if myensname == thisensname && thisorgname == myorgname
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    myensentry->is_equiv_name(thisensentry->equiv_name()) &&
		    lead_entry->is_equiv_name(orig_entry->equiv_name()))
			// thisorgunit -> myorgunit
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;
	}
	return (answer);
}

FN_composite_name *
FNSP_InitialContext::thishost_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry *orig_entry,
				    const FN_composite_name &orig_name,
				    void *ip)
{
	FN_composite_name *answer = NULL;
	Entry *thisensentry, *myensentry, *thisorgentry;

	if (orig_entry->equiv_name() == NULL)
		return (NULL);

	switch (lead_type) {
	case FNSP_MYORGUNIT:
		if ((thisorgentry =
		    find_entry((unsigned char *)"_thisorgunit")) &&
		    lead_entry->is_equiv_name(thisorgentry->equiv_name()) &&
		    (thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    myensentry->is_equiv_name(thisensentry->equiv_name())) {
			// thisorgunit == myorgunit && thisens == myens
			goto compose;
		} else {
			break;
		}

		// fall through to test for ens names

	case FNSP_MYENS:
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    lead_entry->is_equiv_name(thisensentry->equiv_name())) {
			// thisens == myens
		} else {
			break;
		}
		// fall through to do name composition

	case FNSP_GLOBAL:
	case FNSP_THISENS:
	case FNSP_ORGUNIT:
	case FNSP_THISORGUNIT:
	case FNSP_HOST:
	compose:
		// thishost -> host/<hostname>
		answer = concat_cname(*(orig_entry->equiv_name()),
				    orig_name.suffix(ip));
		if (lead_type == FNSP_HOST) {
			// thishost -> host/<hostname>
			answer->prepend_comp(leading_name);
			break;
		}
		answer->prepend_comp((unsigned char *)"_host");
		if (lead_type == FNSP_THISORGUNIT ||
		    lead_type == FNSP_MYORGUNIT) {
			// thishost -> thisorgunit/host/<hostname>
			// thishost -> myorgunit/host/<hostname>
			answer->prepend_comp(leading_name);
			break;
		}
		// find which orgunit this host belongs to
		thisorgentry = find_entry((unsigned char *)"_thisorgunit");
		if (thisorgentry == 0 || thisorgentry->equiv_name() == NULL) {
			delete answer;
			answer = NULL;
			break;
		}
		answer->prepend_comp(*(thisorgentry->equiv_name()));
		if (lead_type == FNSP_ORGUNIT) {
			// thishost -> orgunit/<orgname>/host/<hostname>
			answer->prepend_comp(leading_name);
			break;
		}
		answer->prepend_comp((unsigned char *)"_orgunit");
		if (lead_type == FNSP_THISENS || lead_type == FNSP_MYENS) {
			// thishost -> thisens/orgunit/<orgname>/host/<hostname>
			// thishost -> myens/orgunit/<orgname>/host/<hostname>
			answer->prepend_comp(leading_name);
			break;
		}
		// global case; find which ens this host belongs to
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    thisensentry->equiv_name() != NULL) {
			answer->prepend_comp(*(thisensentry->equiv_name()));
			answer->prepend_comp(leading_name);
		} else {
			delete answer;
			answer = NULL;
		}
		break;
	}

	return (answer);
}

FN_composite_name *
FNSP_InitialContext::thisens_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry *orig_entry,
				    const FN_composite_name &orig_name,
				    void *ip)
{
	FN_composite_name *answer = NULL;
	Entry *orgentry, *myensentry, *orgrootentry, *orgroot;
	const FN_string* next_token;

	switch (lead_type) {
	case FNSP_GLOBAL:
		// thisens -> .../<ensname>
		if (orig_entry->equiv_name() != NULL) {
			answer = concat_cname(*(orig_entry->equiv_name()),
					    orig_name.suffix(ip));
			answer->prepend_comp(leading_name);
		}
		break;
	case FNSP_MYENS:
		// thisens -> myens
		if (lead_entry->is_equiv_name(orig_entry->equiv_name())) {
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		}
		break;
	case FNSP_ORGUNIT:
		// thisens/orgunit -> orgunit
		if (is_org_token(orig_name.next(ip)))
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;
	case FNSP_THISHOST:
	case FNSP_HOST:
	case FNSP_USER:
		// make sure orig_name is of form thisens/orgunit/<orgname>/host
		if (is_org_token(next_token = orig_name.next(ip)) &&
		    (orgentry = find_entry((unsigned char *)"_thisorgunit")) &&
		    orgentry->is_equiv_name(orig_name.next(ip)) &&
		    ((lead_type == FNSP_HOST &&
		    is_host_token(orig_name.next(ip))) ||
		    (lead_type == FNSP_USER &&
		    is_user_token(orig_name.next(ip))))) {
			// so far so good
		} else if (
		    (lead_type == FNSP_HOST && is_host_token(next_token) ||
		    (lead_type == FNSP_USER && is_user_token(next_token))) &&
		    (orgentry = find_entry((unsigned char *)"_thisorgunit")) &&
		    (orgrootentry = find_entry((unsigned char *)"_orgunit")) &&
		    orgentry->is_equiv_name(orgrootentry->equiv_name())) {
			// OK to omit 'orgunit/orgname'
			// if thisorgname == orgrootname
		} else {
			break;
		}

		if (lead_type == FNSP_HOST || lead_type == FNSP_USER) {
			// thisens/orgunit/<orgname>/host -> host
			// thisens/orgunit/<orgname>/user -> user
			// thisens/host -> host
			// thisens/user -> user
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
			break;
		}

		// thisens/orgunit/<orgname>/host/<hostname> -> thishost
		if (lead_entry->is_equiv_name(orig_name.next(ip))) {
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
			break;
		}
		break; // wrong host name

	case FNSP_MYSELF:
		// thisens/orgunit/<orgname>/user/<username> -> myself

		// if thisensname == myensname && myorgunit == thisorgunit

		// ens names must match
		if ((myensentry = find_entry((unsigned char *)"_myens")) &&
		    myensentry->is_equiv_name(orig_entry->equiv_name())) {
			// ens names match
		} else {
			break;
		}

		if (is_org_token(next_token = orig_name.next(ip)) &&
		    (orgentry = find_entry((unsigned char *)"_myorgunit")) &&
		    orgentry->is_equiv_name(orig_name.next(ip)) &&
		    is_user_token(orig_name.next(ip))) {
			// myorgunit == <orgname>
		} else if (is_user_token(next_token) &&
		    (orgentry = find_entry((unsigned char *)"_myorgunit")) &&
		    (orgrootentry = find_entry((unsigned char *)"_orgunit")) &&
		    orgentry->is_equiv_name(orgrootentry->equiv_name())) {
			// OK to omit 'orgunit/<orgname>'
			// if myorgname == <orgrootname>
		} else {
			break;
		}

		// if <username> matches
		if (lead_entry->is_equiv_name(orig_name.next(ip))) {
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		}
		break; // wrong user name

	case FNSP_MYORGUNIT:
		// thisens/orgunit/<orgname> -> myorgunit
		// allowed if thisensname == myensname
		if ((myensentry = find_entry((unsigned char *)"_myens")) &&
		    myensentry->is_equiv_name(orig_entry->equiv_name()))
			;
		else
			break;
		// fall through to do check on myorgunit name

	case FNSP_THISORGUNIT:
		// thisens/orgunit/<orgname> -> thisorgunit

		// make sure orig_name is of form thisens/orgunit/<orgname>
		if (is_org_token(orig_name.next(ip))) {
			if (lead_entry->is_equiv_name(orig_name.next(ip)))
				answer = concat_cname(leading_name,
						    orig_name.suffix(ip));
		} else if ((orgroot =
		    find_entry((unsigned char *)"_orgunit")) &&
		    lead_entry->is_equiv_name(orgroot->equiv_name())) {
			orig_name.first(ip); // reset
			// thisens -> thisorgunit or myorgunit
			// if <orgname> is root name
			answer = concat_cname(leading_name,
					    orig_name.suffix(ip));
		}
		break; // wrong orgname
	}

	return (answer);
}

// Return the equivalent of 'myself' using lead info supplied
FN_composite_name *
FNSP_InitialContext::myself_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry *orig_entry,
				    const FN_composite_name &orig_name,
				    void *ip)
{
	FN_composite_name *answer = NULL;
	Entry *tmpentry = NULL,
	    *myorgentry, *thisorgentry, *myensentry, *thisensentry;

	if (orig_entry->equiv_name() == NULL)
		return (NULL);

	switch (lead_type) {
	case FNSP_THISENS:
		tmpentry = lead_entry;
		// fall through for testing
	case FNSP_ORGUNIT:
		if (tmpentry == NULL)
			tmpentry = find_entry((unsigned char *)"_thisens");
		if (tmpentry &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    (myensentry->is_equiv_name(tmpentry->equiv_name()))) {
			// thisens == myens
		} else {
			break;
		}
		// fall through for composing name

	case FNSP_GLOBAL:
	case FNSP_MYENS:
		// generate user/<username>
		answer = concat_cname(*(orig_entry->equiv_name()),
				    orig_name.suffix(ip));
		answer->prepend_comp((unsigned char *)"_user");
		tmpentry = find_entry((unsigned char *)"_myorgunit");
		if (tmpentry == NULL || tmpentry->equiv_name() == NULL) {
			delete answer;
			return (NULL);
		}

		// <orgname>/user/<username>
		answer->prepend_comp(*(tmpentry->equiv_name()));
		if (lead_type == FNSP_ORGUNIT) {
			// myself -> orgunit/<orgname>/user/<username>
			answer->prepend_comp(leading_name);
			break;
		}
		answer->prepend_comp((unsigned char *)"_orgunit");
		if (lead_type == FNSP_MYENS || lead_type == FNSP_THISENS) {
			// myself -> myens/orgunit/<orgname>/user/<username>
			answer->prepend_comp(leading_name);
			break;
		}

		// global case
		if ((tmpentry = find_entry((unsigned char *)"_myens")) &&
		    (tmpentry->equiv_name() != NULL)) {
			answer->prepend_comp(*(tmpentry->equiv_name()));
			answer->prepend_comp(leading_name);
		} else {
			delete answer;
			answer = NULL;
		}
		break;

	case FNSP_THISORGUNIT:
		// myself -> thisorgunit/user/<username>
		if ((tmpentry = find_entry((unsigned char *)"_myorgunit")) &&
		    lead_entry->is_equiv_name(tmpentry->equiv_name()) &&
		    (thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    (myensentry->is_equiv_name(thisensentry->equiv_name()))) {
			// if thisens == myens && thisorgunit == myorgunit
		} else {
			break;
		}
		// fall through to construct name

	case FNSP_MYORGUNIT:
		// myself -> myorgunit/user/<username>
		answer = concat_cname(*(orig_entry->equiv_name()),
				    orig_name.suffix(ip));
		answer->prepend_comp((unsigned char *)"_user");
		answer->prepend_comp(leading_name);
		break;

	case FNSP_USER:
		// myself -> user/<username>
		// if thisens == myens && thisorgname == myorgname
		if ((myorgentry = find_entry((unsigned char *)"_myorgunit")) &&
		    (thisorgentry =
			find_entry((unsigned char *)"_thisorgunit")) &&
		    thisorgentry->is_equiv_name(myorgentry->equiv_name()) &&
		    (thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    (myensentry->is_equiv_name(thisensentry->equiv_name()))) {
			answer = concat_cname(*(orig_entry->equiv_name()),
					    orig_name.suffix(ip));
			answer->prepend_comp(leading_name);
		}
		break;
	}
	return (answer);
}

FN_composite_name *
FNSP_InitialContext::orgunit_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry * /* orig_entry */,
				    const FN_composite_name &orig_name,
				    void *ip)
{
	FN_composite_name *answer = NULL;
	Entry *tmpentry, *thisensentry, *myensentry;

	switch (lead_type) {
	case FNSP_GLOBAL:
		// orgunit -> .../<ensname>
		if ((tmpentry = find_entry((unsigned char *)"_thisens")) &&
		    tmpentry->equiv_name() != NULL) {
			answer = concat_cname(*(tmpentry->equiv_name()),
					    new FN_composite_name(orig_name));
			answer->prepend_comp(leading_name);
		}
		break;
	case FNSP_MYENS:
		// orgunit -> myens/orgunit
		if ((tmpentry = find_entry((unsigned char *)"_thisens")) &&
		    lead_entry->is_equiv_name(tmpentry->equiv_name())) {
			// thisens == myens
		} else {
			break;
		}
		// fall through to compose name
	case FNSP_THISENS:
		// orgunit -> thisens/orgunit
		answer = concat_cname(leading_name,
				    new FN_composite_name(orig_name));
		break;
	case FNSP_MYORGUNIT:
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    (myensentry->is_equiv_name(thisensentry->equiv_name()))) {
			// thisens == myens
		} else {
			break;
		}
		// fall through to check orgname and compose if matches
	case FNSP_THISORGUNIT:
		// orgunit/<orgname> -> thisorgunit
		// orgunit/<orgname> -> myorgunit
		if (lead_entry->is_equiv_name(orig_name.next(ip)))
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;
	case FNSP_THISHOST:
	case FNSP_HOST:
		// make sure orignal name has prefix: orgunit/<orgname>/host
		if ((tmpentry = find_entry((unsigned char *)"_thisorgunit")) &&
		    tmpentry->is_equiv_name(orig_name.next(ip)) &&
		    is_host_token(orig_name.next(ip))) {
			// so far so good
		} else {
			break; // wrong orgname
		}
		if (lead_type == FNSP_HOST) {
			// orgunit/<orgname>/host -> host
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
			break;
		}
		// orgunit/<orgname>/host/<hostname> -> thishost
		if (lead_entry->is_equiv_name(orig_name.next(ip)))
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;
	case FNSP_MYSELF:
		// orgunit/<orgname>/user/<username> -> myself
		// if thisens == myens && myorgunit == <orgname>
		// && myuser == uname
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    (myensentry->is_equiv_name(thisensentry->equiv_name())) &&
		    (tmpentry = find_entry((unsigned char *)"_myorgunit")) &&
		    tmpentry->is_equiv_name(orig_name.next(ip)) &&
		    is_user_token(orig_name.next(ip)) &&
		    lead_entry->is_equiv_name(orig_name.next(ip)))
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;
	case FNSP_USER:
		// orgunit/<orgname>/user -> user
		if ((tmpentry = find_entry((unsigned char *)"_thisorgunit")) &&
		    tmpentry->is_equiv_name(orig_name.next(ip)) &&
		    is_user_token(orig_name.next(ip))) {
			// orgunit/<orgname>/user -> user
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
			break;
		}
		break;
	}
	return (answer);
}

FN_composite_name *
FNSP_InitialContext::site_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry * lead_entry,
				    Entry * /* orig_entry */,
				    const FN_composite_name &orig_name,
				    void * /* ip */)
{
	FN_composite_name *answer = NULL;
	Entry *tmpentry, *thisensentry, *myensentry;

	switch (lead_type) {
	case FNSP_MYENS:
		if ((tmpentry = find_entry((unsigned char *)"_thisens")) &&
		    lead_entry->is_equiv_name(tmpentry->equiv_name())) {
			// thisens == myens
		} else {
			break;
		}
		// fall through to compose name
	case FNSP_THISENS:
		// site -> thisens/site
		// site -> myens/site
		answer = concat_cname(leading_name,
				    new FN_composite_name(orig_name));
		break;
	case FNSP_GLOBAL:
		// site -> .../<ensname>/site
		if ((tmpentry = find_entry((unsigned char *)"_thisens")) &&
		    tmpentry->equiv_name() != NULL) {
			answer = concat_cname(*(tmpentry->equiv_name()),
					    new FN_composite_name(orig_name));
			answer->prepend_comp(leading_name);
		}
		break;
	case FNSP_ORGUNIT:
		// site -> orgunit/<orgroot>/site
		if (lead_entry->equiv_name())
			answer = concat_cname(*(lead_entry->equiv_name()),
					    new FN_composite_name(orig_name));
		else
			answer = concat_cname((unsigned char *)"",
					    new FN_composite_name(orig_name));
		answer->prepend_comp(leading_name);
		break;
	case FNSP_MYORGUNIT:
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    (myensentry->is_equiv_name(thisensentry->equiv_name()))) {
			// thisens == myens
		} else {
			break;
		}
		// fall through to check myorgname

	case FNSP_THISORGUNIT:
		// site -> thisorgunit/site iff thisorg == orgroot
		if ((tmpentry = find_entry((unsigned char *)"_orgunit")) &&
		    (lead_entry->is_equiv_name(tmpentry->equiv_name()))) {
			answer = concat_cname(leading_name,
					    new FN_composite_name(orig_name));
		}
		break;
	}
	return (answer);
}

FN_composite_name *
FNSP_InitialContext::user_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry * /* orig_entry */,
				    const FN_composite_name &orig_name,
				    void *ip)
{
	FN_composite_name *answer = NULL;
	Entry *tmpentry, *thisensentry, *myensentry, *thisorgentry, *myorgentry;

	switch (lead_type) {
	case FNSP_MYSELF:
		// user/<username> -> mysel
		// if <thisens> == <myens> && <thisorgname> == <myorgname>
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    myensentry->is_equiv_name(thisensentry->equiv_name()) &&
		    (thisorgentry =
			find_entry((unsigned char *)"_thisorgunit")) &&
		    (myorgentry = find_entry((unsigned char *)"_myorgunit")) &&
		    myorgentry->is_equiv_name(thisorgentry->equiv_name()) &&
		    lead_entry->is_equiv_name(orig_name.next(ip)))
			// thisens == myens && thisorgunit == myorgunit
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;
	case FNSP_MYORGUNIT:
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    myensentry->is_equiv_name(thisensentry->equiv_name()) &&
		    (thisorgentry =
			find_entry((unsigned char *)"_thisorgunit")) &&
		    lead_entry->is_equiv_name(thisorgentry->equiv_name())) {
			// thisens == myens && thisorgunit == myorgunit
		} else {
			break;
		}
		// fall through to compose name

	case FNSP_THISORGUNIT:
		// user -> thisorgunit/user
		// user -> myorgunit/user
		answer = concat_cname(leading_name,
		    new FN_composite_name(orig_name));
		break;
	case FNSP_MYENS:
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    lead_entry->is_equiv_name(thisensentry->equiv_name())) {
			// thisens == myens
		} else {
			break;
		}
		// fall through
	case FNSP_GLOBAL:
	case FNSP_THISENS:
	case FNSP_ORGUNIT:
		// user -> orgunit/<orgname>/user
		tmpentry = find_entry((unsigned char *)"_thisorgunit");
		if (tmpentry == NULL)
			break;
		answer = concat_cname(*(tmpentry->equiv_name()),
				    new FN_composite_name(orig_name));

		if (lead_type == FNSP_ORGUNIT) {
			answer->prepend_comp(leading_name);
			break;
		}
		// user -> thisens/orgunit/<orgname>/user
		// user -> myens/orgunit/<orgname>/user
		answer->prepend_comp((unsigned char *)"_orgunit");
		if (lead_type == FNSP_MYENS || lead_type == FNSP_THISENS) {
			answer->prepend_comp(leading_name);
			break;
		}

		// global case user -> .../<ensname>
		if ((tmpentry = find_entry((unsigned char *)"_thisens")) &&
		    tmpentry->equiv_name() != NULL) {
			answer->prepend_comp(*(tmpentry->equiv_name()));
			answer->prepend_comp(leading_name);
		} else {
			delete answer;
			answer = NULL;
		}
	}
	return (answer);
}

FN_composite_name *
FNSP_InitialContext::host_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry * /* orig_entry */,
				    const FN_composite_name &orig_name,
				    void *ip)
{
	FN_composite_name *answer = NULL;
	Entry *tmpentry, *thisensentry, *myensentry, *thisorgentry;

	switch (lead_type) {
	case FNSP_THISHOST:
		// host/<hostname> -> host
		if (lead_entry->is_equiv_name(orig_name.next(ip)))
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;

	case FNSP_MYORGUNIT:
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    myensentry->is_equiv_name(thisensentry->equiv_name()) &&
		    (thisorgentry =
			find_entry((unsigned char *)"_thisorgunit")) &&
		    lead_entry->is_equiv_name(thisorgentry->equiv_name())) {
			// thisens == myens && thisorgunit == myorgunit
		} else {
			break;
		}
		// fall through to do composition
		// host -> myorgunit/host

	case FNSP_THISORGUNIT:
		// host -> thisorgunit/host
		answer = concat_cname(leading_name,
				    new FN_composite_name(orig_name));
		break;
	case FNSP_MYENS:
		// host -> myens/org/<thisorgname>/host
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    myensentry->is_equiv_name(thisensentry->equiv_name())) {
			// thisens == myens
		} else {
			break;
		}
		// fall through to continue

	case FNSP_GLOBAL:
	case FNSP_THISENS:
	case FNSP_ORGUNIT:
		// host -> <orgname>/host
		tmpentry = find_entry((unsigned char *)"_thisorgunit");
		if (tmpentry == NULL || tmpentry->equiv_name() == NULL)
			break;
		answer = concat_cname(*(tmpentry->equiv_name()),
				    new FN_composite_name(orig_name));

		// host -> orgunit/<orgname>/host
		if (lead_type == FNSP_ORGUNIT) {
			answer->prepend_comp(leading_name);
			break;
		}
		// host -> thisens/orgunit/<orgname>/host
		// host -> myens/orgunit/<orgname>/host
		answer->prepend_comp((unsigned char *)"_orgunit");
		if (lead_type == FNSP_THISENS || lead_type == FNSP_MYENS) {
			answer->prepend_comp(leading_name);
			break;
		}
		// global: host -> .../<ensname>/orgunit/<orgname>/host
		if ((tmpentry = find_entry((unsigned char *)"_thisens")) &&
		    tmpentry->equiv_name() != NULL) {
			answer->prepend_comp(*(tmpentry->equiv_name()));
			answer->prepend_comp(leading_name);
		} else {
			delete answer;
			answer = NULL;
		}
		break;
	}
	return (answer);
}

FN_composite_name *
FNSP_InitialContext::myorgunit_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry *orig_entry,
				    const FN_composite_name &orig_name,
				    void *ip)
{
	FN_composite_name *answer = NULL;
	Entry *myensentry, *thisensentry, *thisorgentry;
	const FN_string *next_token;

	switch (lead_type) {
	case FNSP_MYSELF:
		// myorgunit/user/<username> -> myself
		if (is_user_token(orig_name.next(ip)) &&
		    lead_entry->is_equiv_name(orig_name.next(ip)))
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;
	case FNSP_THISENS:
		// if thisens == myens
		if ((myensentry = find_entry((unsigned char *)"_myens")) &&
		    lead_entry->is_equiv_name(myensentry->equiv_name())) {
			break;
		}
		// thisens == myens follow through

	case FNSP_GLOBAL:
	case FNSP_MYENS:
		// myorgunit -> myens/orgunit/<orgname>
		// myorgunit -> thisens/orgunit/<orgname>
		if (orig_entry->equiv_name() == NULL)
			break;
		answer = concat_cname(*(orig_entry->equiv_name()),
				    orig_name.suffix(ip));
		answer->prepend_comp((unsigned char *)"_orgunit");
		if (lead_type == FNSP_MYENS || lead_type == FNSP_THISENS) {
			answer->prepend_comp(leading_name);
			break;
		}

		// global case:
		// 	myorgunit -> .../<ensname>/orgunit/<orgname>
		myensentry = find_entry((unsigned char *)"_myens");
		if (myensentry && myensentry->equiv_name()) {
			answer->prepend_comp(*(myensentry->equiv_name()));
			answer->prepend_comp(leading_name);
		} else {
			delete answer;
			answer = NULL;
		}
		break;
	case FNSP_THISORGUNIT:
		// myorgunit -> thisorgunit
		// if <thisens> == <myens> && thisorgunit == myorgunit
		if (lead_entry->is_equiv_name(orig_entry->equiv_name()) &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    (thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry->is_equiv_name(thisensentry->equiv_name())))
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;
	case FNSP_ORGUNIT:
		// myorgunit -> orgunit/<myorgname>
		// if myens == thisens
		if (orig_entry->equiv_name() &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    (thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry->is_equiv_name(thisensentry->equiv_name()))) {
			answer = concat_cname(*(orig_entry->equiv_name()),
					    orig_name.suffix(ip));
			answer->prepend_comp(leading_name);
		}
		break;
	case FNSP_USER:
	case FNSP_HOST:
	case FNSP_THISHOST:
		// myorgunit/host -> host
		// myorgunit/user -> user
		// myorgunit/host/<hostname> -> thishost
		if ((thisorgentry =
		    find_entry((unsigned char *)"_thisorgunit")) &&
		    thisorgentry->is_equiv_name(orig_entry->equiv_name()) &&
		    (myensentry = find_entry((unsigned char *)"_myens")) &&
		    (thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (myensentry->is_equiv_name(thisensentry->equiv_name()))) {
			next_token = orig_name.next(ip);
			if ((lead_type == FNSP_USER &&
				is_user_token(next_token)) ||
			    (lead_type == FNSP_HOST &&
				is_host_token(next_token)) ||
			    (lead_type == FNSP_THISHOST &&
			    is_host_token(next_token) &&
			    lead_entry->is_equiv_name(orig_name.next(ip)))) {
				answer = concat_cname(leading_name,
						    orig_name.suffix(ip));
			}
		}
		break;
	}

	return (answer);
}

FN_composite_name *
FNSP_InitialContext::myens_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry *orig_entry,
				    const FN_composite_name &orig_name,
				    void *ip)
{
	FN_composite_name *answer = NULL;
	Entry *myorgentry, *orgroot, *thisensentry, *thisorgentry;
	const FN_string *next_token;

	switch (lead_type) {
	case FNSP_MYSELF:
		// myens/orgunit/<orgname>/user/<username> -> myself
		// if myorgunit == <orgname> && myusername == <username>
		if (is_org_token(next_token = orig_name.next(ip)) &&
		    (myorgentry = find_entry((unsigned char *)"_myorgunit")) &&
		    myorgentry->is_equiv_name(orig_name.next(ip)) &&
		    is_user_token(orig_name.next(ip)) &&
		    lead_entry->is_equiv_name(orig_name.next(ip)))
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		else if (is_user_token(next_token) &&
		    lead_entry->is_equiv_name(orig_name.next(ip)) &&
		    (myorgentry = find_entry((unsigned char *)"_myorgunit")) &&
		    (orgroot = find_entry((unsigned char *)"_orgunit")) &&
		    myorgentry->is_equiv_name(orgroot->equiv_name()) &&
		    (thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    orig_entry->is_equiv_name(thisensentry->equiv_name())) {
			// myens/user/<username> -> myself
			// if myens == thisens && myorgunit == orgroot &&
			// myusername  == <username>
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		}

		break;

	case FNSP_THISORGUNIT:
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    orig_entry->is_equiv_name(thisensentry->equiv_name())) {
			// thisens == myens
		} else {
			break;
		}
		// fall through to check orgname

	case FNSP_MYORGUNIT:
		// myens/orgunit/<orgname> -> myorgunit
		// myens/orgunit/<orgname> -> thisorgunit
		if (is_org_token(orig_name.next(ip))) {
			if (lead_entry->is_equiv_name(orig_name.next(ip)))
				answer = concat_cname(leading_name,
						    orig_name.suffix(ip));
		} else if ((orgroot =
			find_entry((unsigned char *)"_orgunit")) &&
		    lead_entry->is_equiv_name(orgroot->equiv_name())) {
			// myens/ -> myorgunit or thisorgunit
			// if orgname is orgroot
			orig_name.first(ip); // reset
			// thisens -> thisorgunit or myorgunit
			// if <orgname> is root name
			answer = concat_cname(leading_name,
					    orig_name.suffix(ip));
		}
		break;

	case FNSP_GLOBAL:
		// myens -> .../<ensname>
		if (orig_entry->equiv_name()) {
			answer = concat_cname(*(orig_entry->equiv_name()),
					    orig_name.suffix(ip));
			answer->prepend_comp(leading_name);
		}
		break;

	case FNSP_THISENS:
		if (orig_entry->is_equiv_name(lead_entry->equiv_name()))
			answer =
			    concat_cname(leading_name, orig_name.suffix(ip));
		break;

	case FNSP_HOST:
	case FNSP_USER:
	case FNSP_THISHOST:
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    orig_entry->is_equiv_name(thisensentry->equiv_name()) &&
		    (thisorgentry =
			find_entry((unsigned char *)"_thisorgunit"))) {
			// thisens == myens
			if (is_org_token(next_token = orig_name.next(ip)) &&
			    thisorgentry->is_equiv_name(orig_name.next(ip))) {
				next_token = orig_name.next(ip); // advance
				// myens/orgunit/<orgnm>/user -> user
				// myens/orgunit/<orgnm>/host -> host
				// ..../host/<hname> -> thishost
			} else if (
			    (orgroot =
				find_entry((unsigned char *)"_orgunit")) &&
			    thisorgentry->is_equiv_name(
				orgroot->equiv_name())) {
				// thisorgunit == orgroot
				// myens/user -> user
				// myens/host -> host
				// myens/host/<hostname> -> thishost
			} else
				break;

			if ((lead_type == FNSP_HOST &&
				is_host_token(next_token)) ||
			    (lead_type == FNSP_USER &&
				is_user_token(next_token)) ||
			    (lead_type == FNSP_THISHOST &&
			    is_host_token(next_token) &&
			    lead_entry->is_equiv_name(orig_name.next(ip)))) {
				answer = concat_cname(leading_name,
						    orig_name.suffix(ip));
			}
		}
		break;

	case FNSP_ORGUNIT:
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    orig_entry->is_equiv_name(thisensentry->equiv_name()) &&
		    is_org_token(orig_name.next(ip))) {
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		}
		break;
	}
	return (answer);
}


FN_composite_name *
FNSP_InitialContext::global_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry * /* orig_entry */,
				    const FN_composite_name &orig_name,
				    void *ip)
{
	FN_composite_name *answer = NULL;
	Entry *orgroot, *thisensentry, *thisorgentry, *myensentry;
	const FN_string *next_token;

	switch (lead_type) {
	case FNSP_MYENS:
	case FNSP_THISENS:
		if (lead_entry->is_equiv_name(orig_name.next(ip)))
			answer = concat_cname(leading_name,
					    orig_name.suffix(ip));
		break;
	case FNSP_MYORGUNIT:
		// .../<ensname>/orgunit/<orgname> -> myorgunit
		// if <ensname> == myens && <orgname> == myorgunit
		if ((myensentry = find_entry((unsigned char *)"_myens")) &&
		    (myensentry->is_equiv_name(orig_name.next(ip))) &&
		    is_org_token(orig_name.next(ip)) &&
		    lead_entry->is_equiv_name(orig_name.next(ip)))
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;
	case FNSP_THISORGUNIT:
		// .../<ensname>/orgunit/<orgname> -> thisorgunit
		// if <ensname> == thisens && <orgname> == thisorgunit
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (thisensentry->is_equiv_name(orig_name.next(ip))) &&
		    is_org_token(orig_name.next(ip)) &&
		    lead_entry->is_equiv_name(orig_name.next(ip)))
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
		break;
	case FNSP_ORGUNIT:
		// .../<ensname>/orgunit -> orgunit
		// if <ensname> == thisens
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (thisensentry->is_equiv_name(orig_name.next(ip))) &&
		    is_org_token(orig_name.next(ip)))
			answer = concat_cname(leading_name,
			    orig_name.suffix(ip));
	case FNSP_MYSELF:
		if ((myensentry = find_entry((unsigned char *)"_myens")) &&
		    (myensentry->is_equiv_name(orig_name.next(ip)))) {
			next_token = orig_name.next(ip);
			thisorgentry =
			    find_entry((unsigned char *)"_myorgunit");
			goto check_orgname;
		} else {
			break;
		}
	case FNSP_USER:
	case FNSP_HOST:
	case FNSP_THISHOST:
		if ((thisensentry = find_entry((unsigned char *)"_thisens")) &&
		    (thisensentry->is_equiv_name(orig_name.next(ip)))) {
			next_token = orig_name.next(ip);
			thisorgentry =
			    find_entry((unsigned char *)"_thisorgunit");
		check_orgname:
			if (is_org_token(next_token) &&
			    thisorgentry->is_equiv_name(orig_name.next(ip))) {
				// .../<ensname>/orgunit/<orgname>/user
				// .../<ensname>/orgunit/<orgname>/host
				next_token = orig_name.next(ip); // advance
			} else if (
			    ((orgroot =
				find_entry((unsigned char *)"_orgunit")) &&
			    thisorgentry->is_equiv_name(
				orgroot->equiv_name()))) {
				// .../<ensname>/user
				// .../<ensname>/host
			} else {
				break;
			}
			if ((lead_type == FNSP_HOST &&
				is_host_token(next_token)) ||
			    (lead_type == FNSP_USER &&
				is_user_token(next_token)) ||
			    (lead_type == FNSP_THISHOST &&
			    is_host_token(next_token) &&
			    lead_entry->is_equiv_name(orig_name.next(ip))) ||
			    (lead_type == FNSP_MYSELF &&
			    is_user_token(next_token) &&
			    lead_entry->is_equiv_name(orig_name.next(ip))))
				answer = concat_cname(leading_name,
						    orig_name.suffix(ip));
		}
		break;
	}

	return (answer);
}

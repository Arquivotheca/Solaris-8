/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_ENSContext.cc	1.6	96/03/31 SMI"

#include "FNSP_ENSContext.hh"
#include <string.h>  /* for strncmp */
#include "fnsp_internal.hh"
#include <FNSP_Syntax.hh>
#include "FNSP_nisplus_address.hh"
#include "fnsp_search.hh"

#include <xfn/FN_searchlist_svc.hh>


// This file contains the implementation of the 'ENS' context.
// The ENS context is the root orgunit context ("org//") plus the
// "org" binding itself.  This would allow one to access all
// namespaces bound in the root orgunit context, plus access to the
// org namespace itself.

static FN_string can_org_nsid((const unsigned char *)"_orgunit");
static FN_string cust_org_nsid((const unsigned char *)"orgunit");
static FN_string empty_name((const unsigned char *)"");

static inline int
is_orgunit_nsid(const FN_string &name)
{
	return ((name.compare(can_org_nsid, FN_STRING_CASE_INSENSITIVE)
		    == 0) ||
		(name.compare(cust_org_nsid, FN_STRING_CASE_INSENSITIVE)
		    == 0));
}

FNSP_ENSContext::~FNSP_ENSContext()
{
	delete my_reference;
	delete org_ref;
	delete org_nns_ref;
	delete org_nns_addr;
}

#ifdef DEBUG
FNSP_ENSContext::FNSP_ENSContext(const FN_string &dirname, unsigned int auth)
: FN_ctx_svc(auth)
{
	my_reference = FNSP_reference(FNSP_nisplus_address_type_name(),
	    dirname, FNSP_enterprise_context);

	org_ref = FNSP_reference(FNSP_nisplus_address_type_name(),
	    dirname, FNSP_organization_context);

	FNSP_nisplus_address org_addr(dirname, FNSP_organization_context,
	    FNSP_normal_repr, auth);
	if (FNSP_context_exists(org_addr) != FN_SUCCESS) {
		my_reference = 0;
		org_ref = 0;
		org_nns_ref = 0;
		return;
	}
	// verify org context and nns of org exists
	// sets 'org_ref' to reference of org context
	// sets 'org_nns_ref'
	// sets 'org_nns_addr' to context handle
	//
	// Alternatively, we could use an org_nns_address instead.
	// Using a context handle, rather than an FNSP_Address
	// makes the impl more flexible -- i.e. org_nns could
	// conceivably be bound to a non-nis+ context (oh yeah :-)).

	if (org_ref) {
		FN_status status;
		FN_ctx* org_ctx = FN_ctx::from_ref(*org_ref, auth, status);
		if (org_ctx) {
			org_nns_ref =
				org_ctx->lookup((unsigned char *)"/",
					    status);
			delete org_ctx;
			// get context address
			if (org_nns_ref)
				org_nns_addr =
				    new FNSP_nisplus_address(*org_nns_ref,
				    auth);
		} else {
			delete org_ref;
			org_ref = 0;
		}
	}

	// check for null pointers
}
#endif /* DEBUG */

FNSP_ENSContext::FNSP_ENSContext(const FN_ref_addr &addr,
    const FN_ref &ref, unsigned int auth)
: FN_ctx_svc(auth)
{
	unsigned int ctx_type;
	unsigned int repr_type;
	FN_string* root_name = FNSP_address_to_internal_name(addr,
	    &ctx_type,
	    &repr_type);

	if (root_name == 0 || ctx_type != FNSP_enterprise_context) {
		delete root_name;
		my_reference = 0;
		return;
	}

	FNSP_nisplus_address org_addr(*root_name, FNSP_organization_context,
				    FNSP_normal_repr, auth);
	if (FNSP_context_exists(org_addr) != FN_SUCCESS) {
		delete root_name;
		my_reference = 0;
		org_ref = 0;
		org_nns_ref = 0;
		org_nns_addr = 0;
		return;
	}

	my_reference = new FN_ref(ref);
	org_ref = FNSP_reference(FNSP_nisplus_address_type_name(),
	    *root_name, FNSP_organization_context);
	delete root_name;


	// verify org context and nns of org exists
	// sets 'org_ref' to reference of org context
	// sets 'org_nns_ref'
	// sets 'org_nns_addr'

	if (org_ref) {
		FN_status status;
		FN_ctx* org_ctx = FN_ctx::from_ref(*org_ref, auth, status);
		if (org_ctx) {
			org_nns_ref =
				org_ctx->lookup((unsigned char *)"/",
					    status);
			delete org_ctx;
			// get context handle
			if (org_nns_ref)
				org_nns_addr = new
				    FNSP_nisplus_address(*org_nns_ref, auth);
			else
				org_nns_addr = 0;

		} else {
			/* Could not get context from org_ref */
			delete org_ref;
			org_ref = 0;
			org_nns_ref = 0;
			org_nns_addr = 0;
		}
	} else {
		org_nns_addr = 0;
		org_nns_ref = 0;
	}
}

FNSP_ENSContext*
FNSP_ENSContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth,
    FN_status &stat)
{
	FNSP_ENSContext *answer =
	    new FNSP_ENSContext(from_addr, from_ref, auth);

	if (answer && answer->my_reference)
		stat.set_success();
	else {
		if (answer) {
			if (answer->my_reference == 0)
				stat.set(FN_E_NOT_A_CONTEXT);
			delete answer;
			answer = 0;

		} else
			stat.set(FN_E_INSUFFICIENT_RESOURCES);
	}
	return (answer);
}

FN_ref *
FNSP_ENSContext::get_ref(FN_status &stat) const
{
	stat.set_success();

	return (new FN_ref(*my_reference));
}

FN_composite_name *
FNSP_ENSContext::equivalent_name(
    const FN_composite_name &name,
    const FN_string &,
    FN_status &status)
{
	status.set(FN_E_OPERATION_NOT_SUPPORTED, my_reference, &name);
	return (0);
}


FN_ref *
FNSP_ENSContext::c_lookup(const FN_string &name,
    unsigned int /* lookup_flags */,
    FN_status_csvc& cstat)
{
	if (name.is_empty()) {
		cstat.set_success();
		return (new FN_ref(*my_reference));
	} else
		return (resolve(name, cstat));
}

FN_ref *
FNSP_ENSContext::resolve(const FN_string &name, FN_status_csvc &cstat)
{
	if (is_orgunit_nsid(name)) {
		if (org_ref) {
			cstat.set_success();
			return (new FN_ref(*org_ref));
		} else {
			cstat.set_error(FN_E_NAME_NOT_FOUND,
			    *my_reference, name);
			return (0);
		}
	} else if (org_nns_ref) {
		// look up in org nns
		cstat.set_error(FN_E_SPI_CONTINUE, *org_nns_ref, name);
		return (0);
	} else {
		// org nns context does not exist
		cstat.set_error(FN_E_NAME_NOT_FOUND,
		    *my_reference, name);
		return (0);
	}
}

FN_namelist*
FNSP_ENSContext::c_list_names(const FN_string &name,
    FN_status_csvc &cstat)
{

	if (name.is_empty()) {
		// listing names bound in ENS context
		// This includes the org nsids (if org exists)
		// and names in org_nns (if any)
		FN_nameset *answer = 0;
		unsigned status;

		if (org_nns_addr) {
			answer = FNSP_list_names(*org_nns_addr, status);
		}
		if (answer == 0)
			answer = new FN_nameset;

		if (org_ref) {
			answer->add(can_org_nsid);
			answer->add(cust_org_nsid);
		}
		cstat.set_success();
		return (new FN_namelist_svc(answer));
	} else {
		FN_ref *ref = resolve(name, cstat);

		if (ref) {
			cstat.set_error(FN_E_SPI_CONTINUE, *ref, empty_name);
			delete ref;
		}
		return (0);
	}
}

FN_bindinglist*
FNSP_ENSContext::c_list_bindings(const FN_string &name,
    FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		// list ENS context
		FN_bindingset *answer = 0;
		unsigned status;

		if (org_nns_addr) {
			answer = FNSP_list_bindings(*org_nns_addr, status);
		}
		if (answer == 0)
			answer = new FN_bindingset;

		if (org_ref) {
			answer->add(can_org_nsid, *org_ref);
			answer->add(cust_org_nsid, *org_ref);
		}
		cstat.set_success();
		return (new FN_bindinglist_svc(answer));
	} else {
		FN_ref *ref = resolve(name, cstat);

		if (ref) {
			cstat.set_error(FN_E_SPI_CONTINUE, *ref,
					empty_name);
			delete ref;
		}
		return (0);
	}
}


int
FNSP_ENSContext::c_bind(const FN_string &name, const FN_ref &,
    unsigned, FN_status_csvc &cstat)
{
	if (name.is_empty() || is_orgunit_nsid(name) ||
	    org_nns_ref == 0) {
		// cannot touch binding of current or org context
		// or no nns context
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED,
				*my_reference, name);
		return (0);
	} else {
		// try in real context
		cstat.set_error(FN_E_SPI_CONTINUE, *org_nns_ref, name);
		return (0);
	}
}

int
FNSP_ENSContext::c_unbind(const FN_string &name,
    FN_status_csvc& cstat)
{
	if (name.is_empty() || is_orgunit_nsid(name)) {
		// cannot touch binding of current or org context
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED,
				*my_reference, name);
		return (0);
	} else if (org_nns_ref) {
		// try in real context
		cstat.set_error(FN_E_SPI_CONTINUE, *org_nns_ref, name);
		return (0);
	} else {
		// no real context, name must not be there
		cstat.set_success();
		return (1);
	}
}

int
FNSP_ENSContext::c_rename(const FN_string &name,
    const FN_composite_name &,
    unsigned, FN_status_csvc &cstat)
{
	if (name.is_empty() || is_orgunit_nsid(name) ||
	    org_nns_ref == 0) {
		// cannot touch binding of current or org context
		// or no nns context
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED,
				*my_reference, name);
		return (0);
	} else {
		// try in real context
		cstat.set_error(FN_E_SPI_CONTINUE, *org_nns_ref, name);
		return (0);
	}
}

FN_ref *
FNSP_ENSContext::c_create_subcontext(const FN_string &name,
    FN_status_csvc &cstat)
{
	if (name.is_empty() || is_orgunit_nsid(name) ||
	    org_nns_ref == 0) {
		// cannot touch binding of current or org context
		// or no nns context
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED,
				*my_reference, name);
		return (0);
	} else {
		// try in real context
		cstat.set_error(FN_E_SPI_CONTINUE, *org_nns_ref, name);
		return (0);
	}
}

int
FNSP_ENSContext::c_destroy_subcontext(const FN_string &name,
    FN_status_csvc &cstat)
{
	if (name.is_empty() || is_orgunit_nsid(name)) {
		// cannot touch binding of current or org context
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED,
				*my_reference, name);
		return (0);
	} else if (org_nns_ref) {
		// try in real context
		cstat.set_error(FN_E_SPI_CONTINUE, *org_nns_ref, name);
		return (0);
	} else {
		// no real context, name must not be there
		cstat.set_success();
		return (1);
	}
}

// attributes are ignored because c_bind() will atmost
// set continuation status
int
FNSP_ENSContext::c_attr_bind(const FN_string &name,
    const FN_ref &ref, const FN_attrset *, unsigned int exclusive,
    FN_status_csvc &status)
{
	// c_bind will check validity of arguments and set continue
	// status if possible; only 'name' arg is actually used here
	return (c_bind(name, ref, exclusive, status));
}

// attributes are ignored because c_create_subcontext() will atmost
// set continuation status
FN_ref *
FNSP_ENSContext::c_attr_create_subcontext(const FN_string &name,
    const FN_attrset *, FN_status_csvc &cstat)
{
	// c_create_subcontext will check validity of arguments and
	// set continue status if appropriate.
	return (c_create_subcontext(name, cstat));
}


FN_searchlist *
FNSP_ENSContext::c_attr_search(const FN_string &name,
    const FN_attrset *match_attrs,
    unsigned int return_ref,
    const FN_attrset *return_attr_ids,
    FN_status_csvc &cstat)
{

	if (name.is_empty()) {
		// searching ENS context
		// This includes the org nsids (if org exist)
		// and names in org_nns (if any)
		FN_searchset *matches = NULL;
		unsigned status = FN_SUCCESS;

		if (org_nns_addr) {
			matches = FNSP_search_attrset(*org_nns_addr,
			    match_attrs, return_ref, return_attr_ids, status);
		}

		if (status == FN_SUCCESS) {
			cstat.set_success();
			// If just listing context, add org bindings
			if (org_ref && match_attrs == NULL) {
				if (matches == NULL)
					matches = new FN_searchset;
				// these guys have no attributes
				matches->add(can_org_nsid,
					    (return_ref ? org_ref : NULL));
				matches->add(cust_org_nsid,
					    (return_ref ? org_ref: NULL));
			}
		} else {
			cstat.set_error(status, *my_reference,
			    (const unsigned char *)"");
		}
		if (matches)
			return (new FN_searchlist_svc(matches));
		else
			return (NULL);
	} else {
		FN_ref *ref = resolve(name, cstat);

		if (ref) {
			cstat.set_error(FN_E_SPI_CONTINUE, *ref, empty_name);
			delete ref;
		}
		return (0);
	}
}

FN_ext_searchlist *
FNSP_ENSContext::c_attr_ext_search(
    const FN_string &name,
    const FN_search_control * /* control */,
    const FN_search_filter * /* filter */,
    FN_status_csvc &cstat)
{
	// %%% needs work
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


FN_attrset*
FNSP_ENSContext::c_get_syntax_attrs(const FN_string &name,
    FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		// return syntax of current (flat) context
		FN_attrset *answer = 0;
		answer = FNSP_Syntax(FNSP_enterprise_context)
		    ->get_syntax_attrs();
		if (answer)
			cstat.set_success();
		else
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
		return (answer);
	} else {
		// resolve name and set continue status
		FN_ref *ref = resolve(name, cstat);

		if (ref) {
			cstat.set_error(FN_E_SPI_CONTINUE, *ref,
					empty_name);
			delete ref;
		}
		return (0);
	}
}

void
FNSP_ENSContext::handle_attr_operation(const FN_string &name,
    FN_status_csvc &cstat)
{
	if (name.is_empty() || is_orgunit_nsid(name))
		cstat.set_error(FN_E_ILLEGAL_NAME, *my_reference, name);
	else if (org_nns_ref)
		cstat.set_error(FN_E_SPI_CONTINUE, *org_nns_ref, name);
	else
		cstat.set_error(FN_E_NAME_NOT_FOUND, *org_nns_ref, name);
}


FN_attribute*
FNSP_ENSContext::c_attr_get(const FN_string &name,
    const FN_identifier &, unsigned int,
    FN_status_csvc &cstat)
{
	handle_attr_operation(name, cstat);
	return (0);
}

int
FNSP_ENSContext::c_attr_modify(const FN_string &name,
    unsigned int,
    const FN_attribute&, unsigned int,
    FN_status_csvc &cstat)
{
	handle_attr_operation(name, cstat);
	return (0);
}

FN_valuelist*
FNSP_ENSContext::c_attr_get_values(const FN_string &name,
    const FN_identifier &, unsigned int,
    FN_status_csvc &cstat)
{
	handle_attr_operation(name, cstat);
	return (0);
}

FN_attrset*
FNSP_ENSContext::c_attr_get_ids(const FN_string &name,
    unsigned int, FN_status_csvc &cstat)
{
	handle_attr_operation(name, cstat);
	return (0);
}

FN_multigetlist*
FNSP_ENSContext::c_attr_multi_get(const FN_string &name,
    const FN_attrset *, unsigned int,
    FN_status_csvc &cstat)
{
	handle_attr_operation(name, cstat);
	return (0);
}

int
FNSP_ENSContext::c_attr_multi_modify(const FN_string &name,
    const FN_attrmodlist&, unsigned int,
    FN_attrmodlist**,
    FN_status_csvc &cstat)
{
	handle_attr_operation(name, cstat);
	return (0);
}


FN_ref *
FNSP_ENSContext::c_lookup_nns(const FN_string &name,
    unsigned int /* lookup_flags */,
    FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		cstat.set_error(FN_E_NAME_NOT_FOUND, *my_reference, name);
		return (0);
	}
	return (resolve(name, cstat));
}

FN_namelist*
FNSP_ENSContext::c_list_names_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *ref = c_lookup_nns(name, 0, cstat);

	if (cstat.is_success()) {
		cstat.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}

	return (0);
}

FN_bindinglist*
FNSP_ENSContext::c_list_bindings_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *ref = c_lookup_nns(name, 0, cstat);

	if (cstat.is_success()) {
		cstat.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}

	return (0);
}

// This function should never be called.
int
FNSP_ENSContext::c_bind_nns(const FN_string &name,
    const FN_ref &ref,
    unsigned flags, FN_status_csvc &cstat)
{
	return (c_bind(name, ref, flags, cstat));
}

int
FNSP_ENSContext::c_unbind_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	return (c_unbind(name, cstat));
}

// This function should never be called.
int
FNSP_ENSContext::c_rename_nns(const FN_string &name,
    const FN_composite_name &newname,
    unsigned flags,
    FN_status_csvc &cstat)
{
	return (c_rename(name, newname, flags, cstat));
}


FN_ref *
FNSP_ENSContext::c_create_subcontext_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	return (c_create_subcontext(name, cstat));
}

int
FNSP_ENSContext::c_destroy_subcontext_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	return (c_destroy_subcontext(name, cstat));
}

FN_attrset*
FNSP_ENSContext::c_get_syntax_attrs_nns(const FN_string &name,
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
FNSP_ENSContext::c_attr_get_nns(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_csvc &cstat)
{
	return (c_attr_get(name, id, follow_link, cstat));
}

int
FNSP_ENSContext::c_attr_modify_nns(const FN_string &name,
    unsigned int op,
    const FN_attribute &attr, unsigned int follow_link,
    FN_status_csvc &cstat)
{
	return (c_attr_modify(name, op, attr, follow_link, cstat));
}

FN_valuelist*
FNSP_ENSContext::c_attr_get_values_nns(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_csvc &cstat)
{
	return (c_attr_get_values(name, id, follow_link, cstat));
}

FN_attrset*
FNSP_ENSContext::c_attr_get_ids_nns(const FN_string &name,
    unsigned int follow_link, FN_status_csvc &cstat)
{
	return (c_attr_get_ids(name, follow_link, cstat));
}

FN_multigetlist*
FNSP_ENSContext::c_attr_multi_get_nns(const FN_string &name,
    const FN_attrset *ids, unsigned int follow_link,
    FN_status_csvc &cstat)
{
	return (c_attr_multi_get(name, ids, follow_link, cstat));
}

int
FNSP_ENSContext::c_attr_multi_modify_nns(const FN_string &name,
    const FN_attrmodlist &modlist, unsigned int follow_link,
    FN_attrmodlist **retmodlist,
    FN_status_csvc &cstat)
{
	return (c_attr_multi_modify(name, modlist, follow_link, retmodlist,
	    cstat));
}


int
FNSP_ENSContext::c_attr_bind_nns(const FN_string &name,
    const FN_ref &ref,
    const FN_attrset *attrs,
    unsigned int exclusive,
    FN_status_csvc &cs)
{
	return (c_attr_bind(name, ref, attrs, exclusive, cs));
}

FN_ref *
FNSP_ENSContext::c_attr_create_subcontext_nns(const FN_string &name,
    const FN_attrset *attrs,
    FN_status_csvc &cstat)
{
	return (c_attr_create_subcontext(name, attrs, cstat));
}

FN_searchlist *
FNSP_ENSContext::c_attr_search_nns(
    const FN_string &name,
    const FN_attrset *match_attrs,
    unsigned int ret_ref,
    const FN_attrset *ret_attr_ids,
    FN_status_csvc &cs)
{
	return (c_attr_search(name, match_attrs, ret_ref, ret_attr_ids, cs));

}

FN_ext_searchlist *
FNSP_ENSContext::c_attr_ext_search_nns(
    const FN_string &name,
    const FN_search_control *control,
    const FN_search_filter *filter,
    FN_status_csvc &cs)
{
	return (c_attr_ext_search(name, control, filter, cs));
}

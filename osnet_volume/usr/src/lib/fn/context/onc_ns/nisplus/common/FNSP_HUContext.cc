/*
 * Copyright (c) 1992 - 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_HUContext.cc	1.17	97/08/15 SMI"


#include <sys/time.h>
#include <rpcsvc/nis.h>
#include <stdlib.h>
#include <string.h>

#include <FNSP_Syntax.hh>
#include "FNSP_nisplus_address.hh"
#include "FNSP_HUContext.hh"
#include "FNSP_HostnameContext.hh"
#include "FNSP_UsernameContext.hh"
#include "FNSP_nisplusImpl.hh"
#include "fnsp_internal.hh"
#include "fnsp_search.hh"
#include "fnsp_attrs.hh"
#include <fnsp_utils.hh>
#include <signal.h>

#include <xfn/FN_searchlist_svc.hh>

#define	AUTHORITATIVE 1

static const FN_composite_name empty_name((const unsigned char *)"");

// True if the identifiers of "needles" are a subset of those of "haystack".
static int is_identifier_subset(const FN_attrset &haystack,
    const FN_attrset &needles);

// True if any identifier of "reds" is also in "greens".
static int is_intersection(const FN_attrset &reds, const FN_attrset &greens);

// True if "needle" is the identifier of a member of "haystack".
static int inline
is_identifier_member(const FN_attrset &haystack, const FN_identifier &needle)
{
	return (haystack.get(needle) != NULL);
}

// Divide "orig_set" into "reds" and "rest" where "reds" are
// items identified in "redlist"
static void divide_attributes(const FN_attrset &orig_set,
    const FN_attrset &redlist, FN_attrset *&reds, FN_attrset *&rest);

// Join results of two searches
static FN_searchset *
join_search_results(FN_searchset *set1, FN_searchset *set2);

static FN_searchset *
multi_attr_search(const FNSP_Address &addr_hdl, const FN_attrset &attrs,
	unsigned int &status);

// %%% eventually, might want to merge this with FNSP_FlatContext
// %%% but for now, this is difficult because of attribute implementation
// %%% need to reexamine that

FNSP_HUContext::~FNSP_HUContext()
{
	if (my_orgname) delete my_orgname;
}

FNSP_HUContext::FNSP_HUContext(const FN_string &dirname,
    unsigned context_type, unsigned child_context_type,
    unsigned int auth) :
    FNSP_nisplusFlatContext(dirname, auth, context_type)
{
	unsigned status;
	my_orgname = FNSP_orgname_of(ns_impl->my_address->get_internal_name(),
	    status);
	my_child_context_type = child_context_type;
	// check for null pointers and status
}

FNSP_HUContext::FNSP_HUContext(const FN_ref &from_ref,
    unsigned child_context_type, unsigned int auth) :
    FNSP_nisplusFlatContext(from_ref, auth)
{
	unsigned status;
	my_orgname = FNSP_orgname_of(ns_impl->my_address->get_internal_name(),
	    status);
	my_child_context_type = child_context_type;
	// check for null pointers and status
}

FNSP_HUContext::FNSP_HUContext(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned child_context_type, unsigned int auth) :
    FNSP_nisplusFlatContext(from_addr, from_ref, auth)
{
	unsigned status;

	my_orgname = FNSP_orgname_of(ns_impl->my_address->get_internal_name(),
	    status);
	my_child_context_type = child_context_type;
	// check for null pointers
}


// Enumeration Operations

class FN_namelist_hu : public FN_namelist {
	nis_name table_name;
	char *next_in_line;
	unsigned int next_status;
	netobj iter_pos;
public:
	FN_namelist_hu(const char *table_name, char *name, netobj);
	~FN_namelist_hu();
	FN_string* next(FN_status &);
};

FN_namelist_hu::FN_namelist_hu(const char *tabname, char *first_name,
    netobj ip)
{
	table_name = strdup(tabname);
	next_in_line = first_name;
	next_status = FN_SUCCESS;
	iter_pos = ip;
}

FN_namelist_hu::~FN_namelist_hu()
{
	free(table_name);
	free(next_in_line);
	free(iter_pos.n_bytes);
}

FN_string*
FN_namelist_hu::next(FN_status &status)
{
	FN_string *answer;

	if (next_status != FN_SUCCESS) {
		status.set(next_status);
		return (0);
	}

	if (next_in_line == 0) {
		next_status = FN_E_INVALID_ENUM_HANDLE;
		status.set_success();
		return (0);
	}

	answer = new FN_string((const unsigned char *)next_in_line);
	free(next_in_line);

	// This will
	// 1. read the next entry
	// 2. free iter_pos
	// 3. reassign iter_pos if any
	next_in_line = FNSP_read_next(table_name, iter_pos, next_status);

	return (answer);
}


FN_namelist* FNSP_HUContext::c_list_names(const FN_string &name,
    FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		// listing names bound in current context
		netobj iter_pos;
		unsigned int status;
		const FN_string tab = ns_impl->my_address->get_table_name();
		const char *table_name = (const char *)tab.str();
		char *first = FNSP_read_first(table_name, iter_pos, status);

		if (status != FN_SUCCESS) {
			cstat.set_error(status, *my_reference, name);
			return (0);
		}
		cstat.set_success();
		return (new FN_namelist_hu(table_name, first, iter_pos));
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

class FN_bindinglist_hu : public FN_bindinglist {
	nis_name table_name;
	char *next_in_line;
	FN_ref *next_ref;
	unsigned int next_status;
	netobj iter_pos;
public:
	FN_bindinglist_hu(const char *table_name, char *name, FN_ref *ref,
			    netobj);
	~FN_bindinglist_hu();
	FN_string* next(FN_ref **ref, FN_status &);
};

FN_bindinglist_hu::FN_bindinglist_hu(const char *tabname, char *first_name,
    FN_ref *first_ref, netobj ip)
{
	table_name = strdup(tabname);
	next_in_line = first_name;
	next_ref = first_ref;
	next_status = FN_SUCCESS;
	iter_pos = ip;
}

FN_bindinglist_hu::~FN_bindinglist_hu()
{
	free(table_name);
	free(next_in_line);
	delete next_ref;
	free(iter_pos.n_bytes);
}

FN_string*
FN_bindinglist_hu::next(FN_ref **ref, FN_status &status)
{
	FN_string *answer;

	if (next_status != FN_SUCCESS) {
		status.set(next_status);
		if (ref)
			*ref = 0;
		return (0);
	}

	if (next_in_line == 0) {
		next_status = FN_E_INVALID_ENUM_HANDLE;
		status.set_success();
		if (ref)
			*ref = 0;
		return (0);
	}

	answer = new FN_string((const unsigned char *)next_in_line);
	free(next_in_line);
	if (ref)
		*ref = next_ref;
	else
		delete next_ref;
	next_ref = 0;

	// This will
	// 1. read the next entry (name and reference)
	// 2. free iter_pos
	// 3. reassign iter_pos if any
	next_in_line = FNSP_read_next(table_name, iter_pos, next_status,
				    &next_ref);

	return (answer);
}



FN_bindinglist* FNSP_HUContext::c_list_bindings(const FN_string &name,
    FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		// listing bindings in current context
		netobj iter_pos;
		unsigned int status;
		FN_ref *first_ref;
		const FN_string tab = ns_impl->my_address->get_table_name();
		const char *table_name = (const char *)tab.str();
		char *first = FNSP_read_first(table_name, iter_pos, status,
					    &first_ref);

		if (status != FN_SUCCESS) {
			cstat.set_error(status, *my_reference, name);
			return (0);
		}
		cstat.set_success();
		return (new FN_bindinglist_hu(table_name, first, first_ref,
					    iter_pos));
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


// Attribute operations

FN_attribute*
FNSP_HUContext::c_attr_get(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_csvc& cs)
{
	if (follow_link && is_following_link(name, cs)) {
		return (0);
	}

	// %%% need to handle name.is_empty() case

	FN_attribute *answer = 0;
	unsigned status;

	if (is_identifier_member(builtin_attrs, id)) {
		answer = builtin_attr_get(name, id, cs);
	} else {
		FNSP_Address *target_context = get_attribute_context(name,
		    status);

		if (status == FN_SUCCESS) {
			answer = FNSP_get_attribute(*target_context, name, id,
			    status);
			delete target_context;
		}
		if (status == FN_SUCCESS) {
			cs.set_success();
		} else {
			cs.set_error(status, *my_reference, name);
		}
	}
	return (answer);
}

int
FNSP_HUContext::c_attr_modify(const FN_string &name,
    unsigned int flag,
    const FN_attribute &attribute, unsigned int follow_link,
    FN_status_csvc& cs)
{
	if (follow_link && is_following_link(name, cs)) {
		return (0);
	}

	// %%% need to handle name.is_empty() case

	unsigned status;
	FNSP_Address *target_context;

	if (is_identifier_member(builtin_attrs, *attribute.identifier())) {
		status = FN_E_ATTR_IN_USE;
	} else {
		target_context = get_attribute_context(name, status,
		    AUTHORITATIVE);
	}

	// Mask all the signals
	sigset_t mask_signal, org_signal;
	sigfillset(&mask_signal);
	sigprocmask(SIG_BLOCK, &mask_signal, &org_signal);
	
	if (status == FN_SUCCESS) {
		switch (flag) {
		case FN_ATTR_OP_ADD:
			status = FNSP_remove_attribute((*target_context), name,
			    &attribute);
			if (status == FN_SUCCESS)
				status = FNSP_set_attribute((*target_context),
				    name, attribute);
			break;

		case FN_ATTR_OP_ADD_EXCLUSIVE: {
			const FN_identifier *id = attribute.identifier();
			if (!(FNSP_get_attribute((*target_context), name,
			    (*id), status)))
				status = FNSP_set_attribute((*target_context),
				    name, attribute);
			else
				status = FN_E_ATTR_IN_USE;
			break;
		}

		case FN_ATTR_OP_ADD_VALUES: {
			const FN_identifier *ident = attribute.identifier();
			const FN_identifier *syntax = attribute.syntax();
			FN_attribute *old_attr;
			if (old_attr = FNSP_get_attribute(
			    (*target_context), name, (*ident), status)) {
				// Check the syntax
				const FN_identifier *old_syntax =
				    old_attr->syntax();
				if ((*syntax) != (*old_syntax)) {
					status = FN_E_INVALID_ATTR_VALUE;
					delete old_attr;
					break;
				}
				delete old_attr;
			}
			status = FNSP_set_attribute((*target_context), name,
			    attribute);
			break;
		}

		case FN_ATTR_OP_REMOVE:
			status = FNSP_remove_attribute(*target_context, name,
			    &attribute);
			break;

		case FN_ATTR_OP_REMOVE_VALUES:
			status = FNSP_remove_attribute_values(*target_context,
			    name, attribute);
			break;

		default:
			status = FN_E_OPERATION_NOT_SUPPORTED;
			break;
		}
		delete target_context;
		// Restore the signals to original
		sigprocmask(SIG_SETMASK, &org_signal, 0);
	}

	// Checking the status after the modify operation
	if (status == FN_SUCCESS) {
		cs.set_success();
		return (1);
	} else {
		cs.set_error(status, *my_reference, name);
		return (0);
	}
}

FN_valuelist*
FNSP_HUContext::c_attr_get_values(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_csvc &cs)
{
	// follow_link is handled by c_attr_get()

	// %%% need to handle name.is_empty() case

	FN_attribute *attribute = c_attr_get(name, id, follow_link, cs);
	if (attribute == 0) {
		return (0);
	}
	FN_valuelist_svc *answer = new FN_valuelist_svc(attribute);
	if (answer == 0) {
		delete attribute;
		cs.set_error(FN_E_INSUFFICIENT_RESOURCES, *my_reference, name);
	}
	return (answer);
}

FN_attrset*
FNSP_HUContext::attr_get_ids_core(const FNSP_Address &target_context,
    const FN_string &name, FN_status_csvc& cs)
{
	FN_attrset *answer;
	unsigned status;

	answer = FNSP_get_attrset(target_context, name, status);
	if (status != FN_SUCCESS) {
		// FNSP_get_attrset returns success
		// for the NOT FOUND case as well, so an error
		// means failed for some other reason
		goto out;
	}
	if (answer == 0) {
		answer = new FN_attrset;
		if (answer == 0) {
			status = FN_E_INSUFFICIENT_RESOURCES;
			goto out;
		}
	}

	// Merge builtin attributes into answer.  Builtins supersede others.
	FN_attrset *bi;
	bi = builtin_attr_get_all(name, cs);
	if (bi == 0) {
		// return what was found thus far
		goto out;
	}
	const FN_attribute *attr;
	void *iter;
	for (attr = bi->first(iter); attr != 0; attr = bi->next(iter)) {
		if (answer->add(*attr, FN_OP_SUPERCEDE) == 0) {
			status = FN_E_INSUFFICIENT_RESOURCES;
			break;
		}
	}
	delete bi;
out:
	if (status == FN_SUCCESS) {
		cs.set_success();
	} else {
		cs.set_error(status, *my_reference, name);
		delete answer;
		answer = 0;
	}
	return (answer);
}


FN_attrset*
FNSP_HUContext::c_attr_get_ids(const FN_string &name, unsigned int follow_link,
    FN_status_csvc& cs)
{
	if (follow_link && is_following_link(name, cs)) {
		return (0);
	}

	// %%% need to handle name.is_empty() case

	unsigned status;
	FNSP_Address *target_context = get_attribute_context(name, status);
	if (status != FN_SUCCESS) {
		cs.set_error(status, *my_reference, name);
		return (0);
	}

	FN_attrset *answer = attr_get_ids_core(*target_context, name, cs);
	delete target_context;

	return (answer);
}

FN_attrset *
FNSP_HUContext::attr_multi_get_core(const FNSP_Address &target_context,
    const FN_string &name,
    const FN_attrset *attrset, FN_status_csvc &cs)
{
	FN_attrset *all;

	if (attrset != 0 && is_identifier_subset(builtin_attrs, *attrset)) {
		all = builtin_attr_get_all(name, cs);
	} else {
		// Use '0' as follow_link argument because
		// caller should already have dealt with links
		all = attr_get_ids_core(target_context, name, cs);
	}

	// If we didn't get any attributes, or if caller is asking
	// for all attributes, no filtering is required.  Return result as is.
	if (all == NULL || attrset == 0)
		return (all);

	FN_attrset *result = FNSP_get_selected_attrset(*all, *attrset);
	delete all;
	return (result);
}


FN_multigetlist*
FNSP_HUContext::c_attr_multi_get(const FN_string &name,
    const FN_attrset *attrset,
    unsigned int follow_link,
    FN_status_csvc &cs)
{
	// %%% need to handle name.is_empty() case

	if (follow_link && is_following_link(name, cs)) {
		return (0);
	}

	// Get argument required for core function
	unsigned status;
	FNSP_Address *target_context = get_attribute_context(name, status);
	if (status != FN_SUCCESS) {
		cs.set_error(status, *my_reference, name);
		return (0);
	}

	FN_attrset *result = attr_multi_get_core(*target_context,
	    name, attrset, cs);

	delete target_context;
	status = cs.code();  		// save for returning at the end

	if (result == NULL)
		return (NULL);		// no attributes were found

	if (status != FN_SUCCESS)	// report error at end of enumeration
		cs.set_success();

	return (new FN_multigetlist_svc(result, status));
}

int
FNSP_HUContext::c_attr_multi_modify(const FN_string &name,
    const FN_attrmodlist &modlist, unsigned int follow_link,
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

// Attribute operations on nns
// (these are identical to the non-nns ones because names bound in HU are
// junctions).



// Definition of 'resolve' that takes into account of possible configuration
// error.
//
// For example, context does not exist for a host, but host entry is in
// 'hosts' table.

FN_ref *
FNSP_HUContext::resolve(const FN_string &name,
    unsigned int lookup_flags, FN_status_csvc &cs)
{
	FN_ref *ref = FNSP_FlatContext::resolve(name, lookup_flags, cs);

	if (cs.code() == FN_E_NAME_NOT_FOUND)
		check_for_config_error(name, cs);

	return (ref);
}

FN_ref *
FNSP_HUContext::c_create_subcontext(const FN_string &name,
    FN_status_csvc& cstat)
{
	unsigned status;
	FN_ref *newref = 0;

	if (name.is_empty()) {
		// there is no nns pointer associated with a HU context
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference,
		    name);
		return (0);
	}

	newref = ns_impl->create_and_bind(name,
	    my_child_context_type, FNSP_normal_repr, status, FNSP_CHECK_NAME);

	if (status == FN_SUCCESS)
		cstat.set_success();
	else
		cstat.set_error(status, *my_reference, name);

	return (newref);
}

FN_ref *
FNSP_HUContext::c_attr_create_subcontext(const FN_string &name,
					    const FN_attrset *attrs,
					    FN_status_csvc &cs)
{
	unsigned context_type;
	unsigned representation_type;
	FN_identifier *ref_type;
	FN_attrset *rest_attrs = FNSP_get_create_params_from_attrs(attrs,
	    context_type, representation_type, &ref_type,
	    my_child_context_type);

	if (name.is_empty() ||
	    context_type != my_child_context_type ||
	    representation_type != FNSP_normal_repr ||
	    ((ref_type != 0) && (*ref_type !=
	    *FNSP_reftype_from_ctxtype(my_child_context_type)))) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference,
			    name);
		delete rest_attrs;
		delete ref_type;
		return (0);
	}

	if (rest_attrs && is_intersection(*rest_attrs, builtin_attrs)) {
		cs.set_error(FN_E_INVALID_ATTR_IDENTIFIER, *my_reference,
			    name);
		delete rest_attrs;
		return (0);
	}

	FN_ref *newref;
	unsigned int status;

	newref = ns_impl->create_and_bind(name,
	    my_child_context_type, FNSP_normal_repr, status, FNSP_CHECK_NAME);

	// add new attributes
	if (status == FN_SUCCESS && rest_attrs && rest_attrs->count() > 0)
		status = add_new_attrs(name, rest_attrs, NULL, cs);
	delete rest_attrs;

	if (status == FN_SUCCESS)
		cs.set_success();
	else
		cs.set_error(status, *my_reference, name);

	return (newref);
}

// Return enumerator when matches is NULL
class FN_searchlist_hu : public FN_searchlist {
	FNSP_nisplusFlatContext *hucontext;
	FN_bindinglist *bl;
	unsigned int return_ref;
	FN_attrset *return_attr_ids;
	FN_status_csvc stat;
 public:
	FN_searchlist_hu(FNSP_nisplusFlatContext *hu,
	    unsigned int rr, const FN_attrset *rai);
	virtual ~FN_searchlist_hu();
	FN_string *next(FN_ref **ref, FN_attrset **attrset,
	    FN_status &status);
};

FN_searchlist_hu::FN_searchlist_hu(FNSP_nisplusFlatContext *hu,
    unsigned int rr, const FN_attrset *rai)
{
	hucontext = hu;
	bl = hucontext->c_list_bindings((unsigned char *) "", stat);
	return_ref = rr;
	if (rai)
		return_attr_ids = new FN_attrset(*rai);
	else
		return_attr_ids = 0;
}

FN_searchlist_hu::~FN_searchlist_hu()
{
	delete hucontext;
	delete bl;
	delete return_attr_ids;
}

FN_string *
FN_searchlist_hu::next(FN_ref **ref, FN_attrset **attrset,
    FN_status &status)
{
	if ((!hucontext) || (!bl)) {
		status.set(FN_E_INSUFFICIENT_RESOURCES);
		return (0);
	}

	FN_string *answer = bl->next(ref, status);
	if ((!answer) || (!status.is_success()))
		return (answer);
	if (attrset == NULL)
		return (answer);

	FN_attrset *fullset = hucontext->c_attr_get_ids(*answer, 0, stat);
	if ((return_attr_ids == NULL) || (return_attr_ids->count() == 0)) {
		*attrset = fullset;
		return (answer);
	}

	*attrset = FNSP_get_selected_attrset(*fullset, *return_attr_ids);
	delete fullset;
	return (answer);
}

FN_searchlist *
FNSP_HUContext::c_attr_search(const FN_string &name,
    const FN_attrset *match_attrs,
    unsigned int return_ref,
    const FN_attrset *return_attr_ids,
    FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		FN_searchset *matches = 0;
		FNSP_Address *attr_hdl = 0;
		unsigned int status = FN_SUCCESS;

		if (match_attrs == NULL) {
			FNSP_nisplusFlatContext *hucontext;
			if (my_child_context_type == FNSP_user_context)
				hucontext = new FNSP_UsernameContext(
				    *my_reference, authoritative);
			else
				hucontext = new FNSP_HostnameContext(
				    *my_reference, authoritative);
			if (!hucontext) {
				cstat.set_error(FN_E_INSUFFICIENT_RESOURCES,
				    *my_reference, name);
				return (0);
			}
			return (new FN_searchlist_hu(hucontext,
			    return_ref, return_attr_ids));
		} else {
			FN_attrset *builtins, *nonbuiltins;
			FN_searchset *bmatches = NULL, *nmatches = NULL;

			divide_attributes(*match_attrs, builtin_attrs,
			    builtins, nonbuiltins);

			// We have a query involving builtins
			if (builtins) {
				bmatches = builtin_attr_search(*builtins,
				    cstat);
				delete builtins;
				status = cstat.code();
				if (bmatches == NULL) {
					// no matches found; terminate search
					delete nonbuiltins;
					return (0);
				}
			}

			// Perform non-builtin query
			if (nonbuiltins) {
				attr_hdl = get_attribute_context(name, status);

				nmatches = multi_attr_search(*attr_hdl,
				    *nonbuiltins, status);
				delete nonbuiltins;
				if (nmatches == NULL) {
					// no matches found; terminate search
					delete bmatches;
					delete attr_hdl;
					goto out;
				}
			}

			// At this point, all searches that were attempted
			// (1 or 2) returned something.
			matches = join_search_results(nmatches, bmatches);
		}

		if (status != FN_SUCCESS || matches == NULL ||
		    matches->count() == 0) {
			delete attr_hdl;
			delete matches;
			goto out;
		}

		// Add references if requested
		if (return_ref) {
			status = search_add_refs(matches);
			if (status != FN_SUCCESS) {
				delete attr_hdl;
				delete matches;
				goto out;
			}
		}

		// Add attributes if requested
		if (return_attr_ids == NULL || return_attr_ids->count() > 0) {
			if (attr_hdl == NULL)
				attr_hdl = get_attribute_context(name, status);
			status = search_add_attrs(*attr_hdl, matches,
			    return_attr_ids);
			if (status != FN_SUCCESS) {
				delete matches;
				delete attr_hdl;
				goto out;
			}
		}

		cstat.set_success();
		delete attr_hdl;
		return (new FN_searchlist_svc(matches));

out:
		if (status == FN_SUCCESS) {
			cstat.set_success();
		} else {
			cstat.set_error(status, *my_reference, name);
		}
		return (0);
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

int
FNSP_HUContext::c_attr_bind(const FN_string &name,
			    const FN_ref &ref,
			    const FN_attrset *attrs,
			    unsigned int exclusive,
			    FN_status_csvc &cstat)
{
	if (attrs != NULL && is_intersection(*attrs, builtin_attrs)) {
		cstat.set_error(FN_E_INVALID_ATTR_IDENTIFIER, *my_reference,
			    name);
		return (0);
	}

	int status = FNSP_FlatContext::c_attr_bind(name, ref,
	    attrs, exclusive, cstat);

	if (status && attrs != NULL) {
		// bind succeeded, fix attributes
		// %%% note: no guarantee of atomicity
		FNSP_Address *target_context;

		// remove any existing attributes
		if (exclusive != 0) {
			if (remove_old_attrs(name, cstat, &target_context) == 0)
				return (0);
		}

		// add new attributes
		if (attrs->count() > 0)
			status = add_new_attrs(name, attrs, target_context,
			    cstat);
	}

	return (status);
}

int
FNSP_HUContext::c_unbind(const FN_string &name, FN_status_csvc &cstat)
{
	int status = FNSP_FlatContext::c_unbind(name, cstat);

	if (status) {
		// remove attributes;
		// %%% note:  no guarantee of atomicity
		status = remove_old_attrs(name, cstat);
	}
	return (status);
}


int
FNSP_HUContext::c_rename(const FN_string &name,
    const FN_composite_name &newname,
    unsigned exclusive, FN_status_csvc &cstat)
{
	int status = FNSP_FlatContext::c_rename(name, newname, exclusive,
	    cstat);

	if (status == 0)
		return (0);

	// move attributes over
	// %%% no atomicity guarantees

	FN_attrset *old_attrs;
	FNSP_Address *new_target = 0;
	void *ip;
	const FN_string *fn = newname.first(ip);

	// If exclusive is 0, we need not worry about old attributes
	// associated with new target because there should be none.
	// For non-exclusive, remove attributes associated with new name
	if (exclusive != 0) {
		if (remove_old_attrs(*fn, cstat, &new_target) == 0) {
			delete old_attrs;
			return (0);
		}
	}

	// remove attributes associated with old name
	if (remove_old_attrs(name, cstat, NULL, &old_attrs) == 0)
		return (0);

	// associate these old attributes with new name
	if (old_attrs) {
		status = add_new_attrs(*fn, old_attrs, new_target, cstat);
		delete old_attrs;
	} else
		delete new_target;

	return (status);
}

int
FNSP_HUContext::c_destroy_subcontext(const FN_string &name,
    FN_status_csvc &cstat)
{
	int status;

	status = FNSP_FlatContext::c_destroy_subcontext(name, cstat);

	if (status) {
		// remove attributes associated with named obj just destroyed
		// no guarantee of atomicity
		status = remove_old_attrs(name, cstat);
	}
	return (status);
}


// Remove all non-builtin attributes associated with name
// and return a copy of those attributes in 'ret_attrs'
int
FNSP_HUContext::remove_old_attrs(const FN_string &name, FN_status_csvc &cstat,
    FNSP_Address **ret_target, FN_attrset **ret_attrs)
{
	void *ip;
	FN_attrset *old_attrs;
	const FN_attribute *attr;
	unsigned int status;
	FNSP_Address *target_context =
		get_attribute_context(name, status, AUTHORITATIVE);

	if (ret_attrs)
		*ret_attrs = NULL;

	if (status == FN_SUCCESS) {
		old_attrs = FNSP_get_attrset(*target_context, name, status);
		if (old_attrs) {
			for (attr = old_attrs->first(ip);
			    attr != NULL && status == FN_SUCCESS;
			    attr = old_attrs->next(ip)) {
				status =
				    FNSP_remove_attribute(*target_context, name,
				    attr);
			}
			if (ret_attrs)
				*ret_attrs = old_attrs;
			else
				delete old_attrs;
		}
		if (ret_target)
			*ret_target = target_context;
		else
			delete target_context;
	}

	if (status != FN_SUCCESS)
		cstat.set_error(status, *my_reference, name);

	return (status == FN_SUCCESS);
}

// Add 'attrs' to named object.
// 'attrs' must be non-builtin attributes, otherwise, it will be hidden
// (will be overridden by builtins).
int
FNSP_HUContext::add_new_attrs(const FN_string &name, const FN_attrset *attrs,
    FNSP_Address *target_context, FN_status_csvc &cstat)
{
	unsigned int status = FN_SUCCESS;
	const FN_attribute *attr;
	void *ip;

	if (target_context == NULL)
		target_context = get_attribute_context(name, status,
		    AUTHORITATIVE);

	// add new attributes
	for (attr = attrs->first(ip);
	    attr != NULL && status == FN_SUCCESS;
	    attr = attrs->next(ip))
		status = FNSP_set_attribute(*target_context, name, *attr);

	if (status != FN_SUCCESS)
		cstat.set_error(status, *my_reference, name);
	delete target_context;

	return (status == FN_SUCCESS);
}

// Retrieve reference associated with items in matches and add them to
// SearchSet

unsigned int
FNSP_HUContext::search_add_refs(FN_searchset *matches)
{
	void *iter_pos;
	void *curr_pos;
	FN_status_csvc cstat;
	FN_ref *ref;
	const FN_string *name;

	if (matches->refs_filled())
		return (1);

	for (name = matches->first(iter_pos, 0, 0, &curr_pos);
	    name != NULL; name = matches->next(iter_pos, 0, 0, &curr_pos)) {
		ref = resolve(*name, 0, cstat);
		if (ref)
			matches->set_ref(curr_pos, ref);
#ifdef TOO_CAREFUL
		else {
			// fail quietly; do not prevent search
			// results from being returned
			// %%% configuration problem: left-over attributes
			if (cstat.code() == FN_E_NAME_NOT_FOUND)
				return (FN_E_CONFIGURATION_ERROR);
			else
				return (cstat.code());
		}
#endif /* TOO_CAREFUL */
	}
	return (FN_SUCCESS);
}

// Retrieve and add attributes named in 'return_attr_ids' to
// the 'attributes' field of items in 'matches.
unsigned int
FNSP_HUContext::search_add_attrs(const FNSP_Address &target_context,
	FN_searchset *matches,
	const FN_attrset *return_attr_ids)
{
	void *iter_pos;
	void *curr_pos;
	FN_status_csvc cstat;
	FN_attrset *attrs;
	const FN_string *name;

	for (name = matches->first(iter_pos, 0, 0, &curr_pos);
	    name != NULL; name = matches->next(iter_pos, 0, 0, &curr_pos)) {
		attrs = attr_multi_get_core(target_context,
		    *name, return_attr_ids, cstat);
		if (attrs)
			matches->set_attrs(curr_pos, attrs);
#ifdef TOO_CAREFUL
		// fail quietly and let successful results be returned
		else {
			return (cstat.code());
		}
#endif /* TOO_CAREFUL */
	}
	return (FN_SUCCESS);
}


// Static helper routines.

static int
is_identifier_subset(const FN_attrset &haystack, const FN_attrset &needles)
{
	const FN_attribute *attr;
	void *ip;
	for (attr = needles.first(ip); attr != 0; attr = needles.next(ip)) {
		if (!is_identifier_member(haystack, *attr->identifier())) {
			return (0);
		}
	}
	return (1);
}



// Divide "origs" into "reds" and "rest" where "reds" are
// items identified in "redlist"

static void divide_attributes(const FN_attrset &origs,
    const FN_attrset &redlist,
    FN_attrset *&reds, FN_attrset *&rest)
{
	const FN_attribute *attr;
	void *ip;

	reds = rest = NULL;

	for (attr = origs.first(ip); attr != NULL; attr = origs.next(ip)) {
		if (is_identifier_member(redlist, *attr->identifier())) {
			if (reds == NULL)
				reds = new FN_attrset;
			reds->add(*attr);
		} else {
			if (rest == NULL)
				rest = new FN_attrset;
			rest->add(*attr);
		}

	}
}

// If either set1 or set2 is NULL, that means one of the searches
// were not attempted, in which case, simply return the one search result.
// Otherwise, we need to take the intersection of the two sets
// because only the intersection satisfied both searches

// Note: upon return, the original input set1, set2 are freed

static FN_searchset *
join_search_results(FN_searchset *set1, FN_searchset *set2)
{
	FN_searchset *joined = NULL;
	const FN_string *name;
	void *ip;

	if (set2 == NULL)
		return (set1);

	if (set1 == NULL)
		return (set2);

	// neither sets are empty, need to take intersection

	for (name = set1->first(ip); name != NULL; name = set1->next(ip)) {
		if (set2->present(*name)) {
			if (joined == NULL)
				joined = new FN_searchset;
			joined->add(*name);
		}
	}
	delete set1;
	delete set2;
	return (joined);
}

// Return entries in 'addr_hdl' that contains all the attributes(values)
// specified by 'attrs'.

static FN_searchset *
multi_attr_search(const FNSP_Address &addr_hdl, const FN_attrset &attrs,
	unsigned int &status)
{
	void *ip;
	const FN_attribute *attr;
	const FN_attrvalue *attr_val;
	FN_searchset *new_matches;
	FN_searchset *curr_matches = NULL;

	for (attr = attrs.first(ip); attr != NULL; attr = attrs.next(ip)) {
		if (attr->valuecount() == 0) {
			// match based on presence of attribute only
			new_matches = FNSP_search_attr(addr_hdl,
			    *(attr->identifier()), *(attr->syntax()),
			    NULL, status);
			if (new_matches == NULL) {
				// an empty match means the whole match failed
				delete curr_matches;
				return (NULL);
			}
			curr_matches = join_search_results(curr_matches,
			    new_matches);
		} else {
			void *vip;
			// match based on attribute/value pair
			for (attr_val = attr->first(vip);
			    attr_val != NULL; attr_val = attr->next(vip)) {
				new_matches = FNSP_search_attr(addr_hdl,
				    *(attr->identifier()), *(attr->syntax()),
				    attr_val, status);
				if (new_matches == NULL) {
					// match failed
					delete curr_matches;
					return (NULL);
				}
				curr_matches = join_search_results(curr_matches,
				    new_matches);
			}
		}
	}

	if (curr_matches != NULL && curr_matches->count() == 0) {
		delete curr_matches;
		curr_matches = NULL;
	}

	return (curr_matches);
}

// Check that the 'attr_id' builtin attribute associated with 'name'
// contains all the values specified in query
// Return 1 if true; 0 otherwise.

int
FNSP_HUContext::builtin_attr_exists(const FN_string &name,
	const FN_identifier &attr_id,
	const FN_attribute &query,
	FN_status_csvc &cstat)
{
	FN_attribute *actuals = builtin_attr_get(name, attr_id, cstat);
	if (actuals != NULL && FNSP_is_attr_subset(*actuals, query))
		return (1);

	return (0);
}

// True if any identifier of "reds" is also in "greens".
static int
is_intersection(const FN_attrset &reds, const FN_attrset &greens)
{
	void *ip;
	const FN_attribute *attr;

	for (attr = reds.first(ip); attr != NULL; attr = reds.next(ip)) {
		if (is_identifier_member(greens, *attr->identifier()))
			return (1);
	}
	return (0);
}

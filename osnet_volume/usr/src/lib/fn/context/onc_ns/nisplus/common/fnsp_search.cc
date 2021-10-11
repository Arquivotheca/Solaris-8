/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnsp_search.cc	1.7	97/08/21 SMI"

#include <rpcsvc/nis.h>
#include <string.h>
#include <stdio.h>

#include "FNSP_nisplus_address.hh"
#include <xfn/FN_searchset.hh>
#include <xfn/FN_searchlist_svc.hh>
#include <xfn/fn_xdr.hh>
#include "fnsp_internal.hh"
#include "fnsp_search.hh"
#include "fnsp_attrs.hh"
#include "fnsp_utils.hh"
#include "fnsp_hostuser.hh"
#include "FNSP_nisplusImpl.hh"

#define	FNSP_NAME_COL 0
#define	FNSP_REF_COL 1
#define	FNSP_ATTR_COL 3

#define	HOST_TABLE "hosts.org_dir"

extern "C" {
nis_result * __nis_list_localcb(nis_name, u_long,
    int (*)(nis_name, nis_object *, void *), void *);
};

static const char *FNSP_attrid_col_label = FNSP_ATTRID_COL_LABEL;
static const char *FNSP_attrvalue_col_label = FNSP_ATTRVAL_COL_LABEL;
static const char *FNSP_org_map = "fns.ctx_dir.";

static const FN_string FNSP_self_name((unsigned char *) FNSP_SELF_NAME);
static const FN_string FNSP_org_attr_str((unsigned char *) "_attribute");

static inline void
free_nis_result(nis_result *res)
{
	if (res)
		nis_freeresult(res);
}


// Operations to support searches

typedef struct {
	const FNSP_Address *parent;
	FN_searchset *ss;
} FNSP_searchset_cb_t;

static int
add_obj_to_searchset(char *, nis_object* ent, void *udata)
{
	FNSP_searchset_cb_t *cbdata = (FNSP_searchset_cb_t *)udata;
	FN_searchset *ss = cbdata->ss;
	FN_ref *ref;
	FN_attrset *attrs;
	unsigned int status;

	ref = FN_ref_xdr_deserialize(ENTRY_VAL(ent, FNSP_REF_COL),
	    ENTRY_LEN(ent, FNSP_REF_COL), status);

	if (ss->attrs_filled())
		attrs = FN_attr_xdr_deserialize(ENTRY_VAL(ent, FNSP_ATTR_COL),
		    ENTRY_LEN(ent, FNSP_ATTR_COL), status);
	else
		attrs = NULL;

	if (ref != NULL) {
		FNSP_process_user_fs(*cbdata->parent, *ref,
		    FNSP_nisplus_get_homedir, status);
		if (status != FN_SUCCESS) {
			delete ref;
			ref = NULL;
		}
	}

	ss->add((unsigned char *)(ENTRY_VAL(ent, FNSP_NAME_COL)), ref, attrs);
	delete attrs;
	delete ref;
	return (0);
}

// Returns all the name in the specified context (parent)
// Used in search when the input expression or matching attributes
// is NULL
FN_searchset *
FNSP_slist_names(const FNSP_Address& parent, unsigned &status)
{
	FNSP_searchset_cb_t cbdata;
	cbdata.parent = &parent;

	switch (parent.get_impl_type()) {
	case FNSP_single_table_impl:
		cbdata.ss = new FN_searchset(1);
		status = FNSP_get_binding_entries(parent.get_table_name(),
		    parent.get_access_flags(),
		    add_obj_to_searchset, &cbdata);
		break;
	case FNSP_shared_table_impl:
		cbdata.ss = new FN_searchset(1, 1);
		status = FNSP_get_binding_entries(parent.get_table_name(),
		    parent.get_access_flags(),
		    add_obj_to_searchset, &cbdata,
		    &FNSP_self_name);
		break;
	case FNSP_entries_impl:
		cbdata.ss = new FN_searchset(1, 1);
		status = FNSP_get_binding_entries(parent.get_table_name(),
		    parent.get_access_flags(),
		    add_obj_to_searchset, &cbdata,
		    &(parent.get_index_name()));
		// get rid of context identifier ('self')
		if (status == FN_SUCCESS)
			cbdata.ss->remove(FNSP_self_name);
		break;
	default:
#ifdef DEBUG
		fprintf(stderr, "bad implementation type = %d\n",
		    parent.get_impl_type());
#endif /* DEBUG */
		status = FN_E_MALFORMED_REFERENCE;
	}

	if (status != FN_SUCCESS) {
		delete cbdata.ss;
		return (NULL);
	}
	return (cbdata.ss);
}

static int
add_obj_from_attrtable_searchset(char *, nis_object* ent, void *udata)
{
	FN_searchset *ss = (FN_searchset *) udata;

	ss->add((unsigned char *)(ENTRY_VAL(ent, FNSP_NAME_COL)), 0, 0);

	return (0);
}

static FN_searchset *
search_table(const FNSP_Address &parent, const char *nisquery,
    unsigned int &status)
{
	nis_result *res;
	unsigned int access_flags = parent.get_access_flags();
	FN_searchset *matches = new FN_searchset;

	res = __nis_list_localcb((nis_name)nisquery,
	    FNSP_nisflags|access_flags,
	    add_obj_from_attrtable_searchset, matches);

	if (res->status == NIS_RPCERROR) {
		// may have failed because too big; use TCP
		free_nis_result(res);
		unsigned long new_flags = (FNSP_nisflags&(~USE_DGRAM));
		res = __nis_list_localcb((nis_name)nisquery,
		    new_flags|access_flags,
		    add_obj_from_attrtable_searchset, matches);
	}

	if ((res->status == NIS_CBRESULTS) || (res->status == NIS_SUCCESS))
		status = FN_SUCCESS;
	else if ((res->status == NIS_NOTFOUND) ||
		    (res->status == NIS_NOSUCHNAME) ||
		    (res->status == NIS_NOSUCHTABLE))
		status = FN_SUCCESS;
	else {
		status = FNSP_map_result(res, 0);
		delete matches;
		matches = NULL;
	}

	if (matches != NULL && matches->count() == 0) {
		delete matches;
		matches = NULL;
	}

	free_nis_result(res);

	return (matches);
}

// Search attribute table for attributes specified by 'attrs'
// If no value is present, then presence of attribute is being queried
static FN_identifier FN_ATTR_SYNTAX_ASCII((const unsigned char *)
	"fn_attr_syntax_ascii");

FN_searchset *
FNSP_search_attr(const FNSP_Address& context,
	const FN_identifier &attr_id, const FN_identifier &syntax,
	const FN_attrvalue *attr_val,
	unsigned int &status,
	const FN_string *table)
{
	if (attr_val != NULL && syntax != FN_ATTR_SYNTAX_ASCII) {
		// can only do searches on ascii
		status = FN_E_SEARCH_INVALID_FILTER;
		return (NULL);
	}

	const FN_string tabname = context.get_table_name();

	char sname[NIS_MAXNAMELEN+1];
	nis_name tablename;
	if (table)
		tablename = (nis_name) table->str(&status);
	else
		tablename = (nis_name) tabname.str(&status);

	if (status != FN_SUCCESS)
		return (0);

	// put in attribute identifier
	sprintf(sname, "[%s=\"%s\"", FNSP_attrid_col_label, attr_id.str());

	// put in attribute value if any
	if (attr_val) {
		size_t lposn;
		strcat(sname, ",");
		strcat(sname, FNSP_attrvalue_col_label);
		strcat(sname, "=\"");
		lposn = strlen(sname);
		strncpy(&sname[lposn], (const char *)(attr_val->contents()),
		    attr_val->length());
		sname[attr_val->length()+lposn] = '"';
		sname[attr_val->length()+lposn+1] = '\0';
	}

	strcat(sname, "],");
	strcat(sname, tablename);
	if (sname[strlen(sname)-1] != '.')
		strcat(sname, ".");

	return (search_table(context, sname, status));
}

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

FN_searchset *
FNSP_multi_attr_search(const FNSP_Address &context, const FN_attrset &attrs,
    unsigned int &status)
{
	void *ip;
	const FN_attribute *attr;
	const FN_attrvalue *attr_val;
	FN_searchset *new_matches;
	FN_searchset *curr_matches = NULL;

	status = FN_SUCCESS;
	for (attr = attrs.first(ip); attr != NULL; attr = attrs.next(ip)) {
		if (attr->valuecount() == 0) {
			// match based on presence of attribute only
			new_matches = FNSP_search_attr(context,
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
				new_matches = FNSP_search_attr(context,
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

static FN_searchset *
FNSP_get_subcontexs(const FNSP_Address &context,
    const FN_searchset &matches, unsigned &status)
{
	// Get the context name
	const FN_string index_name = context.get_index_name();
	char *ctx_name = (char *) index_name.str(&status);
	if (status != FN_SUCCESS)
		return (0);

	// For each match check if the context_name matches
	void *ip;
	char *subctx_name, *ptr;
	FN_searchset *answer = new FN_searchset;
	FN_string *answer_str;
	const FN_string *subctx;
	for (subctx = matches.first(ip); subctx;
	     subctx = matches.next(ip)) {
		subctx_name = (char *) subctx->str(&status);
		if (status != FN_SUCCESS) {
			delete answer;
			return (0);
		}
		if (strncmp(ctx_name, subctx_name, strlen(ctx_name))
		    == 0) {
			// May be a subctx
			ptr = &subctx_name[strlen(ctx_name)];
			if (strncmp(ptr, FNSP_SELF_NAME,
			    strlen(FNSP_SELF_NAME)) == 0)
				ptr += strlen(FNSP_SELF_NAME);
			answer_str = new FN_string((unsigned char *) ptr);
			answer->add(*answer_str);
			delete answer_str;
		}
	}
	return (answer);
}

FN_searchset *
FNSP_search_add_refs(const FNSP_Address &parent, FN_searchset *matches,
    unsigned int return_ref, unsigned int check_subctx, unsigned &status)
{
	void *iter_pos;
	FN_ref *ref;
	const FN_string *name;
	
	status = FN_SUCCESS;
	if ((matches->refs_filled()) ||
	    ((return_ref == 0) && (check_subctx == 0)))
		return (new FN_searchset(*matches));

	FNSP_Address *new_addr = new FNSP_nisplus_address(
	    parent.get_internal_name(),
	    parent.get_context_type(), parent.get_repr_type(),
	    parent.get_access_flags());
	FNSP_nisplusImpl ns_impl(new_addr);
	FN_searchset *answers = new FN_searchset;
	for (name = matches->first(iter_pos);
	    name != NULL; name = matches->next(iter_pos)) {
		ref = ns_impl.lookup_binding(*name, status);
		if (ref) {
			if (return_ref)
				answers->add(*name, ref);
			else
				answers->add(*name);
			delete ref;
		} else {
			// fail quietly; do not prevent search
			// results from being returned
			// %%% configuration problem: left-over attributes
			if (status == FN_E_NAME_NOT_FOUND) {
				if (!check_subctx) {
					status = FN_E_CONFIGURATION_ERROR;
					delete answers;
					return (0);
				}
				status = FN_SUCCESS;
			}
		}
	}
	return (answers);
}

static FN_attrset*
FNSP_attr_multi_get(const FNSP_Address &target_context,
    const FN_string &name, const FN_attrset *return_attr_ids,
    unsigned &status)
{
	FN_attrset *answer;

	answer = FNSP_get_attrset(target_context, name, status);
	if (status != FN_SUCCESS) {
		// FNSP_get_attrset returns success
		// for the NOT FOUND case as well, so an error
		// means failed for some other reason
		return (0);
	}
	if (answer == 0) {
		answer = new FN_attrset;
		if (answer == 0) {
			status = FN_E_INSUFFICIENT_RESOURCES;
			return (answer);
		}
	}

	if (return_attr_ids == 0)
		return (answer);

	FN_attrset *result = FNSP_get_selected_attrset(*answer,
	    *return_attr_ids);
	delete answer;
	return (result);
}

unsigned int
FNSP_search_add_attrs(const FNSP_Address &target_context,
    FN_searchset *matches, const FN_attrset *return_attr_ids)
{
	void *iter_pos, *curr_pos;
	unsigned status;
	FN_attrset *attrs;
	const FN_string *name;

	for (name = matches->first(iter_pos, 0, 0, &curr_pos);
	    name != NULL; name = matches->next(iter_pos, 0, 0, &curr_pos)) {
		attrs = FNSP_attr_multi_get(target_context,
		    *name, return_attr_ids, status);
		if (attrs)
			matches->set_attrs(curr_pos, attrs);
#ifdef TOO_CAREFUL
		// fail quietly and let successful results be returned
		else {
			return (status);
		}
#endif /* TOO_CAREFUL */
	}
	return (FN_SUCCESS);
}

static FN_searchset *
FNSP_org_search_attr(const FNSP_Address &ctx, const FN_attrset &attrs,
    unsigned int return_ref, const FN_attrset *return_attr_ids,
    unsigned int &status)
{
	// Get the FNSP_Address for org attribute context
	const FN_string table_orgname = ctx.get_table_name();
	FN_string *my_orgname = FNSP_orgname_of(table_orgname, status);
	if (status != FN_SUCCESS)
		return (0);
	FN_string *attr_orgname = FNSP_compose_ctx_tablename(
	    FNSP_org_attr_str, *my_orgname);

	FNSP_Address *context = new FNSP_nisplus_address(*attr_orgname,
	    ctx.get_context_type(), FNSP_normal_repr,
	    ctx.get_access_flags());
	delete attr_orgname;
	delete my_orgname;

	FN_searchset *search_results = FNSP_multi_attr_search(*context,
	    attrs, status);
	delete context;
	if ((status != FN_SUCCESS) || (search_results == 0))
		return (search_results);

	// Determine the sub-contexts
	FN_searchset *search_ref = FNSP_get_subcontexs(ctx, *search_results,
	    status);
	delete search_results;
	if ((status != FN_SUCCESS) || (search_ref == 0))
		return (search_ref);

	// Add references if requested
	FN_searchset *answer = FNSP_search_add_refs(ctx, search_ref,
	    return_ref, 1, status);
	delete search_ref;
	if ((status != FN_SUCCESS) || (answer == 0))
		return (answer);

	// Add attributes if requested
	if (return_attr_ids == NULL || return_attr_ids->count() > 0) {
		status = FNSP_search_add_attrs(ctx, answer, return_attr_ids);
		if (status != FN_SUCCESS) {
			delete answer;
			return (0);
		}
	}
	return (answer);
}

FN_searchset *
FNSP_search_host_table(const FNSP_Address &ctx, const FN_string &dirname,
	const char *query, unsigned int &status)
{
	char sname[NIS_MAXNAMELEN + 1];
	const unsigned char *domain = dirname.str();

	sprintf(sname, "[%s],%s.%s", query, HOST_TABLE, domain);
	if (sname[strlen(sname) - 1] != '.') {
		strcat(sname, ".");
	}

	return (search_table(ctx, sname, status));
}

// Only deal with shared or entry context types; does not handle singles

FN_searchset *
FNSP_search_attrset(const FNSP_Address& ctx, const FN_attrset *match_attrs,
	unsigned int return_ref, const FN_attrset *return_attr_ids,
	unsigned int &status)
{
	switch (ctx.get_impl_type()) {
	case FNSP_shared_table_impl:
	case FNSP_entries_impl:
		break;
	default:
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (NULL);
	}

	const FN_string tabname = ctx.get_table_name();
	char *table_name = (char *) tabname.str(&status);
	if (status != FN_SUCCESS)
		return (0);
	if ((strstr(table_name, FNSP_org_map)) && (match_attrs)) {
		return (FNSP_org_search_attr(ctx, *match_attrs,
		    return_ref, return_attr_ids, status));
	}

	FN_searchset *all = FNSP_slist_names(ctx, status);

	if (all == NULL) {
		return (NULL);
	}

	// If not doing filtering, and requesting all attributes to be
	// returned, simply return results now

	if (match_attrs == NULL && return_attr_ids == NULL)
		return (all);

	void *ip;
	const FN_string *name;
	const FN_ref *ref;
	const FN_attrset *stored_attrs, *selected_attrs;
	FN_attrset *sattrs = NULL;
	FN_searchset *matches = new FN_searchset;
	int return_attrs =
		(return_attr_ids == NULL || return_attr_ids->count() > 0);

	if (return_attrs == 0)
		selected_attrs = NULL;

	for (name = all->first(ip, &ref, &stored_attrs);
	    name != NULL; name = all->next(ip, &ref, &stored_attrs)) {
		if (match_attrs == NULL ||
		    (stored_attrs != NULL &&
		    FNSP_is_attrset_subset(*stored_attrs, *match_attrs))) {

			if (return_attr_ids == NULL)
				selected_attrs = stored_attrs;
			else if (return_attrs) {
				// pick out attributes
				sattrs = FNSP_get_selected_attrset(
				    *stored_attrs, *return_attr_ids);
				selected_attrs = sattrs;
			}

			matches->add(*name, (return_ref ? ref : NULL),
				    selected_attrs);

			if (sattrs) {
				delete sattrs;
				sattrs = NULL;
			}
		}
	}
	delete all;
	if (matches != NULL && matches->count() == 0) {
		delete matches;
		matches = NULL;
	}
	return (matches);
}

FN_searchlist *
FNSP_nisplusImpl::search_attrset(const FN_attrset *match_attrs,
	unsigned int return_ref, const FN_attrset *return_attr_ids,
	unsigned int &status)
{
	FN_searchset *answer = FNSP_search_attrset(*my_address,
	    match_attrs, return_ref, return_attr_ids, status);
	if (answer)
		return (new FN_searchlist_svc(answer));
	else
		return (0);
}

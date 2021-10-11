/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_ctx_asvc_strong.cc	1.7	96/11/27 SMI"

#include <xfn/fn_spi.hh>

static FN_string empty_name((unsigned char *)"");

// Resolve to context named by 'n'
//
// Returns 1 if at context named by `n` (this only happens
// if 'n' is the empty name)
//
// Returns 0 if not at context named by 'n' (and sets status
// to continue resolution on parts of 'n' not yet resolved).
//

static int
resolve_to_context(FN_ctx_asvc_strong *ctx,
    const FN_string &n, FN_status_csvc &cs)
{
	int ret = 0;
	FN_string *rn = 0;
	FN_string *fn = ctx->c_component_parser(n, &rn, cs);
	if (fn == 0) {
		cs.set_error_context(cs.code(), *ctx, &n);
		return (0);
	}
	FN_status_asvc as(cs);
	if (rn || !fn->is_empty()) {
		// more components so follow binding
		FN_ref *nref = ctx->a_lookup(*fn, 0, as);
		if (!cs.is_success()) {
			if (cs.is_continue())
				cs.set_remaining_name(rn);
			else
				cs.set_remaining_name(&n);
		} else {
			// attempt to continue
			cs.set_continue_context(*nref, *ctx,
						(rn ? rn : &empty_name));
			delete nref;
		}
		if (rn)
			delete rn;
	} else {
		ret = 1;
	}
	delete fn;
	return (ret);
}


// Resolve to penultimate context named by 'n'
// (i.e. that named by all but the final atomic name of 'n')
//
// Returns 1 if penultimate context is reached (this only happends
// if 'n' has one component left)
//
// Returns 0 if not at penultimate context named by 'n' (and sets
// status to continue resolution on parts of 'n' not yet resolved).

static int
resolve_to_penultimate_context(FN_ctx_asvc_strong *ctx,
    const FN_string &n, FN_status_csvc &cs)
{
	int ret = 0;
	FN_string *rn = 0;
	FN_string *fn = ctx->c_component_parser(n, &rn, cs);
	if (fn == 0) {
		cs.set_error_context(cs.code(), *ctx, &n);
		return (ret);
	}
	FN_status_asvc as(cs);
	if (rn) {
		// more components so follow binding
		FN_ref *nref = ctx->a_lookup(*fn, 0, as);
		if (!cs.is_success()) {
			// resolution failed
			if (cs.is_continue())
				cs.set_remaining_name(rn);
			else
				cs.set_remaining_name(&n);
		} else {
			// pass up continue
			cs.set_continue_context(*nref, *ctx, rn);
			delete nref;
		}
		delete rn;
	} else {
		ret = 1;
	}
	delete fn;
	return (ret);
}

static int
resolve_to_nns_and_continue(FN_ctx_asvc_strong *ctx,
    const FN_string &n, FN_status_csvc &cs)
{
	if (resolve_to_penultimate_context(ctx, n, cs)) {
		FN_status_asvc as(cs);
		// lookup NNS binding
		FN_ref *nref = ctx->a_lookup_nns(n, 0, as);
		if (cs.is_success()) {
			// set up continue status info
			cs.set_continue_context(*nref, *ctx);
			delete nref;
		}
	}
	return (0);
}


FN_ctx_asvc_strong::FN_ctx_asvc_strong()
{
}

FN_ctx_asvc_strong::~FN_ctx_asvc_strong()
{
}

FN_ref *
FN_ctx_asvc_strong::c_lookup(const FN_string &n,
    unsigned int lookup_flags, FN_status_csvc &cs)
{
	FN_ref *ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// lookup final component
		ret = a_lookup(n, lookup_flags, as);
		if (cs.is_success() && ret->is_link()) {
			if (!(lookup_flags&FN_SPI_LEAVE_TERMINAL_LINK)) {
				// need to follow link
				cs.set_continue_context(*ret, *this);
				delete ret;
				ret = 0;
			} else {
				// record last resolved reference
				cs.set_error_context(FN_SUCCESS, *this);
			}
		}
	}
	return (ret);
}

FN_namelist*
FN_ctx_asvc_strong::c_list_names(const FN_string &n, FN_status_csvc &cs)
{
	FN_namelist* ret = 0;
	if (resolve_to_context(this, n, cs)) {
		// list final context
		FN_status_asvc as(cs);
		ret = a_list_names(as);
	}
	return (ret);
}

FN_bindinglist*
FN_ctx_asvc_strong::c_list_bindings(const FN_string &n, FN_status_csvc &cs)
{
	FN_bindinglist* ret = 0;
	if (resolve_to_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// list final context
		ret = a_list_bindings(as);
	}
	return (ret);
}


int
FN_ctx_asvc_strong::c_bind(const FN_string &n, const FN_ref &r,
    unsigned f, FN_status_csvc &cs)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// bind final component
		ret = a_bind(n, r, f, as);
	}
	return (ret);
}

int
FN_ctx_asvc_strong::c_unbind(const FN_string &n, FN_status_csvc &cs)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// unbind final component
		ret = a_unbind(n, as);
	}
	return (ret);
}

FN_ref *
FN_ctx_asvc_strong::c_create_subcontext(const FN_string &n,
    FN_status_csvc &cs)
{
	FN_ref *ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// create final component
		ret = a_create_subcontext(n, as);
	}
	return (ret);
}

int
FN_ctx_asvc_strong::c_destroy_subcontext(const FN_string &n,
    FN_status_csvc &cs)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		ret = a_destroy_subcontext(n, as);
	}
	return (ret);
}

int
FN_ctx_asvc_strong::c_rename(const FN_string &n, const FN_composite_name &newn,
    unsigned f, FN_status_csvc &cs)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		ret = a_rename(n, newn, f, as);
	}
	return (ret);
}


FN_attrset*
FN_ctx_asvc_strong::c_get_syntax_attrs(const FN_string &n,
    FN_status_csvc &cs)
{
	FN_attrset* ret = 0;
	if (resolve_to_context(this, n, cs)) {
		// get final contexts attrs
		FN_status_asvc as(cs);
		ret = a_get_syntax_attrs(as);
	}
	return (ret);
}

FN_attribute*
FN_ctx_asvc_strong::c_attr_get(const FN_string &n, const FN_identifier &i,
    unsigned int fl, FN_status_csvc &cs)
{
	FN_attribute* ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		// get final contexts attrs
		FN_status_asvc as(cs);
		ret = a_attr_get(n, i, fl, as);
	}
	return (ret);
}

int
FN_ctx_asvc_strong::c_attr_modify(const FN_string &n, unsigned int i,
    const FN_attribute &a, unsigned int fl, FN_status_csvc &cs)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		// get final contexts attrs
		FN_status_asvc as(cs);
		ret = a_attr_modify(n, i, a, fl, as);
	}
	return (ret);
}

FN_valuelist *
FN_ctx_asvc_strong::c_attr_get_values(const FN_string &n,
    const FN_identifier &i, unsigned int fl, FN_status_csvc &cs)
{
	FN_valuelist *ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		// get final contexts attrs
		FN_status_asvc as(cs);
		ret = a_attr_get_values(n, i, fl, as);
	}
	return (ret);
}

FN_attrset *
FN_ctx_asvc_strong::c_attr_get_ids(const FN_string &n,
    unsigned int fl, FN_status_csvc &cs)
{
	FN_attrset* ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		// get final contexts attrs
		FN_status_asvc as(cs);
		ret = a_attr_get_ids(n, fl, as);
	}
	return (ret);
}

FN_multigetlist *
FN_ctx_asvc_strong::c_attr_multi_get(const FN_string &n, const FN_attrset *a,
    unsigned int fl, FN_status_csvc &cs)
{
	FN_multigetlist* ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		// get final contexts attrs
		FN_status_asvc as(cs);
		ret = a_attr_multi_get(n, a, fl, as);
	}
	return (ret);
}

int
FN_ctx_asvc_strong::c_attr_multi_modify(const FN_string &n,
    const FN_attrmodlist &m, unsigned int fl,
    FN_attrmodlist **a, FN_status_csvc &cs)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		// get final contexts attrs
		FN_status_asvc as(cs);
		ret = a_attr_multi_modify(n, m, fl, a, as);
	}
	return (ret);
}

int
FN_ctx_asvc_strong::c_attr_bind(const FN_string &n, const FN_ref &r,
    const FN_attrset *attrs, unsigned f, FN_status_csvc &cs)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// bind final component
		ret = a_attr_bind(n, r, attrs, f, as);
	}
	return (ret);
}

FN_ref *
FN_ctx_asvc_strong::c_attr_create_subcontext(const FN_string &n,
    const FN_attrset *a, FN_status_csvc &cs)
{
	FN_ref *ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// create final component
		ret = a_attr_create_subcontext(n, a, as);
	}
	return (ret);
}

FN_searchlist *
FN_ctx_asvc_strong::c_attr_search(const FN_string &n,
	    const FN_attrset *match_attrs,
	    unsigned int rf,
	    const FN_attrset *ra,
	    FN_status_csvc &cs)
{
	FN_searchlist *ret = 0;
	if (resolve_to_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// search final context
		ret = a_attr_search(match_attrs, rf, ra, as);
	}
	return (ret);
}

FN_ext_searchlist *
FN_ctx_asvc_strong::c_attr_ext_search(const FN_string &n,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
    FN_status_csvc &cs)
{
	FN_ext_searchlist *ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// search final component
		ret = a_attr_ext_search(n, control, filter, as);
	}
	return (ret);
}

FN_ref *
FN_ctx_asvc_strong::c_lookup_nns(const FN_string &n,
    unsigned int lookup_flags, FN_status_csvc &cs)
{
	FN_ref *ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// lookup NNS binding
		ret = a_lookup_nns(n, lookup_flags, as);
		if (cs.is_success() && ret->is_link()) {
			if (!(lookup_flags&FN_SPI_LEAVE_TERMINAL_LINK)) {
				//
				cs.set_continue_context(*ret, *this);
				delete ret;
				ret = 0;
			} else {
				// set resolved_ref field in case of rel link
				cs.set_error_context(FN_SUCCESS, *this);
			}
		}
	}
	return (ret);
}

FN_namelist*
FN_ctx_asvc_strong::c_list_names_nns(const FN_string &n,
    FN_status_csvc &cs)
{
	resolve_to_nns_and_continue(this, n, cs);
	return ((FN_namelist *)NULL);
}

FN_bindinglist*
FN_ctx_asvc_strong::c_list_bindings_nns(const FN_string &n,
					FN_status_csvc &cs)
{
	resolve_to_nns_and_continue(this, n, cs);
	return ((FN_bindinglist *)NULL);
}

int
FN_ctx_asvc_strong::c_bind_nns(const FN_string &n, const FN_ref &r,
    unsigned f, FN_status_csvc &cs)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// bind NNS context
		ret = a_bind_nns(n, r, f, as);
	}
	return (ret);
}

int
FN_ctx_asvc_strong::c_unbind_nns(const FN_string &n,
    FN_status_csvc &cs)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		ret = a_unbind_nns(n, as);
	}
	return (ret);
}

FN_ref *
FN_ctx_asvc_strong::c_create_subcontext_nns(const FN_string &n,
    FN_status_csvc &cs)
{
	FN_ref *ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// create NNS context
		ret = a_create_subcontext_nns(n, as);
	}
	return (ret);
}

int
FN_ctx_asvc_strong::c_destroy_subcontext_nns(const FN_string &n,
    FN_status_csvc &cs)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// destroy NNS context
		ret = a_destroy_subcontext_nns(n, as);
	}
	return (ret);
}

int
FN_ctx_asvc_strong::c_rename_nns(const FN_string &n,
    const FN_composite_name &newn, unsigned int f, FN_status_csvc &cs)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// rename NNS context
		ret = a_rename_nns(n, newn, f, as);
	}
	return (ret);
}


FN_attrset*
FN_ctx_asvc_strong::c_get_syntax_attrs_nns(const FN_string &n,
    FN_status_csvc &cs)
{
	resolve_to_nns_and_continue(this, n, cs);
	return ((FN_attrset *)NULL);
}

FN_attribute*
FN_ctx_asvc_strong::c_attr_get_nns(const FN_string &n, const FN_identifier &i,
    unsigned int fl, FN_status_csvc &cs)
{
	FN_attribute *ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		// get final contexts attrs
		FN_status_asvc as(cs);
		ret = a_attr_get_nns(n, i, fl, as);
	}
	return (ret);
}

int
FN_ctx_asvc_strong::c_attr_modify_nns(const FN_string &n,
    unsigned int i, const FN_attribute &a,
    unsigned int fl, FN_status_csvc &cs)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		// get final contexts attrs
		FN_status_asvc as(cs);
		ret = a_attr_modify_nns(n, i, a, fl, as);
	}
	return (ret);
}

FN_valuelist*
FN_ctx_asvc_strong::c_attr_get_values_nns(const FN_string &n,
    const FN_identifier &i, unsigned int fl, FN_status_csvc &cs)
{
	FN_valuelist *ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		// get final contexts attrs
		FN_status_asvc as(cs);
		ret = a_attr_get_values_nns(n, i, fl, as);
	}
	return (ret);
}

FN_attrset*
FN_ctx_asvc_strong::c_attr_get_ids_nns(const FN_string &n,
    unsigned int fl, FN_status_csvc &cs)
{
	FN_attrset *ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		// get final contexts attrs
		FN_status_asvc as(cs);
		ret = a_attr_get_ids_nns(n, fl, as);
	}
	return (ret);
}

FN_multigetlist*
FN_ctx_asvc_strong::c_attr_multi_get_nns(const FN_string &n,
    const FN_attrset *i, unsigned int fl, FN_status_csvc &cs)
{
	FN_multigetlist *ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		// get final contexts attrs
		FN_status_asvc as(cs);
		ret = a_attr_multi_get_nns(n, i, fl, as);
	}
	return (ret);
}

int
FN_ctx_asvc_strong::c_attr_multi_modify_nns(const FN_string &n,
    const FN_attrmodlist &a, unsigned int fl,
    FN_attrmodlist **l, FN_status_csvc &cs)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		// get final contexts attrs
		FN_status_asvc as(cs);
		ret = a_attr_multi_modify_nns(n, a, fl, l, as);
	}
	return (ret);
}

int
FN_ctx_asvc_strong::c_attr_bind_nns(const FN_string &n, const FN_ref &r,
    const FN_attrset *attrs, unsigned f, FN_status_csvc &cs)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// bind NNS context
		ret = a_attr_bind_nns(n, r, attrs, f, as);
	}
	return (ret);
}

FN_ref *
FN_ctx_asvc_strong::c_attr_create_subcontext_nns(const FN_string &n,
    const FN_attrset *attrs, FN_status_csvc &cs)
{
	FN_ref *ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// create NNS context
		ret = a_attr_create_subcontext_nns(n, attrs, as);
	}
	return (ret);
}

FN_searchlist *
FN_ctx_asvc_strong::c_attr_search_nns(const FN_string &n,
	    const FN_attrset * /* match_attrs */,
	    unsigned int /* rf */,
	    const FN_attrset * /* ra */,
	    FN_status_csvc &cs)
{
	resolve_to_nns_and_continue(this, n, cs);
	return ((FN_searchlist *)NULL);
}

FN_ext_searchlist *
FN_ctx_asvc_strong::c_attr_ext_search_nns(const FN_string &n,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
    FN_status_csvc &cs)
{
	FN_ext_searchlist *ret = 0;
	if (resolve_to_penultimate_context(this, n, cs)) {
		FN_status_asvc as(cs);
		// search final component
		ret = a_attr_ext_search_nns(n, control, filter, as);
	}
	return (ret);
}

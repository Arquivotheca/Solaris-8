/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_ctx_asvc_weak.cc	1.6	96/03/31 SMI"

#include <xfn/fn_spi.hh>

// Error object setting policy:
// If SPI continue status
//	a_ interface sets error code and resolved_ref (and link_resolved_ref)
//	(remaining_name field is set to 0)
//	asvc_weak sets remaining_name field with residual
// else
//	a_ interface sets error code and resolved_ref and remaining_name
//	asvc_weak sets remaining_name field with {atomic+residual} when
//	in the middle of resolution; otherwise, remaining_name left as
//	is set by a_ interface (which should be the atomic name)

static FN_composite_name empty_name((unsigned char *)"");

// Resolve to context named by 'n'
//
// Returns 1 if at context named by `n` (this only happens
// if 'n' is the empty name)
//
// Returns 0 if not at context named by 'n' (and sets status
// to continue resolution on parts of 'n' not yet resolved).
//

static int
resolve_to_context(FN_ctx_asvc_weak *ctx, const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	int ret = 0;
	void *p;
	const FN_string *fn = n.first(p);
	if (fn == 0) {
		cns.set_error_context(FN_E_ILLEGAL_NAME, *ctx, &n);
		return (ret);
	}
	FN_composite_name *rn = n.suffix(p);
	if (rn || !fn->is_empty()) {
		FN_status_asvc as(cns);
		// more components so follow binding
		FN_ref *nref = ctx->a_lookup(*fn, 0, as);
		if (!cns.is_success()) {
			if (cns.is_continue())
				cns.set_remaining_name(rn);
			else
				cns.set_remaining_name(&n);
		} else {
			// pass up continue
			cns.set_continue_context(*nref, *ctx,
			    (rn ? rn : &empty_name));
			delete nref;
		}
		if (rn)
			delete rn;
		ret = 0;
	} else {
		// operate on final context
		ret = 1;
	}
	return (ret);
}

// Resolve to penultimate context named by 'n'
// (i.e. that named by all but the final atomic name of 'n')
//
// Returns 1 if penultimate context is reached (this only happens
// if 'n' has one component left)
//
// Returns 0 if not at penultimate context named by 'n' (and sets
// status to continue resolution on parts of 'n' not yet resolved).

static int
resolve_to_penultimate_context(FN_ctx_asvc_weak *ctx,
    const FN_composite_name &n, FN_status_cnsvc &cns)
{
	int ret = 0;
	void *p;
	const FN_string *fn = n.first(p);
	if (fn == 0) {
		cns.set_error_context(FN_E_ILLEGAL_NAME, *ctx, &n);
		return (0);
	}
	FN_composite_name *rn = n.suffix(p);
	if (rn) {
		FN_status_asvc as(cns);
		// more components so follow binding
		FN_ref *nref = ctx->a_lookup(*fn, 0, as);
		if (!cns.is_success()) {
			if (cns.is_continue())
				cns.set_remaining_name(rn);
			else
				cns.set_remaining_name(&n);
		} else {
			// resolution succeeded
			cns.set_continue_context(*nref, *ctx, rn);
			delete nref;
		}
		delete rn;
		ret = 0;
	} else {
		// penultimate context reached
		// operate on final component
		ret = 1;
	}
	return (ret);
}


static int
resolve_to_nns_and_continue(FN_ctx_asvc_weak *ctx,
    const FN_composite_name &n, FN_status_cnsvc &cns)
{
	if (resolve_to_penultimate_context(ctx, n, cns)) {
		FN_status_asvc as(cns);
		// lookup NNS binding
		FN_string *fn = n.string();
		if (fn) {
			FN_ref *nref = ctx->a_lookup_nns(*fn, 0, as);
			if (cns.is_success()) {
				// set up continue status info
				cns.set_continue_context(*nref, *ctx);
				delete nref;
			}
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (0);
}


FN_ctx_asvc_weak::FN_ctx_asvc_weak()
{
}

FN_ctx_asvc_weak::~FN_ctx_asvc_weak()
{
}


FN_ref *
FN_ctx_asvc_weak::cn_lookup(const FN_composite_name &n,
    unsigned int lookup_flags, FN_status_cnsvc &cns)
{
	FN_ref *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		// lookup final component
		FN_string *fn = n.string();
		if (fn) {
			ret = a_lookup(*fn, lookup_flags, as);
			delete fn;
			if (cns.is_success() && ret->is_link()) {
				if (!(lookup_flags&
				    FN_SPI_LEAVE_TERMINAL_LINK)) {
					// pass back continue to follow link
					cns.set_continue_context(*ret, *this,
					    0);
					delete ret;
					ret = 0;
				} else {
					// set last resolved ref
					// for resolving relative link
					cns.set_error_context(FN_SUCCESS, *this,
					    0);
				}
			}
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}


FN_namelist*
FN_ctx_asvc_weak::cn_list_names(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_namelist *ret = 0;
	if (resolve_to_context(this, n, cns)) {
		FN_status_asvc as(cns);
		// list final context
		ret = a_list_names(as);
	}
	return (ret);
}

FN_bindinglist*
FN_ctx_asvc_weak::cn_list_bindings(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_bindinglist* ret = 0;
	if (resolve_to_context(this, n, cns)) {
		FN_status_asvc as(cns);
		ret = a_list_bindings(as);
	}
	return (ret);
}

int
FN_ctx_asvc_weak::cn_bind(const FN_composite_name &n, const FN_ref &r,
    unsigned f, FN_status_cnsvc &cns)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		// bind final component
		FN_string *fn = n.string();
		if (fn) {
			ret = a_bind(*fn, r, f, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

int
FN_ctx_asvc_weak::cn_unbind(const FN_composite_name &n, FN_status_cnsvc &cns)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		// unbind final component
		FN_string *fn = n.string();
		if (fn) {
			ret = a_unbind(*fn, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

FN_ref *
FN_ctx_asvc_weak::cn_create_subcontext(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_ref *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			// create final component
			ret = a_create_subcontext(*fn, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

int
FN_ctx_asvc_weak::cn_destroy_subcontext(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			// destroy final component
			ret = a_destroy_subcontext(*fn, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

int
FN_ctx_asvc_weak::cn_rename(const FN_composite_name &n,
    const FN_composite_name &newn, unsigned f, FN_status_cnsvc &cns)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			// rename final component
			ret = a_rename(*fn, newn, f, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

FN_attrset*
FN_ctx_asvc_weak::cn_get_syntax_attrs(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_attrset* ret = 0;
	if (resolve_to_context(this, n, cns)) {
		FN_status_asvc as(cns);
		ret = a_get_syntax_attrs(as);
	}
	return (ret);
}

FN_attribute*
FN_ctx_asvc_weak::cn_attr_get(const FN_composite_name &n,
    const FN_identifier &i, unsigned int fl, FN_status_cnsvc &cns)
{
	FN_attribute* ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_get(*fn, i, fl, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

int
FN_ctx_asvc_weak::cn_attr_modify(const FN_composite_name &n,
    unsigned int i, const FN_attribute &a, unsigned int fl,
    FN_status_cnsvc &cns)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_modify(*fn, i, a, fl, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

FN_valuelist *
FN_ctx_asvc_weak::cn_attr_get_values(const FN_composite_name &n,
    const FN_identifier &a, unsigned int fl, FN_status_cnsvc &cns)
{
	FN_valuelist *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_get_values(*fn, a, fl, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

FN_attrset*
FN_ctx_asvc_weak::cn_attr_get_ids(const FN_composite_name &n,
    unsigned int fl, FN_status_cnsvc &cns)
{
	FN_attrset *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_get_ids(*fn, fl, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

FN_multigetlist *
FN_ctx_asvc_weak::cn_attr_multi_get(const FN_composite_name &n,
    const FN_attrset *a, unsigned int fl, FN_status_cnsvc &cns)
{
	FN_multigetlist *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_multi_get(*fn, a, fl, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

int
FN_ctx_asvc_weak::cn_attr_multi_modify(const FN_composite_name &n,
    const FN_attrmodlist &m, unsigned int fl,
    FN_attrmodlist **u, FN_status_cnsvc &cns)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_multi_modify(*fn, m, fl, u, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}


/* **** extended attribute operations **** */
int
FN_ctx_asvc_weak::cn_attr_bind(const FN_composite_name &n, const FN_ref &r,
    const FN_attrset *attrs, unsigned f, FN_status_cnsvc &cns)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		// bind final component
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_bind(*fn, r, attrs, f, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

FN_ref *
FN_ctx_asvc_weak::cn_attr_create_subcontext(const FN_composite_name &n,
    const FN_attrset *attrs, FN_status_cnsvc &cns)
{
	FN_ref *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			// create final component
			ret = a_attr_create_subcontext(*fn, attrs, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

FN_searchlist *
FN_ctx_asvc_weak::cn_attr_search(const FN_composite_name &n,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_cnsvc &cns)
{
	FN_searchlist *ret = 0;
	if (resolve_to_context(this, n, cns)) {
		FN_status_asvc as(cns);
		ret = a_attr_search(match_attrs, return_ref, return_attr_ids,
		    as);
	}
	return (ret);
}

FN_ext_searchlist *
FN_ctx_asvc_weak::cn_attr_ext_search(
	    const FN_composite_name &n,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_cnsvc &cns)
{
	FN_ext_searchlist *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			// Search final component
			ret = a_attr_ext_search(*fn, control, filter, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}


/* ******** NNS operations **************************** */
FN_ref *
FN_ctx_asvc_weak::cn_lookup_nns(const FN_composite_name &n,
    unsigned int lookup_flags, FN_status_cnsvc &cns)
{
	FN_ref *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		// lookup NNS binding
		FN_string *fn = n.string();
		if (fn) {
			ret = a_lookup_nns(*fn, lookup_flags, as);
			delete fn;
			if (cns.is_success() && ret->is_link()) {
				// pass back continue to follow link
				if (!(lookup_flags&
				    FN_SPI_LEAVE_TERMINAL_LINK)) {
					cns.set_continue_context(*ret,
					    *this, 0);
					delete ret;
					ret = 0;
				} else {
					// set last resolved ref
					// for resolving relative link
					cns.set_error_context(FN_SUCCESS,
					    *this);
				}
			}
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

FN_namelist*
FN_ctx_asvc_weak::cn_list_names_nns(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	resolve_to_nns_and_continue(this, n, cns);
	return ((FN_namelist *)NULL);
}

FN_bindinglist*
FN_ctx_asvc_weak::cn_list_bindings_nns(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	resolve_to_nns_and_continue(this, n, cns);
	return ((FN_bindinglist *)NULL);
}

FN_attrset*
FN_ctx_asvc_weak::cn_get_syntax_attrs_nns(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	resolve_to_nns_and_continue(this, n, cns);
	return ((FN_attrset *)NULL);
}

int
FN_ctx_asvc_weak::cn_bind_nns(const FN_composite_name &n, const FN_ref &r,
    unsigned f, FN_status_cnsvc &cns)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		// bind NNS of final component
		FN_string *fn = n.string();
		if (fn) {
			ret = a_bind_nns(*fn, r, f, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

int
FN_ctx_asvc_weak::cn_unbind_nns(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		// unbind NNS context of final component
		FN_string *fn = n.string();
		if (fn) {
			ret = a_unbind_nns(*fn, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

FN_ref *
FN_ctx_asvc_weak::cn_create_subcontext_nns(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_ref *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		// create NNS context for final component
		FN_string *fn = n.string();
		if (fn) {
			// create final component
			ret = a_create_subcontext_nns(*fn, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

int
FN_ctx_asvc_weak::cn_destroy_subcontext_nns(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			// destroy final component
			ret = a_destroy_subcontext_nns(*fn, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

int
FN_ctx_asvc_weak::cn_rename_nns(const FN_composite_name &n,
    const FN_composite_name &newn, unsigned f, FN_status_cnsvc &cns)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			// rename final component
			ret = a_rename_nns(*fn, newn, f, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}


FN_attribute*
FN_ctx_asvc_weak::cn_attr_get_nns(const FN_composite_name &n,
    const FN_identifier &i, unsigned int fl, FN_status_cnsvc &cns)
{
	FN_attribute *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_get_nns(*fn, i, fl, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

int
FN_ctx_asvc_weak::cn_attr_modify_nns(const FN_composite_name &n,
    unsigned int i, const FN_attribute &a, unsigned int fl,
    FN_status_cnsvc &cns)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_modify_nns(*fn, i, a, fl, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}


FN_valuelist*
FN_ctx_asvc_weak::cn_attr_get_values_nns(const FN_composite_name &n,
    const FN_identifier &i, unsigned int fl, FN_status_cnsvc &cns)
{
	FN_valuelist *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_get_values_nns(*fn, i, fl, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

FN_attrset*
FN_ctx_asvc_weak::cn_attr_get_ids_nns(const FN_composite_name &n,
    unsigned int fl, FN_status_cnsvc &cns)
{
	FN_attrset *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_get_ids_nns(*fn, fl, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

FN_multigetlist*
FN_ctx_asvc_weak::cn_attr_multi_get_nns(const FN_composite_name &n,
    const FN_attrset *a, unsigned int fl, FN_status_cnsvc &cns)
{
	FN_multigetlist *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_multi_get_nns(*fn, a, fl, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

int
FN_ctx_asvc_weak::cn_attr_multi_modify_nns(const FN_composite_name &n,
    const FN_attrmodlist &i, unsigned int fl,
    FN_attrmodlist **l, FN_status_cnsvc &cns)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_multi_modify_nns(*fn, i, fl, l, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

/* extended attribute nns operations  */
int
FN_ctx_asvc_weak::cn_attr_bind_nns(const FN_composite_name &n, const FN_ref &r,
    const FN_attrset *attrs, unsigned f, FN_status_cnsvc &cns)
{
	int ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		// bind NNS of final component
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_bind_nns(*fn, r, attrs, f, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

FN_ref *
FN_ctx_asvc_weak::cn_attr_create_subcontext_nns(const FN_composite_name &n,
    const FN_attrset *attrs, FN_status_cnsvc &cns)
{
	FN_ref *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		// create NNS context for final component
		FN_string *fn = n.string();
		if (fn) {
			// create final component
			ret = a_attr_create_subcontext_nns(*fn, attrs, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

FN_searchlist *
FN_ctx_asvc_weak::cn_attr_search_nns(const FN_composite_name &n,
	    const FN_attrset * /* match_attrs */,
	    unsigned int /* return_ref */,
	    const FN_attrset * /* return_attr_ids */,
	    FN_status_cnsvc &cns)
{
	resolve_to_nns_and_continue(this, n, cns);
	return ((FN_searchlist *)NULL);
}

FN_ext_searchlist *
FN_ctx_asvc_weak::cn_attr_ext_search_nns(
	    const FN_composite_name &n,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_cnsvc &cns)
{
	FN_ext_searchlist *ret = 0;
	if (resolve_to_penultimate_context(this, n, cns)) {
		FN_status_asvc as(cns);
		FN_string *fn = n.string();
		if (fn) {
			ret = a_attr_ext_search_nns(*fn,
						    control, filter, as);
			delete fn;
		} else {
			cns.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (ret);
}

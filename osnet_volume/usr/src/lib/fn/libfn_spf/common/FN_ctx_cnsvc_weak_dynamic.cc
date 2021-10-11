/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_ctx_cnsvc_weak_dynamic.cc	1.5	96/03/31 SMI"

#include <xfn/fn_spi.hh>

FN_ctx_cnsvc_weak_dynamic::FN_ctx_cnsvc_weak_dynamic()
{
}

FN_ctx_cnsvc_weak_dynamic::~FN_ctx_cnsvc_weak_dynamic()
{
}

int
FN_ctx_cnsvc_weak_dynamic::p_resolve(const FN_composite_name &n,
    FN_status_psvc &ps, FN_composite_name **ret_fn)
{
	int ret = RS_STATUS_SET;
	void *p;
	const FN_string *fnstr = n.first(p);
	if (fnstr == 0) {
		ps.set_error_context(FN_E_ILLEGAL_NAME, *this, &n);
		return (0);
	}

	FN_composite_name *fn = new FN_composite_name(*fnstr);
	FN_composite_name *rn = n.suffix(p);
	FN_status_cnsvc rs(ps);
	if (ret_fn)
		*ret_fn = fn;
	if (rn) {
		// more than one component to name
		if (fn->is_empty()) {
			// leading component is empty
			if (!rn->is_empty()) {
				// leading empty name with suffix
				// (e.g. /a/b/c, /a, /a/, /a/b/)
				// follow nns and continue operation on 'rn'
				FN_ref *nref = cn_lookup_nns(*fn, 0, rs);
				if (!ps.is_success()) {
					ps.append_remaining_name(*rn);
				} else {
					ps.set_continue_context(*nref,
					    *this, rn);
					delete nref;
				}
			} else {
				// leading null name only
				// i.e. single slash as name ("/")
				ret = RS_TERMINAL_NNS_COMPONENT;
			}
		} else {
			// name has both non-empty leader and suffix
			// e.g. a/b, a/b/
			const FN_string *lastcomp = n.last(p);
			if (lastcomp == 0) {
				// impossible
				ps.set_error_context(FN_E_ILLEGAL_NAME,
				    *this, &n);
				return (0);
			}
			if (lastcomp->is_empty()) {
				ret = RS_TERMINAL_NNS_COMPONENT;
				delete fn;
				// get everything except for trailer
				fn = n.prefix(p);
				if (ret_fn)
					*ret_fn = fn;
			} else {
				ret = RS_TERMINAL_COMPONENT;
				delete fn;
				// return entire name
				fn = new FN_composite_name(n);
				if (ret_fn)
					*ret_fn = fn;
			}
		}
		delete rn;
	} else {
		// single (terminal) component name (i.e. no separators)
		// e.g. a, b, c
		ret = RS_TERMINAL_COMPONENT;
	}
	return (ret);
}

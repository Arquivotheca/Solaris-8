/*
 * Copyright (c) 1992 - 1993 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)FN_ctx_cnsvc_weak_static.cc	1.3 94/11/15 SMI"

#include <xfn/fn_spi.hh>

FN_ctx_cnsvc_weak_static::FN_ctx_cnsvc_weak_static()
{
}

FN_ctx_cnsvc_weak_static::~FN_ctx_cnsvc_weak_static()
{
}

int
FN_ctx_cnsvc_weak_static::p_resolve(const FN_composite_name &n,
    FN_status_psvc &ps, FN_composite_name **ret_fn)
{
	int ret = RS_STATUS_SET;
	FN_composite_name *rn = 0;
	FN_composite_name *fn = p_component_parser(n, &rn, ps);
	if (ret_fn)
		*ret_fn = fn;
	if (fn == 0) {
		ps.set_error_context(ps.code(), *this, &n);
		return (ret);
	}
	FN_status_cnsvc bs(ps);
	if (rn) {
		if (!rn->is_empty()) {
			// more components so follow NNS binding
			FN_ref *nref;
			nref = cn_lookup_nns(*fn, 0, bs);
			if (!ps.is_success()) {
				ps.append_remaining_name(*rn);
			} else {
				ps.set_continue_context(*nref, *this, rn);
				delete nref;
			}
			ret = RS_STATUS_SET;
		} else {
			// try NNS context
			ret = RS_TERMINAL_NNS_COMPONENT;
		}
		delete rn;
	} else {
		ret = RS_TERMINAL_COMPONENT;
	}
	return (ret);
}

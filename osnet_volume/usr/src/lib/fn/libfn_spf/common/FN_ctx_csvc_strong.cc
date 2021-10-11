/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)FN_ctx_csvc_strong.cc	1.2 94/08/04 SMI"

#include <xfn/fn_spi.hh>


FN_ctx_csvc_strong::FN_ctx_csvc_strong()
{
}

FN_ctx_csvc_strong::~FN_ctx_csvc_strong()
{
}

// returns copy of first component (strong separation)
// and set 'rn' to remaining components
FN_composite_name *
FN_ctx_csvc_strong::p_component_parser(const FN_composite_name &n,
    FN_composite_name **rn, FN_status_psvc &ps)
{
	void *p;
	const FN_string *first_comp = n.first(p);
	ps.set_success();
	if (first_comp) {
		if (rn)
			*rn = n.suffix(p);
		// separate construction of answer (instead of making one call
		// to FN_composite_name(const FN_string &)constructor) so that
		// the constructor try to parse it and eat up any
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

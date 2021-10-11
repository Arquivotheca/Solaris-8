/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisplusDotContext.cc	1.3	96/03/31 SMI"

#include "FNSP_nisplusDotContext.hh"
#include <FNSP_Syntax.hh>
#include <xfn/fn_p.hh>

FNSP_nisplusDotContext::FNSP_nisplusDotContext(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth)
: FNSP_nisplusHierContext(from_addr, from_ref, auth)
{
	my_syntax = FNSP_Syntax(FNSP_site_context);
}


FNSP_nisplusDotContext*
FNSP_nisplusDotContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth,
    FN_status &stat)
{
	FNSP_nisplusDotContext *answer =
		new FNSP_nisplusDotContext(from_addr, from_ref, auth);

	if (answer && answer->my_reference && answer->ns_impl &&
	    answer->ns_impl->my_address)
		stat.set_success();
	else {
		if (answer) {
			delete answer;
			answer = 0;
		}
		stat.set(FN_E_INSUFFICIENT_RESOURCES);
	}
	return (answer);
}

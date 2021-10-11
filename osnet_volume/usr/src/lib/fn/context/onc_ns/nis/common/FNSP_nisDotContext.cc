/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisDotContext.cc	1.1	96/03/31 SMI"

#include "FNSP_nisDotContext.hh"
#include <FNSP_Syntax.hh>

FNSP_nisDotContext::FNSP_nisDotContext(const FN_ref_addr &from_addr,
    const FN_ref &from_ref)
: FNSP_nisHierContext(from_addr, from_ref)
{
	my_syntax = FNSP_Syntax(FNSP_site_context);
}


FNSP_nisDotContext*
FNSP_nisDotContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    FN_status &stat)
{
	FNSP_nisDotContext *answer = new FNSP_nisDotContext(from_addr,
	    from_ref);

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

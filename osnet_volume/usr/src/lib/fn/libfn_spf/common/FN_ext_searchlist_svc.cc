/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_ext_searchlist_svc.cc	1.1	96/03/31 SMI"

#include <xfn/FN_ext_searchlist_svc.hh>

enum {
	FN_ITER_NOT_STARTED = 0,
	FN_ITER_IN_PROGRESS = 1,
	FN_ITER_COMPLETED = 2
	};

FN_ext_searchlist_svc::FN_ext_searchlist_svc(FN_ext_searchset *ss,
    unsigned int es)
{
	search_hits = ss;
	iter_status = FN_ITER_NOT_STARTED;
	iter_pos = 0;
	end_status = es;
}

FN_ext_searchlist_svc::~FN_ext_searchlist_svc()
{
	if (search_hits)
		delete search_hits;
}

FN_composite_name*
FN_ext_searchlist_svc::next(FN_ref **ret_ref, FN_attrset **ret_attrs,
	unsigned int &rel, FN_status &status)
{
	const FN_composite_name *answer = 0;
	const FN_ref *ref = 0;
	const FN_attrset *attrs = 0;
	FN_composite_name *ret_answer = 0;

	switch (iter_status) {
	case FN_ITER_NOT_STARTED:
		answer = search_hits->first(iter_pos, (ret_ref ? &ref : NULL),
		    (ret_attrs ? &attrs : NULL), &rel);
		iter_status = FN_ITER_IN_PROGRESS;
		status.set_success();
		break;
	case FN_ITER_IN_PROGRESS:
		answer = search_hits->next(iter_pos, (ret_ref ? &ref : NULL),
		    (ret_attrs ? &attrs : NULL), &rel);
		status.set_success();
		break;
	default:
		answer = 0;
		end_status = FN_E_INVALID_ENUM_HANDLE; // reset
	}

	if (answer == 0) {
		iter_status = FN_ITER_COMPLETED;
		status.set(end_status);
	} else {
		ret_answer = new FN_composite_name(*answer);
	}

	if (ret_ref)
		*ret_ref = (ref ? new FN_ref(*ref) : 0);

	if (ret_attrs)
		*ret_attrs = (attrs ? new FN_attrset(*attrs) : 0);

	return (ret_answer);
}

/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_bindinglist_svc.cc	1.3	96/03/31 SMI"

#include <xfn/FN_bindinglist_svc.hh>

enum {
	FN_ITER_NOT_STARTED = 0,
	FN_ITER_IN_PROGRESS = 1,
	FN_ITER_COMPLETED = 2
	};

FN_bindinglist_svc::FN_bindinglist_svc(FN_bindingset *bs, unsigned int es)
{
	bindings = bs;
	iter_status = FN_ITER_NOT_STARTED;
	end_status = es;
	iter_pos = 0;
}

FN_bindinglist_svc::~FN_bindinglist_svc()
{
	if (bindings)
		delete bindings;
}

FN_string*
FN_bindinglist_svc::next(FN_ref **final_ref, FN_status &status)
{
	const FN_string *answer = 0;
	const FN_ref *ref = 0;
	FN_string *final_answer = 0;

	switch (iter_status) {
	case FN_ITER_NOT_STARTED:
		answer = bindings->first(iter_pos, ref);
		iter_status = FN_ITER_IN_PROGRESS;
		status.set_success();
		break;
	case FN_ITER_IN_PROGRESS:
		answer = bindings->next(iter_pos, ref);
		status.set_success();
		break;
	default:
		answer = 0;
		end_status = FN_E_INVALID_ENUM_HANDLE;
	}

	if (answer == 0) {
		iter_status = FN_ITER_COMPLETED;
		status.set(end_status);
	} else {
		final_answer = new FN_string(*answer);
		if (final_ref)
			*final_ref = new FN_ref(*ref);
	}
	return (final_answer);
}

/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)FN_multigetlist_svc.cc	1.4	96/03/31 SMI"

#include <xfn/FN_multigetlist_svc.hh>

enum {
	FN_ITER_NOT_STARTED = 0,
	FN_ITER_IN_PROGRESS = 1,
	FN_ITER_COMPLETED = 2
	};

FN_multigetlist_svc::FN_multigetlist_svc(FN_attrset *as, unsigned int es)
{
	attribute_set = as;
	iter_status = FN_ITER_NOT_STARTED;
	end_status = es;
	iter_pos = 0;
}

FN_multigetlist_svc::~FN_multigetlist_svc()
{
	if (attribute_set)
		delete attribute_set;
}

FN_attribute*
FN_multigetlist_svc::next(FN_status &status)
{
	const FN_attribute *answer = 0;
	FN_attribute *final_answer = 0;

	if (attribute_set) {
		switch (iter_status) {
		case FN_ITER_NOT_STARTED:
			answer = attribute_set->first(iter_pos);
			iter_status = FN_ITER_IN_PROGRESS;
			break;
		case FN_ITER_IN_PROGRESS:
			answer = attribute_set->next(iter_pos);
			break;
		default:
			answer = 0;
			// Should have returned end_status once already
			// Reset it to invalid handle for subsequent returns
			end_status = FN_E_INVALID_ENUM_HANDLE;
		}
	}

	if (answer == 0) {
		iter_status = FN_ITER_COMPLETED;
		status.set(end_status);
	} else {
		final_answer = new FN_attribute(*answer);  // make copy
		status.set_success();
	}
	return (final_answer);
}

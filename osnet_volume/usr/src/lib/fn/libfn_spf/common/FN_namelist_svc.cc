/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_namelist_svc.cc	1.3	96/03/31 SMI"

#include <xfn/FN_namelist_svc.hh>

enum {
	FN_ITER_NOT_STARTED = 0,
	FN_ITER_IN_PROGRESS = 1,
	FN_ITER_COMPLETED = 2
	};

FN_namelist_svc::FN_namelist_svc(FN_nameset *ns, unsigned int es)
{
	names = ns;
	iter_status = FN_ITER_NOT_STARTED;
	end_status = es;
	iter_pos = 0;
}

FN_namelist_svc::~FN_namelist_svc()
{
	if (names)
		delete names;
}

FN_string*
FN_namelist_svc::next(FN_status &status)
{
	const FN_string *answer = 0;
	FN_string *final_answer = 0;

	switch (iter_status) {
	case FN_ITER_NOT_STARTED:
		answer = names->first(iter_pos);
		iter_status = FN_ITER_IN_PROGRESS;
		status.set_success();
		break;
	case FN_ITER_IN_PROGRESS:
		answer = names->next(iter_pos);
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
		final_answer = new FN_string(*answer);  // make copy
	}
	return (final_answer);
}

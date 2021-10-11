/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_BINDINGLIST_SVC_HH
#define	_XFN_FN_BINDINGLIST_SVC_HH

#pragma ident	"@(#)FN_bindinglist_svc.hh	1.4	96/03/31 SMI"

#include <xfn/xfn.hh>
#include <xfn/FN_bindingset.hh>

class FN_bindinglist_svc : public FN_bindinglist {
	FN_bindingset *bindings;
	void *iter_pos;
	unsigned int iter_status;
	unsigned int end_status;
public:
	FN_bindinglist_svc(FN_bindingset *, unsigned int status = FN_SUCCESS);
	~FN_bindinglist_svc();
	FN_string *next(FN_ref **, FN_status &);
};

#endif /* _XFN_FN_BINDINGLIST_SVC_HH */

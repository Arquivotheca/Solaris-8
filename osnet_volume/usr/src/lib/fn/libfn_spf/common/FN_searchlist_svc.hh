/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_SEARCHLIST_SVC_HH
#define	_XFN_FN_SEARCHLIST_SVC_HH

#pragma ident	"@(#)FN_searchlist_svc.hh	1.1	96/03/31 SMI"

#include <xfn/xfn.hh>
#include <xfn/FN_searchset.hh>

class FN_searchlist_svc : public FN_searchlist {
	FN_searchset *search_hits;
	void *iter_pos;
	unsigned int iter_status;
	unsigned int end_status;
public:
	FN_searchlist_svc(FN_searchset *, unsigned int status = FN_SUCCESS);
	~FN_searchlist_svc();
	FN_string *next(FN_ref **, FN_attrset **, FN_status &);
};

#endif /* _XFN_FN_SEARCHLIST_SVC_HH */

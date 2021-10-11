/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_EXT_SEARCHLIST_SVC_HH
#define	_XFN_FN_EXT_SEARCHLIST_SVC_HH

#pragma ident	"@(#)FN_ext_searchlist_svc.hh	1.1	96/03/31 SMI"

#include <xfn/xfn.hh>
#include <xfn/FN_ext_searchset.hh>

class FN_ext_searchlist_svc : public FN_ext_searchlist {
	FN_ext_searchset *search_hits;
	void *iter_pos;
	unsigned int iter_status;
	unsigned int end_status;
public:
	FN_ext_searchlist_svc(FN_ext_searchset *,
	    unsigned int status = FN_SUCCESS);
	~FN_ext_searchlist_svc();
	FN_composite_name *next(FN_ref **, FN_attrset **,
	    unsigned int& rel, FN_status &);
};

#endif /* _XFN_FN_EXT_SEARCHLIST_SVC_HH */

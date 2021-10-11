/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN__FN_NAMELIST_SVC_HH
#define	_XFN__FN_NAMELIST_SVC_HH

#pragma ident	"@(#)FN_namelist_svc.hh	1.4	96/03/31 SMI"

#include <xfn/xfn.hh>
#include <xfn/FN_nameset.hh>

class FN_namelist_svc : public FN_namelist {
	FN_nameset *names;
	void *iter_pos;
	unsigned int iter_status;
	unsigned int end_status;
public:
	FN_namelist_svc(FN_nameset *, unsigned int status = FN_SUCCESS);
	~FN_namelist_svc();
	FN_string* next(FN_status &);
};

#endif /* _XFN__FN_NAMELIST_SVC_HH */

/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 */

#ifndef _XFN__FN_VALUELIST_SVC_HH
#define	_XFN__FN_VALUELIST_SVC_HH

#pragma ident	"@(#)FN_valuelist_svc.hh	1.5	96/03/31 SMI"

#include <xfn/xfn.hh>

class FN_valuelist_svc : public FN_valuelist {
	FN_attribute *attribute;
	void *iter_pos;
	unsigned int iter_status;
	unsigned int end_status;
public:
	FN_valuelist_svc(FN_attribute *, unsigned int status = FN_SUCCESS);
	~FN_valuelist_svc();
	FN_attrvalue *next(FN_identifier **, FN_status &);
};

#endif /* _XFN__FN_VALUELIST_SVC_HH */

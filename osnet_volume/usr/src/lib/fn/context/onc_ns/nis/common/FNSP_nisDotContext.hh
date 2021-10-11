/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_NISDOTCONTEXT_HH
#define	_FNSP_NISDOTCONTEXT_HH

#pragma ident	"@(#)FNSP_nisDotContext.hh	1.1	96/03/31 SMI"

#include "FNSP_nisHierContext.hh"

class FNSP_nisDotContext : public FNSP_nisHierContext {
public:
	static FNSP_nisDotContext* from_address(const FN_ref_addr&,
	    const FN_ref&,
	    FN_status& stat);

private:
	FNSP_nisDotContext(const FN_ref_addr& from_addr,
	    const FN_ref& from_ref);
};


#endif /* _FNSP_NISDOTCONTEXT_HH */

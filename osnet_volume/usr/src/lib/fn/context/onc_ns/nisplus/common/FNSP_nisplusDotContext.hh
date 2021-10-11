/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_NISPLUSDOTCONTEXT_HH
#define	_FNSP_NISPLUSDOTCONTEXT_HH

#pragma ident	"@(#)FNSP_nisplusDotContext.hh	1.3	96/03/31 SMI"

#include "FNSP_nisplusHierContext.hh"

class FNSP_nisplusDotContext : public FNSP_nisplusHierContext {
public:
	static FNSP_nisplusDotContext* from_address(const FN_ref_addr&,
	    const FN_ref&, unsigned int auth,
	    FN_status& stat);

private:
	FNSP_nisplusDotContext(const FN_ref_addr& from_addr,
	    const FN_ref& from_ref, unsigned int auth);
};


#endif /* _FNSP_NISPLUSDOTCONTEXT_HH */

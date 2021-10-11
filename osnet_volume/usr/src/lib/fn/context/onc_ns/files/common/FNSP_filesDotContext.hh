/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_FILESDOTCONTEXT_HH
#define	_FNSP_FILESDOTCONTEXT_HH

#pragma ident	"@(#)FNSP_filesDotContext.hh	1.1	96/03/31 SMI"

#include "FNSP_filesHierContext.hh"

class FNSP_filesDotContext : public FNSP_filesHierContext {
public:
	static FNSP_filesDotContext* from_address(const FN_ref_addr&,
	    const FN_ref&,
	    FN_status& stat);

private:
	FNSP_filesDotContext(const FN_ref_addr& from_addr,
	    const FN_ref& from_ref);
};


#endif /* _FNSP_FILESDOTCONTEXT_HH */

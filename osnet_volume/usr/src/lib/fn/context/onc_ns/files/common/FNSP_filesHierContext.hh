/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */


#ifndef _FNSP_FILESHIERCONTEXT_HH
#define	_FNSP_FILESHIERCONTEXT_HH

#pragma ident	"@(#)FNSP_filesHierContext.hh	1.1	96/03/31 SMI"

#include <FNSP_HierContext.hh>

/* For:  FNSP_generic_context, FNSP_service_context, FNSP_site_context */

class FNSP_filesHierContext : public FNSP_HierContext {
public:
	~FNSP_filesHierContext();
	FNSP_filesHierContext(const FN_ref_addr &, const FN_ref &);
};

#endif	/* _FNSP_FILESHIERCONTEXT_HH */

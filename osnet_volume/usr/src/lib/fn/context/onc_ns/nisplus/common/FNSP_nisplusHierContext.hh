/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_NISPLUSHIERCONTEXT_HH
#define	_FNSP_NISPLUSHIERCONTEXT_HH

#pragma ident	"@(#)FNSP_nisplusHierContext.hh	1.7	96/03/31 SMI"

#include <FNSP_HierContext.hh>

/* For:  FNSP_generic_context, FNSP_service_context, FNSP_site_context */
class FNSP_nisplusHierContext : public FNSP_HierContext {
protected:
	FNSP_nisplusHierContext(const FN_ref_addr &, const FN_ref &,
	    unsigned int);
public:
	~FNSP_nisplusHierContext();
};

#endif	/* _FNSP_NISPLUSHIERCONTEXT_HH */

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_FNSP_NISFLATCONTEXT_HH
#define	_FNSP_NISFLATCONTEXT_HH

#pragma ident	"@(#)FNSP_nisFlatContext.hh	1.1	96/03/31 SMI"

#include "FNSP_FlatContext.hh"
#include "FNSP_nisImpl.hh"

/* For supporting flat naming systems; used for NamingSystemNames */

class FNSP_nisFlatContext : public FNSP_FlatContext {
protected:
	FNSP_nisImpl *nis_impl;	// nis-specific handle for nis-specific funcs
	FNSP_nisFlatContext(const FN_ref_addr &, const FN_ref &);
public:
	virtual ~FNSP_nisFlatContext();
	static FNSP_nisFlatContext* from_address(const FN_ref_addr &,
	    const FN_ref &,
	    FN_status &stat);

	// probably only used for testing
	FNSP_nisFlatContext(const FN_string &,
			    unsigned context_type = FNSP_nsid_context);
	FNSP_nisFlatContext(const FN_ref &);
};

#endif	/* _FNSP_NISFLATCONTEXT_HH */

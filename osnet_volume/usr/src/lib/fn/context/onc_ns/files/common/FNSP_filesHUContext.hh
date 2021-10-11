/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_FNSP_FILESHUCONTEXT_HH
#define	_FNSP_FILESHUCONTEXT_HH

#pragma ident	"@(#)FNSP_filesHUContext.hh	1.1	96/03/31 SMI"

#include "FNSP_filesFlatContext.hh"

/* For supporting host/user naming systems */

class FNSP_filesHUContext : public FNSP_filesFlatContext {
protected:
	// internal functions
	FNSP_filesHUContext(const FN_ref_addr &, const FN_ref &);
public:
	virtual ~FNSP_filesHUContext();
	static FNSP_filesHUContext* from_address(const FN_ref_addr &,
	    const FN_ref &,
	    FN_status &stat);

	// probably only used for testing
	FNSP_filesHUContext(const FN_string &,
	    unsigned context_type = FNSP_nsid_context);
	FNSP_filesHUContext(const FN_ref &);

	FN_namelist* c_list_names(const FN_string &n, FN_status_csvc&);
	FN_bindinglist* c_list_bindings(const FN_string &name,
	    FN_status_csvc&);
};

#endif	/* _FNSP_FILESHUCONTEXT_HH */

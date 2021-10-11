/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_FNSP_FILESFLATCONTEXT_HH
#define	_FNSP_FILESFLATCONTEXT_HH

#pragma ident	"@(#)FNSP_filesFlatContext.hh	1.1	96/03/31 SMI"

#include <xfn/fn_spi.hh>
#include <xfn/fn_p.hh>
#include <FNSP_FlatContext.hh>
#include "FNSP_filesImpl.hh"

/* For supporting flat naming systems; used for NamingSystemNames */

class FNSP_filesFlatContext : public FNSP_FlatContext {
protected:
	// files-specific handle for subclasses to use
	FNSP_filesImpl *files_impl;
	FNSP_filesFlatContext(const FN_ref_addr &, const FN_ref &);

public:
	virtual ~FNSP_filesFlatContext();
	static FNSP_filesFlatContext* from_address(const FN_ref_addr &,
	    const FN_ref &,
	    FN_status &stat);

	// probably only used for testing
	FNSP_filesFlatContext(const FN_string &,
			    unsigned context_type = FNSP_nsid_context);
	FNSP_filesFlatContext(const FN_ref &);
};

#endif	/* _FNSP_FILESFLATCONTEXT_HH */

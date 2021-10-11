/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_FNSP_NISPLUSFLATCONTEXT_HH
#define	_FNSP_NISPLUSFLATCONTEXT_HH

#pragma ident	"@(#)FNSP_nisplusFlatContext.hh	1.8	96/03/31 SMI"

#include <xfn/fn_p.hh>
#include <FNSP_FlatContext.hh>

/*
 * For supporting flat naming systems; these include:
 * - Namespace ID context
 * - Username context
 * - Hostname context
 * Username and hostname contexts use this as their base class.
 */

class FNSP_nisplusFlatContext : public FNSP_FlatContext {
protected:
	// internal functions
	FNSP_nisplusFlatContext(const FN_ref_addr &, const FN_ref &,
	    unsigned int auth = 0);
public:
	virtual ~FNSP_nisplusFlatContext();
	static FNSP_nisplusFlatContext* from_address(const FN_ref_addr &,
	    const FN_ref &, unsigned int auth, FN_status &stat);

	// probably only used for testing
	FNSP_nisplusFlatContext(const FN_string &, unsigned int auth = 0,
			    unsigned context_type = FNSP_nsid_context);
	FNSP_nisplusFlatContext(const FN_ref &, unsigned int auth = 0);
};

#endif	/* _FNSP_NISPLUSFLATCONTEXT_HH */

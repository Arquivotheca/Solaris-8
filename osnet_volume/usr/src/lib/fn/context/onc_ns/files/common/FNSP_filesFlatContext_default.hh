/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_FILESFLATCONTEXT_DEFAULT_HH
#define	_FNSP_FILESFLATCONTEXT_DEFAULT_HH

#pragma ident	"@(#)FNSP_filesFlatContext_default.hh	1.2	97/10/21 SMI"

#include <FNSP_nisFlatContext_default.hh>

class FNSP_filesFlatContext_default : public FNSP_nisFlatContext_default {
    protected:
	// internal functions
	FNSP_filesFlatContext_default(const FN_ref_addr&, const FN_ref&);
	FN_ref *make_service_ref();
	FN_ref *make_printername_ref();

public:
#ifdef DEBUG
	// only used for testing
	FNSP_filesFlatContext_default(const FN_identifier *addr_type,
	    const FN_string&);
	FNSP_filesFlatContext_default(const FN_ref&);
#endif /* DEBUG */

	~FNSP_filesFlatContext_default();
	static FNSP_filesFlatContext_default* from_address(
	    const FN_ref_addr&, const FN_ref&, FN_status& stat);

};

#endif /* _FNSP_FILESFLATCONTEXT_DEFAULT_HH */

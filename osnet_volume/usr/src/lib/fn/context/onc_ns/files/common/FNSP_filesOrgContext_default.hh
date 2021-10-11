/*
 * Copyright (c) 1994-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_FILESORGCONTEXT_DEFAULT_HH
#define	_FNSP_FILESORGCONTEXT_DEFAULT_HH

#pragma ident	"@(#)FNSP_filesOrgContext_default.hh	1.2	97/10/21 SMI"

#include <FNSP_nisOrgContext_default.hh>

/* For context type OrganizationName */
class FNSP_filesOrgContext_default : public FNSP_nisOrgContext_default {
    protected:
	// internal functions
	FNSP_filesOrgContext_default(const FN_ref_addr&, const FN_ref&);
	FN_ref *make_nsid_ref();
	FN_ref *make_service_ref();

    public:
	~FNSP_filesOrgContext_default();

#ifdef DEBUG
	// only used for testing
	FNSP_filesOrgContext_default(const FN_identifier *addr_type,
	    const FN_string&);
	FNSP_filesOrgContext_default(const FN_ref&);
#endif /* DEBUG */

	static FNSP_filesOrgContext_default* from_address(
	    const FN_ref_addr&, const FN_ref&, FN_status& stat);
};

#endif /* _FNSP_FILESORGCONTEXT_DEFAULT_HH */

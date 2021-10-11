/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 */

#ifndef _FNSP_FILESORGCONTEXT_HH
#define	_FNSP_FILESORGCONTEXT_HH

#pragma ident	"@(#)FNSP_filesOrgContext.hh	1.1	96/03/31 SMI"

#include <FNSP_OrgContext.hh>

/* For context type OrganizationName */
class FNSP_filesOrgContext : public FNSP_OrgContext {
protected:
	// internal functions
	FNSP_filesOrgContext(const FN_ref_addr &, const FN_ref &);
	FNSP_Impl * get_nns_impl(const FN_ref &ref,
	    unsigned &status, FN_string **dirname_holder = 0);

public:
	~FNSP_filesOrgContext();

	// probably not used (only for testing)
	FNSP_filesOrgContext(const FN_string &);
	FNSP_filesOrgContext(const FN_ref &);

	static FNSP_filesOrgContext* from_address(const FN_ref_addr &,
	    const FN_ref &,
	    FN_status &stat);
};

#endif	/* _FNSP_FILESORGCONTEXT_HH */

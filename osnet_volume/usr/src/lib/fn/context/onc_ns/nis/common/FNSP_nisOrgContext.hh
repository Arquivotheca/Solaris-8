/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 */

#ifndef _FNSP_NISORGCONTEXT_HH
#define	_FNSP_NISORGCONTEXT_HH

#pragma ident	"@(#)FNSP_nisOrgContext.hh	1.1	96/03/31 SMI"

#include <FNSP_OrgContext.hh>

/* For context type OrganizationName */
class FNSP_nisOrgContext : public FNSP_OrgContext {
protected:
	// internal functions
	FNSP_nisOrgContext(const FN_ref_addr &, const FN_ref &);
	FN_ref *resolve(const FN_string &, FN_status_csvc&);

	FNSP_Impl *get_nns_impl(const FN_ref &ref, unsigned &status,
	    FN_string **dirname_holder = 0);
public:
	~FNSP_nisOrgContext();

	// probably not used (only for testing)
	FNSP_nisOrgContext(const FN_string &);
	FNSP_nisOrgContext(const FN_ref &);

	static FNSP_nisOrgContext* from_address(const FN_ref_addr &,
	    const FN_ref &, FN_status &stat);

	FN_ref *c_lookup_nns(const FN_string &name, unsigned int f,
	    FN_status_csvc&);
};

#endif	/* _FNSP_NISORGCONTEXT_HH */

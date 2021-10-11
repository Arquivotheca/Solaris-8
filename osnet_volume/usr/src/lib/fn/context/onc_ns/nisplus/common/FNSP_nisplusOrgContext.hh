/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_NISPLUSORGCONTEXT_HH
#define	_FNSP_NISPLUSORGCONTEXT_HH

#pragma ident	"@(#)FNSP_nisplusOrgContext.hh	1.6	96/03/31 SMI"

#include <FNSP_OrgContext.hh>
#include <xfn/fn_p.hh>

/* For context type OrganizationName */
class FNSP_nisplusOrgContext : public FNSP_OrgContext {
private:
	FNSP_nisplusOrgContext(const FN_ref_addr &, const FN_ref &,
	    unsigned int);
	FN_ref *resolve(const FN_string &, FN_status_csvc&);
public:
	~FNSP_nisplusOrgContext();

	// probably not used (only for testing)
	FNSP_nisplusOrgContext(const FN_string &, unsigned int auth = 0);
	FNSP_nisplusOrgContext(const FN_ref &, unsigned int auth = 0);

	static FNSP_nisplusOrgContext* from_address(const FN_ref_addr &,
	    const FN_ref &, unsigned int auth,
	    FN_status &stat);

	FN_namelist* c_list_names(const FN_string &name, FN_status_csvc&);
	FN_bindinglist* c_list_bindings(const FN_string &name,
	    FN_status_csvc&);

	FN_ref *c_lookup_nns(const FN_string &name, unsigned int f,
	    FN_status_csvc&);
	FN_bindinglist* c_list_bindings_nns(const FN_string &name,
	    FN_status_csvc&);

	FNSP_Impl *get_nns_impl(const FN_ref &ref,
	    unsigned &status, FN_string **dirname_holder = 0);
};

#endif	/* _FNSP_NISPLUSORGCONTEXT_HH */

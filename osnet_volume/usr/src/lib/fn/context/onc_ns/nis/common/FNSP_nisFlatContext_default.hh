/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_NISFLATCONTEXT_DEFAULT_HH
#define	_FNSP_NISFLATCONTEXT_DEFAULT_HH

#pragma ident	"@(#)FNSP_nisFlatContext_default.hh	1.2	96/06/19 SMI"

#include <FNSP_defaultContext.hh>
#include "FNSP_nisAddress.hh"

class FNSP_nisFlatContext_default : public FNSP_defaultContext {
    protected:
	FNSP_nisAddress * my_address;  /* decoded */

	// internal functions
	FNSP_nisFlatContext_default(const FN_ref_addr&, const FN_ref&);
	FN_ref *resolve(const FN_string&, FN_status_csvc&);
	virtual FN_ref *make_service_ref();
	virtual FN_ref *make_printername_ref();

    public:
#ifdef DEBUG
	// only used for testing
	FNSP_nisFlatContext_default(const FN_identifier *addr_type,
	    const FN_string&);
	FNSP_nisFlatContext_default(const FN_ref&);
#endif /* DEBUG */

	~FNSP_nisFlatContext_default();
	static FNSP_nisFlatContext_default* from_address(
	    const FN_ref_addr&, const FN_ref&, FN_status& stat);

	// implementations FN_ctx_csvc_strong
	FN_ref* c_lookup(const FN_string& n, unsigned int f, FN_status_csvc&);
	FN_namelist* c_list_names(const FN_string& name, FN_status_csvc&);
	FN_bindinglist* c_list_bindings(const FN_string& name, FN_status_csvc&);
	FN_attrset* c_get_syntax_attrs(const FN_string& name, FN_status_csvc&);

	FN_ref* c_lookup_nns(const FN_string& name,
				    unsigned int f, FN_status_csvc&);
	FN_namelist* c_list_names_nns(const FN_string& name, FN_status_csvc&);
	FN_bindinglist* c_list_bindings_nns(const FN_string&n, FN_status_csvc&);
};

#endif /* _FNSP_NISFLATCONTEXT_DEFAULT_HH */

/*
 * Copyright (c) 1992 - 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_USERNAMECONTEXT_HH
#define	_FNSP_USERNAMECONTEXT_HH

#pragma ident	"@(#)FNSP_UsernameContext.hh	1.5	97/08/15 SMI"

#include "FNSP_HUContext.hh"

class FNSP_UsernameContext : public FNSP_HUContext {
public:
	static FNSP_UsernameContext* from_address(const FN_ref_addr &,
	    const FN_ref &, unsigned int auth,
	    FN_status &stat);

	FNSP_UsernameContext(const FN_ref &, unsigned int auth = 0);

private:
	FNSP_UsernameContext(const FN_ref_addr&, const FN_ref&, unsigned int);

protected:
	// implementation for FNSP_HUContext virtual functions
	FNSP_Address *get_attribute_context(const FN_string &,
	    unsigned &status, unsigned int auth = 0);
	int check_for_config_error(const FN_string &, FN_status_csvc &);

	// operations on builtin attributes
	FN_attribute *builtin_attr_get(const FN_string &,
	    const FN_identifier &, FN_status_csvc &);
	FN_attrset *builtin_attr_get_all(const FN_string &,
	    FN_status_csvc &);
	FN_searchset *builtin_attr_search(const FN_attrset &,
	    FN_status_csvc &);
};

#endif	/* _FNSP_USERNAMECONTEXT_HH */

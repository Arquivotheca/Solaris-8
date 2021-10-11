/*
 * Copyright (c) 1992 - 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_HOSTNAMECONTEXT_HH
#define	_FNSP_HOSTNAMECONTEXT_HH

#pragma ident	"@(#)FNSP_HostnameContext.hh	1.5	97/08/15 SMI"

#include <netdb.h>
#include "FNSP_HUContext.hh"

class FNSP_HostnameContext : public FNSP_HUContext {
public:
	static FNSP_HostnameContext* from_address(const FN_ref_addr&,
	    const FN_ref&, unsigned int auth,
	    FN_status& stat);

	FNSP_HostnameContext(const FN_ref &, unsigned int auth = 0);

private:
	FNSP_HostnameContext(const FN_ref_addr&, const FN_ref&,
	    unsigned int auth);
	struct hostent *get_hostent(const FN_string &,
	    FN_status_csvc &);



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

#endif	/* _FNSP_HOSTNAMECONTEXT_HH */

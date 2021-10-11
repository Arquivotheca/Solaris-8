/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_NISPLUS_ADDRESS_HH
#define	_FNSP_NISPLUS_ADDRESS_HH

#pragma ident	"@(#)FNSP_nisplus_address.hh	1.1	96/03/31 SMI"

#include <FNSP_Address.hh>

class FNSP_nisplus_address : public FNSP_Address {
protected:
	void nisplus_init(unsigned int auth);

public:
	~FNSP_nisplus_address();

	FNSP_nisplus_address(const FN_ref_addr&, unsigned int auth);
	FNSP_nisplus_address(const FN_string&, unsigned ctx_type,
	    unsigned int repr = FNSP_normal_repr, unsigned int auth = 0);

	// used by some constructors in Org and Flat
	// probably only for testing
	FNSP_nisplus_address(const FN_ref&, unsigned int auth = 0);

};

#endif /* _FNSP_NISPLUS_ADDRESS_HH */

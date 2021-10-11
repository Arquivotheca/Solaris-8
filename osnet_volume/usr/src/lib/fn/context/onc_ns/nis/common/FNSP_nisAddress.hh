/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_NISADDRESS_HH
#define	_FNSP_NISADDRESS_HH

#pragma ident	"@(#)FNSP_nisAddress.hh	1.2	97/08/18 SMI"

#include <xfn/fn_p.hh>
#include <FNSP_Address.hh>

class FNSP_nisAddress : public FNSP_Address {
private:
	void nis_init();

public:
	FNSP_nisAddress(const FN_ref_addr&);
	FNSP_nisAddress(const FN_string&, unsigned ctx_type,
	    unsigned = FNSP_normal_repr);
	FNSP_nisAddress(const FNSP_nisAddress *);
	~FNSP_nisAddress();

	// used by some constructors in Org and Flat
	// probably only for testing
	FNSP_nisAddress(const FN_ref&);
};

#endif /* _FNSP_NISADDRESS_HH */

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_XDSDN_HH
#define	_XDSDN_HH

#pragma ident	"@(#)XDSDN.hh	1.1	96/03/31 SMI"


#include <xfn/xfn.h>
#include <xfn/fn_spi.hh>
#include "XDSInfo.hh"


extern "C" {

#include "xom.h"
#include "xds.h"
#include "xdsbdcp.h"
#include "xdsxfnp.h"

}


/*
 * XDS Distinguished Name
 */


class XDSDN : public XDSInfo
{
	OM_descriptor		*dn;
	int			delete_me;

public:

	// internal access
	OM_object	internal(void) { return (dn); };

	// string format
	unsigned char	*str(int is_dn = 0);

	XDSDN(const FN_string &name);
	XDSDN(OM_descriptor *name);
	virtual ~XDSDN();
};


#endif	/* _XDSDN_HH */

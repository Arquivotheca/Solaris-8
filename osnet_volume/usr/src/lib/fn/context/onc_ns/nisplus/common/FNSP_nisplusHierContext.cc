/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisplusHierContext.cc	1.10	96/03/31 SMI"

#include "FNSP_nisplusHierContext.hh"
#include "FNSP_nisplus_address.hh"
#include "FNSP_nisplusImpl.hh"

//  A FNSP_HierContext is derived from NS_ServiceContextAtomic.
//  A naming system composed of FNSP_HierContext supports a hierarchical
//  name space.  By the time processing gets to a FNSP_HierContext,
//  it only needs to deal  with 'atomic names'.  'nns pointers' may be
//  associated with each atomic  name.
//
//  The FNSP_HierContext itself may have an associatd nns pointer;
//  in this case, its binding could be found under the reserved name "FNS_nns"
//  in the context.  The bindings of names associated with the atomic names
//  are stored in the binding table of the atomic context.
//

FNSP_nisplusHierContext::~FNSP_nisplusHierContext()
{
	delete my_reference;
	delete ns_impl;
}


FNSP_nisplusHierContext::FNSP_nisplusHierContext(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth)
: FN_ctx_svc(auth)
{
	my_reference = new FN_ref(from_ref);
	ns_impl = new FNSP_nisplusImpl(
	    new FNSP_nisplus_address(from_addr, auth));
}

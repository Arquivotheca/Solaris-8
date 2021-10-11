/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisHierContext.cc	1.1	96/03/31 SMI"

#include "FNSP_nisHierContext.hh"
#include "FNSP_nisImpl.hh"

//  A FNSP_nisHierContext is derived from NS_ServiceContextAtomic.
//  A naming system composed of FNSP_nisHierContext supports a hierarchical
//  name space.  By the time processing gets to a FNSP_nisHierContext,
//  it only needs to deal  with 'atomic names'.  'nns pointers' may be
//  associated with each atomic  name.
//
//  The FNSP_nisHierContext itself may have an associatd nns pointer;
//  in this case, its binding could be found under the reserved name "FNS_nns"
//  in the context.  The bindings of names associated with the atomic names
//  are stored in the binding table of the atomic context.
//

FNSP_nisHierContext::~FNSP_nisHierContext()
{
	delete my_reference;
	delete ns_impl;
}


FNSP_nisHierContext::FNSP_nisHierContext(const FN_ref_addr &from_addr,
const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);

	ns_impl = new FNSP_nisImpl(new FNSP_nisAddress(from_addr));
}

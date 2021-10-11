/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_filesHierContext.cc	1.1	96/03/31 SMI"

#include "FNSP_filesHierContext.hh"
#include "FNSP_filesImpl.hh"

//  A FNSP_filesHierContext is derived from NS_ServiceContextAtomic.
//  A naming system composed of FNSP_filesHierContext supports a hierarchical
//  name space.  By the time processing gets to a FNSP_filesHierContext,
//  it only needs to deal  with 'atomic names'.  'nns pointers' may be
//  associated with each atomic  name.
//
//  The FNSP_filesHierContext itself may have an associatd nns pointer;
//  in this case, its binding could be found under the reserved name "FNS_nns"
//  in the context.  The bindings of names associated with the atomic names
//  are stored in the binding table of the atomic context.
//

FNSP_filesHierContext::~FNSP_filesHierContext()
{
	delete my_reference;
	delete ns_impl;
}


FNSP_filesHierContext::FNSP_filesHierContext(const FN_ref_addr &from_addr,
    const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);
	ns_impl = new FNSP_filesImpl(new FNSP_nisAddress(from_addr));
}

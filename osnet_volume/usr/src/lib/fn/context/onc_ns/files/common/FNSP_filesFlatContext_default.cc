/*
 * Copyright (c) 1994-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_filesFlatContext_default.cc	1.1	96/03/31 SMI"

#include "FNSP_filesFlatContext_default.hh"
#include "FNSP_filesImpl.hh"

//  A flat naming system is one in which only non-hierarchical names are bound.
//  All names in the naming system are bound in a single context.
//  The only thing that can be bound under a flat name is a nns pointer.
//  This implementation supports junctions.  This means
//  that 'a' and 'a/' resolve to the same thing.
//  This implementation is for files and nis.
//  All bindings are generated algorithmically.  No storage is acctually
//  consulted.
//
//  This is used for the nsid context of org and the service context.
//  The following are the currently supported bindings.  In the future,
//  others (like username and hostname) might also be supported.
//  In an org's nsid context, the following names are bound:
//    service, _service
//  In the service context:
//    printer
//

FNSP_filesFlatContext_default::~FNSP_filesFlatContext_default()
{
}

#ifdef DEBUG
FNSP_filesFlatContext_default::FNSP_filesFlatContext_default(
    const FN_identifier *addr_type, const FN_string &dirname)
: FNSP_nisFlatContext_default(addr_type, dirname)
{
}

FNSP_filesFlatContext_default::FNSP_filesFlatContext_default(
    const FN_ref &from_ref) : FNSP_nisFlatContext_default(from_ref)
{
}
#endif

FNSP_filesFlatContext_default::FNSP_filesFlatContext_default(
    const FN_ref_addr &from_addr, const FN_ref& from_ref)
: FNSP_nisFlatContext_default(from_addr, from_ref)
{
}

FNSP_filesFlatContext_default*
FNSP_filesFlatContext_default::from_address(
    const FN_ref_addr &from_addr, const FN_ref& from_ref, FN_status& stat)
{
	FNSP_filesFlatContext_default *answer = new
	    FNSP_filesFlatContext_default(from_addr, from_ref);

	if (answer && answer->my_reference && answer->my_address)
		stat.set_success();
	else {
		if (answer) {
			delete answer;
			answer = 0;
		}
		stat.set(FN_E_INSUFFICIENT_RESOURCES);
	}
	return (answer);
}

FN_ref *
FNSP_filesFlatContext_default::make_service_ref()
{
	return (FNSP_reference(FNSP_files_address_type_name(),
	    my_address->get_internal_name(), FNSP_service_context));
}


static const FN_string
FNSP_printername_files_address_contents((unsigned char *) "printers");

FN_ref *
FNSP_filesFlatContext_default::make_printername_ref()
{
	return (FNSP_reference(FNSP_printer_files_address_type_name(),
	    *FNSP_reftype_from_ctxtype(FNSP_printername_context),
	    FNSP_printername_files_address_contents,
	    FNSP_printername_context,
	    FNSP_normal_repr));
}

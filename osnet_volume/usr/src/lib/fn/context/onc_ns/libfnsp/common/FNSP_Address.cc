/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_Address.cc	1.9	97/08/18 SMI"

#include "FNSP_Address.hh"
#include <string.h>

FNSP_Address::~FNSP_Address()
{
}

void
FNSP_Address::init(const FN_string &contents, unsigned int /* auth */)
{
	internal_name = contents;

	switch (ctx_type) {
	case FNSP_organization_context:
		impl_type = FNSP_directory_impl;
		table_name = internal_name;
		break;
	case FNSP_hostname_context:
	case FNSP_username_context:
	case FNSP_enterprise_context:
		impl_type = FNSP_single_table_impl;
		table_name = internal_name;
		break;
	case FNSP_generic_context:
	case FNSP_service_context:
	case FNSP_nsid_context:
	case FNSP_user_context:
	case FNSP_host_context:
	case FNSP_site_context:
	case FNSP_printername_context:
	case FNSP_printer_object:
		table_name = internal_name;
		impl_type = FNSP_shared_table_impl;
		break;
	default:
		impl_type = FNSP_null_impl;  // should be error;
		return;
	}

	access_flags = 0;
}

FNSP_Address::FNSP_Address(const FN_string &contents,
    unsigned context_type,
    unsigned r_type,
    unsigned int authoritative)
{
	ctx_type = context_type;
	repr_type = r_type;

	init(contents, authoritative);
}

// For address type of onc_fn_printer_ have to check the
// ctx_type and set it properly
static const char *FNSP_printer_addr_type = "onc_fn_printer_";

FNSP_Address::FNSP_Address(const FN_ref_addr &addr, unsigned int auth)
{
	FN_string* iname = FNSP_address_to_internal_name(addr,
	    &ctx_type,
	    &repr_type);

	// Check if the address type is of printer type
	const FN_identifier *id = addr.type();
	if (strncmp((char *) id->str(), FNSP_printer_addr_type,
	    strlen(FNSP_printer_addr_type)) == 0) {
		if (ctx_type == 1)
			ctx_type = FNSP_printername_context;
		else if (ctx_type == 2)
			ctx_type = FNSP_printer_object;
	}

	if (iname) {
		init(*iname, auth);
		delete iname;
	} else {
		ctx_type = repr_type = 0;
		impl_type = FNSP_single_table_impl;
	}
}

// Check ref type for either printername context or printer object
// If so, make sure the ctx_type is set properly

FNSP_Address::FNSP_Address(const FN_ref &ref, unsigned int auth)
{
	FN_string *iname = FNSP_reference_to_internal_name(ref,
	    &ctx_type,
	    &repr_type);

	// Check the ref type for printername context or object
	const FN_identifier *id = ref.type();
	if ((*id) ==
	    (*FNSP_reftype_from_ctxtype(FNSP_printername_context)))
		ctx_type = FNSP_printername_context;
	else if ((*id) ==
	    (*FNSP_reftype_from_ctxtype(FNSP_printer_object)))
		ctx_type = FNSP_printer_object;

	if (iname) {
		init(*iname, auth);
		delete iname;
	} else {
		ctx_type = repr_type = 0;
		impl_type = FNSP_single_table_impl;
	}
}

FNSP_Address::FNSP_Address(const FNSP_Address *address)
{
	if (!address)
		return;
	ctx_type = address->ctx_type;
	repr_type = address->repr_type;
	impl_type = address->impl_type;
	access_flags = address->access_flags;
	internal_name = address->internal_name;
	index_name = address->index_name;
	table_name = address->table_name;
}

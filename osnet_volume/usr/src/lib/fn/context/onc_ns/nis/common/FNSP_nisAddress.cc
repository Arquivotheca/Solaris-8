/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisAddress.cc	1.2	97/08/18 SMI"

#include "FNSP_nisAddress.hh"

extern int
FNSP_decompose_nis_index_name(const FN_string &src,
    FN_string &tabname, FN_string &indexname);

void
FNSP_nisAddress::nis_init()
{
	switch (ctx_type) {
	case FNSP_organization_context:
		impl_type = FNSP_directory_impl;
		break;
	case FNSP_hostname_context:
	case FNSP_username_context:
		FNSP_decompose_nis_index_name(internal_name,
		    table_name, index_name);
		impl_type = FNSP_single_table_impl;
		break;
	case FNSP_enterprise_context:
	case FNSP_generic_context:
	case FNSP_service_context:
	case FNSP_nsid_context:
	case FNSP_user_context:
	case FNSP_host_context:
	case FNSP_site_context:
	case FNSP_printername_context:
	case FNSP_printer_object:
		FNSP_decompose_nis_index_name(internal_name,
		    table_name, index_name);
		impl_type = FNSP_shared_table_impl;
		break;
	default:
		impl_type = FNSP_null_impl;  // should be error;
		return;
	}

	access_flags = 0;
}

FNSP_nisAddress::FNSP_nisAddress(const FN_string &contents,
    unsigned context_type,
    unsigned r_type)
: FNSP_Address(contents, context_type, r_type)
{
	nis_init();
}


FNSP_nisAddress::FNSP_nisAddress(const FN_ref_addr &addr)
: FNSP_Address(addr, 0)
{
	if (ctx_type == 0)
		impl_type = FNSP_single_table_impl;
	else
		nis_init();
}

FNSP_nisAddress::FNSP_nisAddress(const FN_ref &ref)
:FNSP_Address(ref)
{
	if (ctx_type == 0)
		impl_type = FNSP_single_table_impl;
	else
		nis_init();
}

FNSP_nisAddress::~FNSP_nisAddress()
{
}

FNSP_nisAddress::FNSP_nisAddress(const FNSP_nisAddress *address)
:FNSP_Address(address)
{
}

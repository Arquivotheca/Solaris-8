/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_enterprise_nis.cc	1.3	96/03/31 SMI"

#include <xfn/xfn.hh>
#include "FNSP_enterprise_nis.hh"

#define	NIS_ADDRESS_STR "onc_fn_nis"
static const FN_identifier
my_addr_type_str((const unsigned char *)NIS_ADDRESS_STR);

FNSP_enterprise_nis::FNSP_enterprise_nis(const FN_string &domain)
{
	root_directory = new FN_string(domain);
}

FNSP_enterprise_nis::~FNSP_enterprise_nis()
{
	delete root_directory;
}

const FN_identifier*
FNSP_enterprise_nis::get_addr_type()
{
	return (&my_addr_type_str);
}

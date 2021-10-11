/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_enterprise_files.cc	1.4	96/03/31 SMI"

#include <xfn/xfn.hh>
#include "FNSP_enterprise_files.hh"

#define	FILES_ADDRESS_STR "onc_fn_files"
static const FN_identifier
my_addr_type_str((const unsigned char *)FILES_ADDRESS_STR);

static const FN_string empty_name((unsigned char *) "");

const FN_identifier*
FNSP_enterprise_files::get_addr_type()
{
	return (&my_addr_type_str);
}


FNSP_enterprise_files::FNSP_enterprise_files()
{
	root_directory = new FN_string(empty_name);
}

FNSP_enterprise_files::~FNSP_enterprise_files()
{
	delete root_directory;
}

/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_ENTERPRISE_NISPLUS_HH
#define	_FNSP_ENTERPRISE_NISPLUS_HH

#pragma ident	"@(#)FNSP_enterprise_nisplus.hh	1.7	96/03/31 SMI"

#include <xfn/xfn.hh>
#include "FNSP_enterprise.hh"

#define	NISPLUS_ADDRESS_STR	"onc_fn_nisplus"

class FNSP_enterprise_nisplus: public FNSP_enterprise
{
private:
	const FN_identifier *my_address_type;

public:
	FNSP_enterprise_nisplus();

	FNSP_enterprise_user_info *init_user_info(uid_t);

	const FN_string *get_root_orgunit_name();

	FN_string *get_user_orgunit_name(uid_t,
	    const FNSP_enterprise_user_info*, FN_string ** = NULL);

	FN_string* get_user_name(uid_t, const FNSP_enterprise_user_info *);

	FN_string* get_host_orgunit_name(FN_string ** = NULL);

	FN_string* get_host_name();

	const FN_identifier *get_addr_type();
};

#endif /* _FNSP_ENTERPRISE_NISPLUS_HH */

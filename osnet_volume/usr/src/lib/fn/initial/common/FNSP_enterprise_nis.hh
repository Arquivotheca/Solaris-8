/*
 * Copyright (c) 1994-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_ENTERPRISE_NIS_HH
#define	_FNSP_ENTERPRISE_NIS_HH

#pragma ident	"@(#)FNSP_enterprise_nis.hh	1.3	96/03/31 SMI"

#include <xfn/xfn.hh>
#include "FNSP_enterprise_files.hh"

class FNSP_enterprise_nis: public FNSP_enterprise
{
public:
	FNSP_enterprise_nis(const FN_string& domain);
	virtual ~FNSP_enterprise_nis();

	const FN_identifier* get_addr_type();
};

#endif /* _FNSP_ENTERPRISE_NIS_HH */

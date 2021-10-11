/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_ENTERPRISE_FILES_HH
#define	_FNSP_ENTERPRISE_FILES_HH

#pragma ident	"@(#)FNSP_enterprise_files.hh	1.3	96/03/31 SMI"

#include <xfn/xfn.hh>
#include "FNSP_enterprise.hh"

class FNSP_enterprise_files: public FNSP_enterprise
{
public:
	FNSP_enterprise_files();
	virtual ~FNSP_enterprise_files();

	const FN_identifier* get_addr_type();
};

#endif /* _FNSP_ENTERPRISE_FILES_HH */

/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_NISWEAKSLASHCONTEXT_HH
#define	_FNSP_NISWEAKSLASHCONTEXT_HH

#pragma ident	"@(#)FNSP_nisWeakSlashContext.hh	1.1	96/03/31 SMI"

#include <FNSP_WeakSlashContext.hh>

/* For:  service context */
class FNSP_nisWeakSlashContext : public FNSP_WeakSlashContext {
private:
	FNSP_nisWeakSlashContext(const FN_ref_addr &, const FN_ref &);
public:
	~FNSP_nisWeakSlashContext();
	static FNSP_nisWeakSlashContext* from_address(const FN_ref_addr &,
	    const FN_ref &,
	    FN_status &stat);
};

#endif	/* _FNSP_NISWEAKSLASHCONTEXT_HH */

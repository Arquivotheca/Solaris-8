/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_FILESWEAKSLASHCONTEXT_HH
#define	_FNSP_FILESWEAKSLASHCONTEXT_HH

#pragma ident	"@(#)FNSP_filesWeakSlashContext.hh	1.1	96/03/31 SMI"

#include <FNSP_WeakSlashContext.hh>

/* For:  service context */
class FNSP_filesWeakSlashContext : public FNSP_WeakSlashContext {
private:
	FNSP_filesWeakSlashContext(const FN_ref_addr &, const FN_ref &);
public:
	~FNSP_filesWeakSlashContext();
	static FNSP_filesWeakSlashContext* from_address(const FN_ref_addr &,
	    const FN_ref &,
	    FN_status &stat);
};

#endif	/* _FNSP_FILESWEAKSLASHCONTEXT_HH */

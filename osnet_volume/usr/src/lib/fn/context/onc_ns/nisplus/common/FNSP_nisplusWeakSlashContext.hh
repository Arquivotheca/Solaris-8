/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_NISPLUSWEAKSLASHCONTEXT_HH
#define	_FNSP_NISPLUSWEAKSLASHCONTEXT_HH

#pragma ident	"@(#)FNSP_nisplusWeakSlashContext.hh	1.9	96/03/31 SMI"

#include <xfn/fn_spi.hh>
#include <FNSP_WeakSlashContext.hh>

/* For:  service context */
class FNSP_nisplusWeakSlashContext : public FNSP_WeakSlashContext {
	FNSP_nisplusWeakSlashContext(const FN_ref_addr &, const FN_ref &,
	    unsigned int auth);
public:
	~FNSP_nisplusWeakSlashContext();
	static FNSP_nisplusWeakSlashContext* from_address(const FN_ref_addr &,
	    const FN_ref &, unsigned int auth,
	    FN_status &stat);
};

#endif	/* _FNSP_NISPLUSWEAKSLASHCONTEXT_HH */

/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_FNSP_NISPLUSPRINTERNAMECONTEXT_HH
#define	_FNSP_NISPLUSPRINTERNAMECONTEXT_HH

#pragma ident	"@(#)FNSP_nisplusPrinternameContext.hh	1.1	96/03/31 SMI"

#include <FNSP_PrinternameContext.hh>

class FNSP_nisplusPrinternameContext : public FNSP_PrinternameContext {
	FNSP_nisplusPrinternameContext(const FN_ref_addr&,
	    const FN_ref&,
	    unsigned int auth);

public:
	~FNSP_nisplusPrinternameContext();

	static FNSP_nisplusPrinternameContext* from_address(
	    const FN_ref_addr&,
	    const FN_ref&, unsigned int auth, FN_status &stat);
};

#endif	/* _FNSP_NISPLUSPRINTERNAMECONTEXT_HH */

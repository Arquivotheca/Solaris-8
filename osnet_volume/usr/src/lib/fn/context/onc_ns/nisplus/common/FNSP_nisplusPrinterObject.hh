/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_FNSP_NISPLUSPRINTEROBJECT_HH
#define	_FNSP_NISPLUSPRINTEROBJECT_HH

#pragma ident	"@(#)FNSP_nisplusPrinterObject.hh	1.1	96/03/31 SMI"

#include <FNSP_PrinterObject.hh>

class FNSP_nisplusPrinterObject : public FNSP_PrinterObject {
public:
	~FNSP_nisplusPrinterObject();

	static FNSP_nisplusPrinterObject* from_address(
	    const FN_ref_addr&, const FN_ref&, unsigned int auth,
	    FN_status& stat);

protected:
	// internal functions
	FNSP_nisplusPrinterObject(const FN_ref_addr&, const FN_ref&,
	    unsigned int auth);
};

#endif /* _FNSP_NISPLUSPRINTEROBJECT_HH */

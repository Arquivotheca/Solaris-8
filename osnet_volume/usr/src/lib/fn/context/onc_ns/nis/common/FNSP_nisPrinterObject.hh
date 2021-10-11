/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_FNSP_NISPRINTEROBJECT_HH
#define	_FNSP_NISPRINTEROBJECT_HH

#pragma ident	"@(#)FNSP_nisPrinterObject.hh	1.1	96/03/31 SMI"

#include <FNSP_PrinterObject.hh>

class FNSP_nisPrinterObject : public FNSP_PrinterObject {
protected:
	// internal functions
	FNSP_nisPrinterObject(const FN_ref_addr&, const FN_ref&);
public:
	~FNSP_nisPrinterObject();

	static FNSP_nisPrinterObject* from_address(const FN_ref_addr&,
	    const FN_ref&, FN_status& stat);

#ifdef DEBUG
	// probably not used (only for testing)
	FNSP_nisPrinterObject(const FN_string&);
	FNSP_nisPrinterObject(const FN_ref&);
#endif
};

#endif /* _FNSP_NISPRINTEROBJECT_HH */

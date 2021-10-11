/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_FNSP_FILESPRINTEROBJECT_HH
#define	_FNSP_FILESPRINTEROBJECT_HH

#pragma ident	"@(#)FNSP_filesPrinterObject.hh	1.1	96/03/31 SMI"


#include <FNSP_PrinterObject.hh>

class FNSP_filesPrinterObject : public FNSP_PrinterObject {
protected:
	FNSP_filesPrinterObject(const FN_ref_addr&, const FN_ref&);
public:
	~FNSP_filesPrinterObject();

	static FNSP_filesPrinterObject* from_address(const FN_ref_addr&,
	    const FN_ref&, FN_status& stat);

#ifdef DEBUG
	// probably not used (only for testing)
	FNSP_filesPrinterObject(const FN_ref&);
#endif
};

#endif /* _FNSP_FILESPRINTEROBJECT_HH */

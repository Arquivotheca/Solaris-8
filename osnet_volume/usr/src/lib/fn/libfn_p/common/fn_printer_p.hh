/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _FNSP_PRINTER_HH
#define	_FNSP_PRINTER_HH

#pragma ident "@(#)fn_printer_p.hh	1.3 94/11/08 SMI"

#include <xfn/xfn.hh>

/* context types */

enum FNSP_printer_context_types {
	FNSP_printername_context = 1,
	FNSP_printer_object = 2,
	FNSP_printername_context_nis = 3,
	FNSP_printername_context_files = 4
};

#endif // _FNSP_PRINTER_HH

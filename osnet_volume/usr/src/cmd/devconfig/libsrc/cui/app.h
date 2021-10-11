#pragma ident "@(#)app.h   1.3     92/11/25 SMI"

/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved. Sun
 * considers its source code as an unpublished, proprietary trade secret, and
 * it is available only under strict license provisions.  This copyright
 * notice is placed here only to protect Sun in the event the source is
 * deemed a published work.  Dissassembly, decompilation, or other means of
 * reducing the object code to human readable form is prohibited by the
 * license agreement under which this code is provided to the user or company
 * in possession of this copy.
 * 
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the Government
 * is subject to restrictions as set forth in subparagraph (c)(1)(ii) of the
 * Rights in Technical Data and Computer Software clause at DFARS 52.227-7013
 * and in similar clauses in the FAR and NASA FAR Supplement.
 */

//============================== -*-Mode: c++;-*- =============================
//	Application class definition
//		(Derived from Widget so we can use resource logic, although it
//		doesn't have much else in common.  All initialization and cleanup
//		logic is handled in constructor/destructor.)
//
//   ** No resource logic is used, it gets from widget the name string
//      and flags so it can set "CUI_REALIZED", that all! **
//
//	$RCSfile: app.h $ $Revision: 1.4 $ $Date: 1992/09/12 15:31:54 $
//=============================================================================


#ifndef _CUI_APPLICATION_H
#define _CUI_APPLICATION_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_SYMTAB_H
#include "symtab.h"
#endif
#ifndef _CUI_STRTAB_H
#include "strtab.h"
#endif
#ifndef _CUI_RESTAB_H
#include "restab.h"
#endif

#endif // PRE_COMPILED_HEADERS


//
// globals
//

extern Symtab	   *CUI_Symtab; 	// system Symtab
extern ResourceTable *CUI_Restab;	// global ResourceTable



class Application
{
	private:

		static int	count;
		char*		name;

        int initCurses(void);
		int exitCurses(void);

    public:

		// constructor, destructor, realization method

		Application(char* Name);
		virtual ~Application(void);

		// access methods

		virtual char* typeName(void)	{ return("Application"); }
		char* getName(void) 			{ return(name); 		 }
};

#endif /* _CUI_APPLICATION_H */


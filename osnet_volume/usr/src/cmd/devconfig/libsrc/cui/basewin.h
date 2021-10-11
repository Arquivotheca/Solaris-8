#pragma ident "@(#)basewin.h   1.3     92/11/25 SMI"

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
//	BaseWindow class definition
//
//	$RCSfile: basewin.h $ $Revision: 1.8 $ $Date: 1992/09/25 00:41:29 $
//=============================================================================


#ifndef _CUI_BASEWIN_H
#define _CUI_BASEWIN_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_SHELL_H
#include "shell.h"
#endif
#ifndef _CUI_APP_H
#include "app.h"
#endif

#endif // PRE_COMPILED_HEADERS


class BaseWindow : public Shell
{
	protected:

		Application *application;

        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);

	public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		BaseWindow(char *Name, Widget * = NULL,
				   CUI_Resource * = NULL, CUI_WidgetId id = CUI_BASEWIN_ID);
        virtual ~BaseWindow(void);

		// CUI_initialize() and CUI_exit() call these methods

		void		appInit(Application *);
		Application *getApp(void)		{ return(application);				}

        // utility routines

        virtual int isKindOf(CUI_WidgetId type)
				{ return(type == CUI_BASEWIN_ID || Shell::isKindOf(type));	}
		virtual char *typeName(void)		   { return Widget::typeName(); }


		// make sure we ignore 'cancel' message (from CANCEL key)

		virtual int cancel()  { return(0); }
};

#endif // _CUI_BASEWIN_H

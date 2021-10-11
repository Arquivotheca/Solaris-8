#pragma ident "@(#)popwin.h   1.3     92/11/25 SMI"

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
//	PopupWindow class definition
//
//	$RCSfile: popwin.h $ $Revision: 1.4 $ $Date: 1992/09/12 15:17:55 $
//=============================================================================

#ifndef _CUI_POPWIN_H
#define _CUI_POPWIN_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_SHELL_H
#include "shell.h"
#endif

#endif // PRE_COMPILED_HEADERS


class PopupWindow : public Shell
{
	protected:

		bool	poppedUp;

        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);

	public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		PopupWindow(char *Name, Widget * = NULL, CUI_Resource * = NULL,
					CUI_WidgetId id = CUI_POPWIN_ID);
        virtual ~PopupWindow(void)              { /* nothing */             }

        // utility routines

		virtual int  popUp(CUI_GrabMode mode = CUI_GRAB_EXCLUSIVE);
		virtual void popDown(void);
		virtual int  isKindOf(CUI_WidgetId type)
				 { return(type == CUI_POPWIN_ID || Shell::isKindOf(type));	}
		virtual char *typeName(void)		   { return Widget::typeName(); }
		bool	isPoppedUp(void)			   { return(poppedUp);			}
};

#endif // _CUI_POPWIN_H


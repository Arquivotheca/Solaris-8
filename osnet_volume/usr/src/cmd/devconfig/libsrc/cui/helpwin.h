#pragma ident "@(#)helpwin.h   1.3     92/11/25 SMI"

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
//	HelpWindow class definition
//
//	$RCSfile: helpwin.h $ $Revision: 1.13 $ $Date: 1992/09/12 15:31:28 $
//=============================================================================


#ifndef _CUI_HELPWIN_H
#define _CUI_HELPWIN_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_POPWIN_H
#include "popwin.h"
#endif

#endif // PRE_COMPILED_HEADERS


#define HELP_INDEX_KEY "!INDEX"


class HelpWindow : public PopupWindow
{
	protected:

		short  normalColor; 	// color of inactive buttons
		short  activeColor; 	// color of current link/button
		short  setColor;		// color of links at rest

        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);

	public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		HelpWindow(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				   CUI_WidgetId = CUI_HELPWIN_ID);
		virtual int realize(void);
        virtual ~HelpWindow(void)           { /* nothing */                 }

        // utility routines

		virtual int  isKindOf(CUI_WidgetId type)
		  { return(type == CUI_HELPWIN_ID || PopupWindow::isKindOf(type));	}
		virtual char *typeName(void)		   { return Widget::typeName(); }
        virtual short &normalAttrib()          { return(normalColor);       }
		virtual short &activeAttrib()		   { return(activeColor);		}
		virtual short &setAttrib()			   { return(setColor);			}
		int  showHelp(char *topic);
		int  showPrevHelp(void);
		void focusPanel(bool showFirst);
		int  exitHelp(void);
};

#endif // _CUI_HELPWIN_H


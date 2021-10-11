#pragma ident "@(#)mitem.h   1.4     92/11/25 SMI"

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
//	MenuItem class definition
//
//	$RCSfile: mitem.h $ $Revision: 1.10 $ $Date: 1992/09/12 15:19:22 $
//=============================================================================

#ifndef _CUI_MITEM_H
#define _CUI_MITEM_H

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_ITEM_H
#include "item.h"
#endif

#endif // PRE_COMPILED_HEADERS


class MenuItem : public Item
{
    protected:

		Menu		*subMenu;
		bool		poppedUp;

        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		MenuItem(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				 CUI_WidgetId id = CUI_MITEM_ID);
        virtual int realize(void);
		virtual ~MenuItem(void);

        // utility routines

		virtual int isKindOf(CUI_WidgetId type)
					{ return(type == CUI_MITEM_ID || Item::isKindOf(type)); }
		virtual int addChild(Widget * child)
									{ return(defineSubmenu((Menu *)child)); }
        int         defineSubmenu(Menu *);
        int         fixupArrow(bool current);

		// message-handlers

		virtual int doKey(int, Widget *);
		virtual bool filterKey(CUI_KeyFilter filter, int key);
        virtual int show(void)          { return(fixupArrow(isCurrent()));  }
		virtual int hide(void)			{ return(fixupArrow(isCurrent()));	}
		virtual int select(void);
		virtual int focus(bool = TRUE)
				{ doCallback(CUI_FOCUS_CALLBACK); return(fixupArrow(TRUE)); }
		virtual int unfocus(void)
			 { doCallback(CUI_UNFOCUS_CALLBACK); return(fixupArrow(FALSE)); }
        virtual int cancel(void);
};
#define NULL_MITEM (MenuItem *)0


#endif // _CUI_MITEM_H


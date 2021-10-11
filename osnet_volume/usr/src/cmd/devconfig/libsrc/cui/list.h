#pragma ident "@(#)list.h 1.7 93/03/15 SMI"

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
//	ScrollingList class definition
//
//	$RCSfile: list.h $ $Revision: 1.16 $ $Date: 1992/09/12 15:28:42 $
//=============================================================================


#ifndef _CUI_LIST_H
#define _CUI_LIST_H

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_VCONTROL_H
#include "vcontrol.h"
#endif
#ifndef  _CUI_MENU_H
#include "cuimenu.h"
#endif

#endif // PRE_COMPILED_HEADERS

class ListItem;


class ScrollingList : public Menu
{
	private:

    protected:

		Widget**		children;
		short			childArraySize;
		short			numChildren;
		VirtualControl	*control;			// our VirtualControl
		Menu			*popupMenu; 		// Menu to popup on Item select

		virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);
				int processSavedChildren(void);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		ScrollingList(char *Name, Widget * = NULL, CUI_Resource * = NULL,
					  CUI_WidgetId id = CUI_LIST_ID);
        virtual int realize(void);
		virtual ~ScrollingList(void);

        // utility routines

		virtual int isKindOf(CUI_WidgetId type)
		{
			return(type == CUI_LIST_ID || Menu::isKindOf(type));
		}
		virtual int addChild(Widget *);
		virtual int manageGeometry(void);
		virtual bool isCurrent(void)
			{ return(control && control->isCurrent()); }
		virtual int setDefault(void)
			{ return control && control->setDefault(); }
		virtual int addItem(ITEM *item, int index = INT_MAX);

		// message-handlers

        virtual int show(void);
		virtual int focus(bool reset = TRUE);
		virtual int unfocus(void);
		virtual int cancel(void)		{ return(0);						}
		virtual int done(void)			{ return(0);						}
        virtual int interpret(char *c)  { badCommand(c); return(-1);        }
};
#define NULL_LIST (ScrollingList *)0


#endif // _CUI_LIST_H


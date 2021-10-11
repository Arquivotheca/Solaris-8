#pragma ident "@(#)item.h   1.5     93/01/08 SMI" 

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
//	Item class definition (abstract class - base for MenuItem and ListItem)
//
//	$RCSfile: item.h $ $Revision: 1.2 $ $Date: 1992/12/30 21:12:47 $
//=============================================================================

#ifndef _CUI_ITEM_H
#define _CUI_ITEM_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_SYSCURSES_H
#include "syscurses.h"
#endif
#ifndef _CUI_WIDGET_H
#include "widget.h"
#endif

#endif // PRE_COMPILED_HEADERS


class Menu; 	// forward declaration

class Item : public Widget
{
    protected:

		bool	set;  // is value set?
		char	*label;
		ITEM	*item;

        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		Item(char *Name, Widget * = NULL, CUI_Resource * = NULL,
			 CUI_WidgetId id = CUI_ITEM_ID);
		virtual int realize(void);
		virtual ~Item(void);

		// access methods

		MENU	*ETImenu(void)		  { return(item ? item->imenu : NULL);	}
		ITEM	*ETIitem(void)		  { return(item);						}
		Menu	*getMenu(void);
		bool	isSet(void) 		  { return(set);						}
		char	*getLabel(void) 	  { return(label);						}

        // utility routines

		virtual bool isCurrent(void);
		virtual int  isKindOf(CUI_WidgetId type)
											{ return(type == CUI_ITEM_ID);	}
		virtual int addChild(Widget *)		{ return(-1);					}
		virtual int alignLabel(short Col);
		virtual int labelLength(void)  { return(label ? strlen(label) : 0); }
		virtual void setOn(bool);
        int         redraw(bool focus);

		// message-handlers

		virtual int doKey(int, Widget *) { return(-1);						}
		virtual bool filterKey(CUI_KeyFilter filter, int key)
			{ return(filter(this, key)); }
        virtual int show(void)          { return(0);                        }
		virtual int hide(void)			{ return(0);						}
        virtual int select(void);
		virtual int unselect(void)		{ return(0);						}
		virtual int focus(bool reset = TRUE);
		virtual int unfocus(void);
		virtual int cancel(void)		{ return(0);						}
        virtual int done(void)          { return(0);                        }
		virtual int refresh(void)		{ return(0);						}

};
#define NULL_ITEM (Item *)0


#endif // _CUI_ITEM_H


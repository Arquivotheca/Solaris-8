#pragma ident "@(#)mbutton.h   1.4     92/11/25 SMI"

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
//	MenuButton class definition
//
//	$RCSfile: mbutton.h $ $Revision: 1.9 $ $Date: 1992/09/12 15:18:09 $
//=============================================================================

#ifndef _CUI_MBUTTON_H
#define _CUI_MBUTTON_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_BUTTON_H
#include "button.h"
#endif
#ifndef _CUI_MENU_H
#include "cuimenu.h"
#endif

#endif // PRE_COMPILED_HEADERS


class MenuButton : public OblongButton
{
	protected:

		Menu		*menuPtr;		// pointer to our menu
		bool		menuActive; 	// is menu active?

        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);
        virtual int setLabel(char *Label);
        int         popup(void);
		int 		fixupArrow(bool current);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		MenuButton(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				 CUI_WidgetId id = CUI_MENUBUTTON_ID);
        virtual int realize(void);
		virtual ~MenuButton(void);

		// access routines

		Menu * &menu(void)				{ return(menuPtr);					}
		virtual Window *getWindow(void)
			 { return( menuPtr && menuActive ? menuPtr->getWindow() : NULL); }

        // utility routines

        virtual int isKindOf(CUI_WidgetId type)
		{
			return(type == CUI_MENUBUTTON_ID || OblongButton::isKindOf(type));
		}
		virtual void locateCursor(void);

		// add Child

		virtual int addChild(Widget *);

        // message-handlers

		virtual int doKey(int key, Widget *from = NULL);
		virtual bool filterKey(CUI_KeyFilter filter, int key);
		virtual int show(void)			{ return(fixupArrow(isCurrent()));	}
		virtual int hide(void)			{ return(fixupArrow(isCurrent()));	}
		virtual int select(void)		{ return(0);						}
		virtual int unselect(void)		{ return(0);						}
		virtual int focus(bool reset = TRUE);
		virtual int unfocus(void);
		virtual int cancel(void)		{ menuActive = FALSE; return(0);	}
		virtual int done(void)			{ return(0);						}
        virtual int refresh(void);
};
#define NULL_MBUTTON (MenuButton *)0


#endif /* _CUI_MBUTTON_H */


#pragma ident "@(#)button.h   1.4     93/01/08 SMI"

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
//	Button, OblongButton class definitions
//
//	$RCSfile: button.h $ $Revision: 1.2 $ $Date: 1992/12/29 22:15:44 $
//=============================================================================


#ifndef _CUI_BUTTON_H
#define _CUI_BUTTON_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_CONTROL_H
#include "control.h"
#endif

#endif // PRE_COMPILED_HEADERS


//=============================================================================
//	Button class is abstract
//=============================================================================

class Button : public Control
{
	protected:

        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);
        virtual int setLabel(char *Label);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		Button(char *Name, Widget * = NULL, CUI_Resource * = NULL,
			   CUI_WidgetId = CUI_BUTTON_ID);
		virtual int realize(void);
		virtual ~Button(void);

        // utility routines

        virtual int isKindOf(CUI_WidgetId type)
				{ return(type == CUI_BUTTON_ID || Control::isKindOf(type)); }
		virtual void locateCursor(void);
		virtual Window *getWindow(void) 	{ return(0);					}

		// add Child

		virtual int addChild(Widget *)		{ return(0);					}

        // message-handlers

		virtual int doKey(int key, Widget *from = NULL);

};
#define NULL_BUTTON (Button *)0


//=============================================================================
//	OblongButton class
//=============================================================================

class OblongButton : public Button
{
    public:
		// constructor & destructor

		OblongButton(char *Name, Widget * = NULL, CUI_Resource * = NULL,
					 CUI_WidgetId = CUI_OBLONGBUTTON_ID);
        virtual ~OblongButton(void)         { /* nothing */                 }

        // utility routines

        virtual int isKindOf(CUI_WidgetId type)
		{ return(type == CUI_OBLONGBUTTON_ID || Button::isKindOf(type));	}
		virtual Window *getWindow(void) 	{ return(0);					}
};
#define NULL_OBLONGBUTTON (RectButton *)0


#endif // _CUI_BUTTON_H



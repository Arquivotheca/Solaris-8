#pragma ident "@(#)xbutton.h   1.5     93/01/08 SMI"

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
//	Exclusive/nonexclusive-type Buttons
//		(RectButton, CheckBox)
//
//	$RCSfile: xbutton.h $ $Revision: 1.2 $ $Date: 1992/12/30 21:12:47 $
//=============================================================================

#ifndef _CUI_XBUTTON_H
#define _CUI_XBUTTON_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_BUTTON_H
#include "button.h"
#endif

#endif // PRE_COMPILED_HEADERS


//=============================================================================
//	RectButton class
//=============================================================================

class RectButton : public Button
{
	protected:

		bool	isSet;		// is value set?
		short	setColor;	// color when set

        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);
        virtual int setLabel(char *Label);
		virtual int showFocus(bool current);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

        // constructor & destructor

		RectButton(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				   CUI_WidgetId id = CUI_RECTBUTTON_ID);
        virtual int realize(void);
		virtual ~RectButton(void)			{ /* nothing */ 				}

        // utility routines

        virtual int isKindOf(CUI_WidgetId type)
			 { return(type == CUI_RECTBUTTON_ID || Button::isKindOf(type)); }
        virtual short &setAttrib()          { return(setColor);             }

		// access routines

		bool &set(void) 					{ return(isSet);				}
		virtual void setOn(bool);
		virtual int toggle(void);

        // message-handlers

		virtual int doKey(int key, Widget *from = NULL);
		virtual int focus(bool = TRUE)	 { return(showFocus(TRUE)); 		}

		virtual int unfocus(void)		{ return(showFocus(FALSE)); 		}
};
#define NULL_RECTBUTTON (RectButton *)0


//=============================================================================
//	CheckBox class (same as RectButton, except it displays checkmark when set)
//=============================================================================

class CheckBox : public RectButton
{
	protected:

		virtual void setOn(bool);
		virtual int toggle(void);
		virtual int setLabel(char *Label);
        void        updateCheck(bool on);

    public:

		// constructor

		CheckBox(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				 CUI_WidgetId id = CUI_CHECKBOX_ID);
        virtual int realize(void);

        // utility routines

        virtual int isKindOf(CUI_WidgetId type)
		{
			return(type == CUI_CHECKBOX_ID || RectButton::isKindOf(type));
		}
};
#define NULL_CHECKBOX (CheckBox *)0


#endif // _CUI_XBUTTON_H



#pragma ident "@(#)text.h   1.4     93/01/08 SMI"

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
//	TextEdit, TextField, StaticText, and NumericField class definitions
//
//	$RCSfile: text.h $ $Revision: 1.2 $ $Date: 1992/12/29 21:44:27 $
//=============================================================================


#ifndef _CUI_TEXT_H
#define _CUI_TEXT_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_CONTROL_H
#include "control.h"
#endif

#endif // PRE_COMPILED_HEADERS


//=============================================================================
//	TextEdit is the base for all the others...
//=============================================================================

class TextEdit : public Control
{
	private:

    protected:

		char		*initialValue;

        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);
		virtual int translateKey(int key);
		static char *wrapText(char *text, int width);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		TextEdit(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				 CUI_WidgetId id = CUI_TEXTEDIT_ID);
		virtual int realize(void);
		virtual ~TextEdit(void);

		// access methods


        // utility routines

		virtual int isKindOf(CUI_WidgetId type)
		{
			return(type == CUI_TEXTEDIT_ID || Control::isKindOf(type));
		}
		virtual int manageGeometry(void);

		// message-handlers

		virtual int show(void)			{ return(0);						}
		virtual int hide(void)			{ return(0);						}
		virtual int select(void)		{ return(0);						}
		virtual int unselect(void)		{ return(0);						}
		virtual int cancel(void)		{ return(0);						}
		virtual int done(void)			{ return(0);						}
		virtual int refresh(void)		{ return(0);						}
        virtual int interpret(char *c)  { badCommand(c); return(-1);        }
		virtual int focus(bool reset = TRUE);
};
#define NULL_TEXTEDIT (TextEdit *)0


//=============================================================================
//	TextField is simply a one-line TextEdit
//=============================================================================

class TextField : public TextEdit
{
	private:

    protected:

        virtual int translateKey(int key);

    public:

		TextField(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				  CUI_WidgetId id = CUI_TEXTFIELD_ID);
        virtual int isKindOf(CUI_WidgetId type)
		{
			return(type == CUI_TEXTFIELD_ID || TextEdit::isKindOf(type));
		}
		virtual int manageGeometry(void)  { return(Control::manageGeometry()); }
		virtual int focus(bool reset = TRUE);
		virtual int unfocus(void);
};
#define NULL_TEXTFIELD (TextField *)0


//=============================================================================
//	StaticText is simply a non-editable TextEdit
//=============================================================================

class StaticText : public TextEdit
{
	private:

    protected:

		virtual int setValue(CUI_Resource *);

    public:

		StaticText(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				   CUI_WidgetId id = CUI_STATIC_TEXT_ID);
        virtual int isKindOf(CUI_WidgetId type)
		{
			return(type == CUI_STATIC_TEXT_ID || TextEdit::isKindOf(type));
		}
};
#define NULL_STATIC_TEXT (StaticText *)0


//=============================================================================
//	NumericField is a TextField that takes only numeric chars,
//	and that has up/down scroll arrows
//=============================================================================

class NumericField : public TextField
{
	private:

		long	minValue;
		long	maxValue;

    protected:

		virtual int getValue(CUI_Resource *);
        virtual int setValue(CUI_Resource *resource);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		NumericField(char *Name, Widget * = NULL, CUI_Resource * = NULL,
					 CUI_WidgetId id = CUI_NUMFIELD_ID);
        virtual int realize(void);
        virtual int isKindOf(CUI_WidgetId type)
		{
			return(type == CUI_NUMFIELD_ID || TextField::isKindOf(type));
		}
		virtual int manageGeometry(void)  { return(Control::manageGeometry()); }
};
#define NULL_NUMFIELD (NumField *)0


#endif // _CUI_TEXT_H



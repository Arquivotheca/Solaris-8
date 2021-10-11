#pragma ident "@(#)control.h   1.4     92/11/25 SMI"

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
//	Control class definition (maps to an ETI field)
//
//	$RCSfile: control.h $ $Revision: 1.20 $ $Date: 1992/09/12 15:17:48 $
//=============================================================================

#ifndef _CUI_CONTROL_H
#define _CUI_CONTROL_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_WIDGET_H
#include "widget.h"
#endif
#ifndef _CUI_SYSCURSES_H
#include "syscurses.h"
#endif

#endif // PRE_COMPILED_HEADERS

class ControlArea;	// don't need to include just for prototype


// field buffers

#define CUI_INIT_BUFF 0
#define CUI_EDIT_BUFF 1


class Control : public Widget
{
	private:

		static int	  insertMode;

    protected:

		char   *label;			// label
		int    rows;			// total number of rows
		FIELD  *field;			// ETI field
		short  normalColor; 	// color at rest
		short  activeColor; 	// color when current
		short  disabledColor;	// color when disabled

		virtual int  setValue(CUI_Resource *);
		virtual int  getValue(CUI_Resource *);
		virtual int  translateKey(int key);
		virtual int  setLabel(char *Label);
				int  doMetaKey(ControlArea *, int ,int);
		ControlArea  *getContainingControlArea(void);
		ControlArea  *getTopmostControlArea(void);
				int  validate(int request);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		Control(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				CUI_WidgetId = CUI_CONTROL_ID);
        virtual int realize(void);
		virtual ~Control(void);

		// access methods

		virtual short &normalAttrib()		{ return(normalColor);			}
		virtual short &activeAttrib()		{ return(activeColor);			}
		virtual short &disabledAttrib() 	{ return(disabledColor);		}
        virtual FORM *ETIform(void);
		FIELD	*ETIfield(void) 			{ return(field);				}
		virtual ControlArea *getControlArea(void);
		virtual Window *getWindow(void) 	{ return(0);					}

		// add Child

		virtual int addChild(Widget *)		{ return(0);					}

        // utility routines

		virtual bool isCurrent(void);
		virtual int  isKindOf(CUI_WidgetId type)
			{ return(type == CUI_CONTROL_ID); }
		virtual int  labelLength(void);
		virtual int  manageGeometry(void);
		virtual void locateCursor(void) 	{ pos_form_cursor(ETIform());  }
		virtual int  setDefault(void);
				void syncOptions(void);

		// message-handlers

		virtual int doKey(int key, Widget *from = NULL);
		virtual bool filterKey(CUI_KeyFilter filter, int key)
			{ return(filter(this, key)); }
		virtual int focus(bool reset = TRUE);
		virtual int unfocus(void);

};
#define NULL_CONTROL (Control *)0


#endif // _CUI_CONTROL_H



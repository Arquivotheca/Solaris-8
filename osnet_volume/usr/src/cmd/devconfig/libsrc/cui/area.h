#pragma ident "@(#)area.h   1.5     93/01/08 SMI" 

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
//	ControlArea class definition (a Composite with an associated ETI Form)
//
//	$RCSfile: area.h $ $Revision: 1.2 $ $Date: 1992/12/29 22:53:32 $
//=============================================================================

#ifndef _CUI_AREA_H
#define _CUI_AREA_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_COMPOSITE_H
#include "composite.h"
#endif
#ifndef _CUI_SYSCURSES_H
#include "syscurses.h"
#endif

#endif // PRE_COMPILED_HEADERS


class VirtualControl;	// forward-declare

class ControlArea : public Composite
{
	protected:

		char			*title; 		// title for our window
		Window			*window;		// window we're displayed in
		VirtualControl	*control;		// if we're contained in ControlArea...
		FIELD			**fields;		// array of ETI fields
		int 			fieldArraySize; // size of fields array (we can resize)
		int 			numFields;		// number of fields currently in array
		FORM			*form;			// the ETI form
		short			borderColor;	// color of our border (if any)
		short			titleColor; 	// color of our title (if any)
		short			interiorColor;	// color of our interior

        virtual int setValue(CUI_Resource *resource);
		virtual int getValue(CUI_Resource *);
        int         lookupWidget(Widget *widget);

        int         lookupField(FIELD *field);
		int 		initWindow(void);
		int 		freeWindow(void);
        int         removeField(FIELD *field);
		int 		connectFields(void);
        void        getDimensions(void);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		ControlArea(char *Name, Widget * = NULL, CUI_Resource * = NULL,
			  CUI_WidgetId id = CUI_CONTROL_AREA_ID);
        virtual int realize(void);
        virtual ~ControlArea(void);

        // access methods

		virtual Window *getWindow(void) 		  { return(window); 		}
		virtual ControlArea *getControlArea(void) { return(this);			}
		virtual FORM  *ETIform(void)			  { return(form);			}
		virtual FIELD *ETIfield(void);
        virtual short &borderAttrib()             { return(borderColor);    }
		virtual short &titleAttrib()			  { return(titleColor); 	}
		virtual short &interiorAttrib() 		  { return(interiorColor);	}

        // utility routines

		Widget *currentWidget(void)
		{
			return(form ? (Widget *)field_userptr(current_field(form)) : NULL);
		}
        virtual int isKindOf(CUI_WidgetId type)
		{
			return(type == CUI_CONTROL_AREA_ID || Composite::isKindOf(type));
		}
		int 		 addField(FIELD *field);
		virtual void adjustLocation(short &, short &)		 { /*nothing*/	}
		virtual int  locate(short Row, short Col);
		int unsetCurrent(void);
        virtual bool isCurrent(void);    // can't inline this!
		virtual void locateCursor(void);
        Widget *nextWidget(Widget *widget);
		Widget *previousWidget(Widget *widget);
		int    setCurrent(Widget *widget);
		virtual int setDefault(void);
		bool	isFirst(Widget *widget);
		bool	isLast(Widget *widget);
		bool	onFirstRow(Widget *widget);
		bool	onLastRow(Widget *widget);
		Widget	*getFirst(void);
		Widget	*getLast(void);
		void	fixupLast(int);

        // message-handlers

		virtual int doKey(int key, Widget *from = NULL);
		virtual bool filterKey(CUI_KeyFilter filter, int key);
        virtual int show(void);
		virtual int hide(void);
		virtual int select(void)		{ return(0);						}
		virtual int unselect(void)		{ return(0);						}
		virtual int focus(bool reset = TRUE);
		virtual int unfocus(void);
		virtual int cancel(void)		{ return(0);						}
		virtual int done(void)			{ return(0);						}
        virtual int refresh(void);
};

#endif // _CUI_AREA_H



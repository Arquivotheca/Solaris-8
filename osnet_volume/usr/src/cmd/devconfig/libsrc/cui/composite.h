#pragma ident "@(#)composite.h   1.6     92/12/07 SMI"

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
//	Composite class definition (a collection of Widgets)
//
//	$RCSfile: composite.h $ $Revision: 1.16 $ $Date: 1992/09/12 15:30:10 $
//=============================================================================

#ifndef _CUI_COMPOSITE_H
#define _CUI_COMPOSITE_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_WIDGET_H
#include "widget.h"
#endif

#endif // PRE_COMPILED_HEADERS


class Composite : public Widget
{
	protected:

		Widget** children;
		short	childArraySize;
		short	numChildren;
		byte	hSpace;
		byte	vSpace;
		byte	hPad;
		byte	vPad;

		virtual int  setValue(CUI_Resource *);
		virtual int  getValue(CUI_Resource *);
				void alignCaptions(void);
				void adjustHorizontal(void);
				void adjustVertical(void);

	public:

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		Composite(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				  CUI_WidgetId id = CUI_COMPOSITE_ID);
		virtual int realize(void);
		virtual ~Composite(void);

		// geometry-management

		virtual int manageGeometry(void);
		virtual int adjustChildren(void);
		virtual int setDimensions(void);

		// utility routines

		virtual int isKindOf(CUI_WidgetId type)
										{ return(type == CUI_COMPOSITE_ID); }
        virtual Window *getWindow(void)     { return(NULL);                 }
		virtual ControlArea *getControlArea(void);
		virtual int    addChild(Widget *);
		virtual int    labelLength(void)				{ return(-1);		}
		virtual int    alignLabel(short)				{ return(0);		}
		virtual void   locateCursor(void)				{ /* nothing */ 	}
		virtual bool   isCurrent(void)					{ return(FALSE);	}
		FIELD	*ETIfield(void) 						{ return(NULL); 	}
		virtual int  setDefault(void)					{ return(0);		}
				void adjustCenter(void);
				void adjustRight(void);

		// message-handlers

		virtual int doKey(int = 0, Widget * = NULL) 	{ return(0);		}
		virtual int show(void)			{ return(0);						}
		virtual int hide(void)			{ return(0);						}
		virtual int select(void)		{ return(0);						}
		virtual int unselect(void)		{ return(0);						}
		virtual int focus(bool = TRUE) 	{ return(0);						}	
		virtual int unfocus(void)		{ return(0);						}
		virtual int cancel(void)		{ return(0);						}
		virtual int done(void)			{ return(0);						}
		virtual int refresh(void)	   { return(0); 						}
};

#endif // _CUI_COMPOSITE_H

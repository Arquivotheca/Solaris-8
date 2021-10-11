#pragma ident "@(#)vcontrol.h   1.4     92/11/25 SMI"

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
//	VirtualControl class definition
//		(Control that's associtated with another Widget)
//
//	$RCSfile: vcontrol.h $ $Revision: 1.11 $ $Date: 1992/09/12 15:18:05 $
//=============================================================================

#ifndef _CUI_VCONTROL_H
#define _CUI_VCONTROL_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_CONTROL_H
#include "control.h"
#endif

#endif // PRE_COMPILED_HEADERS


class VirtualControl : public Control
{
	private:

    protected:

		Widget	*widget;					// pointer to our Widget
		bool	widgetCurrent;				// is our Widget current?
		bool	valid(void);				// is our Widget valid?

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		VirtualControl(char *Name, Widget * = NULL, CUI_Resource * = NULL,
					   CUI_WidgetId id = CUI_VCONTROL_ID);
        virtual int realize(void);
		virtual ~VirtualControl(void);

		// access methods

		Widget *&widgetPtr(void)			{ return(widget);				}
		bool   &currentStatus(void) 		{ return(widgetCurrent);		}

        // utility routines

		virtual int isKindOf(CUI_WidgetId type)
										 { return(type == CUI_VCONTROL_ID); }

		virtual void locateCursor(void) { widget->locateCursor();			}

		// we never resize...

		virtual int resize(short, short)			  { return(0);			}

		// message-handlers

		virtual int doKey(int key, Widget *from = NULL);
		virtual bool filterKey(CUI_KeyFilter filter, int key)
								  { return(widget->filterKey(filter, key)); }
		virtual int show(void)		  { return(widget->show()); 			}
		virtual int hide(void)		  { return(widget->hide()); 			}
		virtual int select(void)	  { return(widget->select());			}
		virtual int unselect(void)	  { return(widget->unselect()); 		}
		virtual int focus(bool reset = TRUE) 	  
			{ return(widget->focus(reset));			}
		virtual int unfocus(void)	  { return(widget->unfocus());			}
		virtual int cancel(void)	  { return(widget->cancel());			}
		virtual int done(void)		  { return(widget->done()); 			}
		virtual int refresh(void)	  { return(widget->refresh());			}
		virtual int interpret(char *c){ return(widget->interpret(c));		}
};
#define NULL_VCONTROL (VirtualControl *)0


#endif // _CUI_MCONTROL_H



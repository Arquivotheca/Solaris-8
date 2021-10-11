#pragma ident "@(#)gauge.h   1.4     92/11/25 SMI"

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
//	SliderControl, Gauge, and Slider class definitions
//
//	$RCSfile: gauge.h $ $Revision: 1.4 $ $Date: 1992/09/12 15:17:58 $
//=============================================================================

#ifndef _CUI_GAUGE_H
#define _CUI_GAUGE_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_COMPOSITE_H
#include "composite.h"
#endif
#ifndef _CUI_CONTROL_H
#include "control.h"
#endif

#endif // PRE_COMPILED_HEADERS


//=============================================================================
//	SliderControl class definition
// (this is the slider portion of the Guage/Slider)
//=============================================================================

class SliderControl : public Control
{
	// friends:

		friend class Gauge;

	protected:

		short ticks;
		short value;
		short minValue;
		short maxValue;

		virtual int  setValue(CUI_Resource *);
		virtual int  getValue(CUI_Resource *);
				void setSliderValue(int newValue);
				int  updateSlider(void);
				int  valuesPerTick(void);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor

		SliderControl(char *Name, Widget * = NULL, CUI_Resource * = NULL,
					  CUI_WidgetId = CUI_SLIDER_CONTROL_ID);
        virtual int realize(void);

		// utility routines

        virtual int isKindOf(CUI_WidgetId type)
		{ return(type == CUI_SLIDER_CONTROL_ID || Control::isKindOf(type)); }

		// message handlers

		virtual int doKey(int key = 0, Widget * from = NULL);
};
#define NULL_SLIDER_CONTROL (SliderControl *)0


//=============================================================================
//	Gauge class definition
//=============================================================================

class Gauge : public Composite
{
	protected:

		char  *minLabel;
		char  *maxLabel;
		short normalColor;	   // color at rest
		short activeColor;	   // color when current

        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor

		Gauge(char *Name, Widget * = NULL, CUI_Resource * = NULL,
			  CUI_WidgetId = CUI_GAUGE_ID);
        virtual int realize(void);

		// geometry-management

		virtual int manageGeometry(void);

        // utility routines

        virtual int isKindOf(CUI_WidgetId type)
			   { return(type == CUI_GAUGE_ID || Composite::isKindOf(type)); }
		virtual int setDefault(void)   { return(children[0]->setDefault()); }

		// beware! these won't pass on to SliderControl

		virtual short &normalAttrib()		{ return(normalColor);			}
		virtual short &activeAttrib()		{ return(activeColor);			}

		// message handlers

		virtual int doKey(int key, Widget *from = NULL)
                                 { return(children[0]->doKey(key, from));   }
        virtual int show(void)          { return(children[0]->show());      }
		virtual int hide(void)			{ return(children[0]->hide());		}
		virtual int select(void)		{ return(children[0]->select());	}
		virtual int unselect(void)		{ return(children[0]->unselect());	}
		virtual int focus(bool reset = TRUE) 		
			{ return(children[0]->focus(reset)); 	}
		virtual int unfocus(void)		{ return(children[0]->unfocus());	}
		virtual int refresh(void)		{ return(children[0]->refresh());	}
};
#define NULL_GAUGE (Gauge *)0


//=============================================================================
//	Slider class definition
//=============================================================================

class Slider : public Gauge
{
    public:

		// constructor

		Slider(char *Name, Widget * = NULL, CUI_Resource * = NULL,
			   CUI_WidgetId = CUI_SLIDER_ID);
        virtual int realize(void);

        // utility routines

        virtual int isKindOf(CUI_WidgetId type)
				  { return(type == CUI_SLIDER_ID || Gauge::isKindOf(type)); }
};
#define NULL_SLIDER (Slider *)0


#endif // _CUI_GAUGE_H



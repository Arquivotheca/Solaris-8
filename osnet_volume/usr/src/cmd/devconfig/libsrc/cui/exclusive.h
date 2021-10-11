#pragma ident "@(#)exclusive.h   1.3     92/11/25 SMI"

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
//	Exclusive and derived class (Exclusives, NonExclusives) definitions
//		(populate with RectButtons or CheckBoxes)
//
//	$RCSfile: exclusive.h $ $Revision: 1.13 $ $Date: 1992/09/12 15:29:58 $
//=============================================================================


#ifndef _CUI_EXCLUSIVE_H
#define _CUI_EXCLUSIVE_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_AREA_H
#include "area.h"
#endif
#ifndef _CUI_CONTROL_H
#include "control.h"
#endif

#endif // PRE_COMPILED_HEADERS


//=============================================================================
//	Exclusive is an abstract class...
//=============================================================================

class Exclusive : public ControlArea
{
	protected:

		CUI_StringId	layout;
		short			measure;

		virtual int setValue(CUI_Resource *);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		Exclusive(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				  CUI_WidgetId = CUI_WIDGET_ID);
        virtual int realize(void);
		virtual ~Exclusive(void)			{ /* nothing */ 				}

        // utility routines

		virtual int isKindOf(CUI_WidgetId)	{ return FALSE; 				}
		virtual int manageGeometry(void);
};


//=============================================================================
//	derived Exclusives and NonExclusives classes
//=============================================================================

class Exclusives : public Exclusive
{
    public:

		Exclusives(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				   CUI_WidgetId = CUI_EXCLUSIVES_ID);
        virtual int isKindOf(CUI_WidgetId type)
		{
			return(type == CUI_EXCLUSIVES_ID || ControlArea::isKindOf(type));
        }
};

class NonExclusives : public Exclusive
{
    public:

		NonExclusives(char *Name, Widget * = NULL, CUI_Resource * = NULL,
					  CUI_WidgetId = CUI_NONEXCLUSIVES_ID);
        virtual int isKindOf(CUI_WidgetId type)
		{
			return(type == CUI_NONEXCLUSIVES_ID || ControlArea::isKindOf(type));
		}
};


#endif // _CUI_EXCLUSIVE_H



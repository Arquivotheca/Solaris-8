#pragma ident "@(#)footer.h   1.4     92/11/25 SMI"

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
//	FooterPanel class definition (a Composite with an associated ETI Form)
//
//	$RCSfile: footer.h $ $Revision: 1.3 $ $Date: 1992/09/12 15:16:50 $
//=============================================================================

#ifndef _CUI_FOOTER_H
#define _CUI_FOOTER_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_AREA_H
#include "area.h"
#endif
#ifndef _CUI_TEXT_H
#include "text.h"
#endif

#endif // PRE_COMPILED_HEADERS


class FooterPanel : public ControlArea
{
	protected:

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		FooterPanel(char *Name, Widget * = NULL, CUI_Resource * = NULL,
					CUI_WidgetId = CUI_FOOTER_ID);
        virtual int realize(void);

        // utility routines

        virtual int isKindOf(CUI_WidgetId type)
		{
			return(type == CUI_FOOTER_ID || ControlArea::isKindOf(type));
		}
		StaticText *getStaticText(void);

        // message-handlers

        virtual int show(void);
		virtual int hide(void);
};

#endif // _CUI_FOOTER_H



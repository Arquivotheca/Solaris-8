#pragma ident "@(#)shell.h   1.4     92/11/25 SMI"

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
//	Shell class definition
//
//	$RCSfile: shell.h $ $Revision: 1.7 $ $Date: 1992/09/12 15:16:52 $
//=============================================================================

#ifndef _CUI_SHELL_H
#define _CUI_SHELL_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_AREA_H
#include "area.h"
#endif
#ifndef _CUI_TEXT_H
#include "text.h"
#endif

#endif // PRE_COMPILED_HEADERS


class Shell : public ControlArea
{
	protected:

        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);
        int         doExit();

    public:

		// memory-management
		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		Shell(char *Name, Widget * = NULL, CUI_Resource * = NULL,
			  CUI_WidgetId id = CUI_SHELL_ID);
		virtual int realize(void);
		StaticText *getFooterText(void);

        // utility routines

        virtual int isKindOf(CUI_WidgetId type)
		{
			return(type == CUI_SHELL_ID || ControlArea::isKindOf(type));
		}

		// message-handlers

		virtual int show();
		virtual int hide();
        virtual int select();
		virtual int unselect();
		virtual int cancel();
		virtual int done();
};

#endif // _CUI_SHELL_H


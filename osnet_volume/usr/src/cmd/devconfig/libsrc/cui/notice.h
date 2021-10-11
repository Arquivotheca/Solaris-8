#pragma ident "@(#)notice.h   1.3     92/11/25 SMI"

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
//	Notice widget definition
//
//	$RCSfile: notice.h $
//=============================================================================

#ifndef _CUI_NOTICE_H
#define _CUI_NOTICE_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_POPWIN_H
#include "popwin.h"
#endif
#ifndef _CUI_TEXT_H
#include "text.h"
#endif
#ifndef _CUI_BUTTON_H
#include "button.h"
#endif

#endif // PRE_COMPILED_HEADERS


class Notice: public PopupWindow
{
	private:

		StaticText* 	t;
		OblongButton*	ok;

	protected:

				int saveString;   // save compiled 'string' resource

        virtual int adjustChildren(void);

	public:

		Notice(char* Name, Widget* = NULL, CUI_Resource* = NULL,
			   CUI_WidgetId id = CUI_NOTICE_ID);
        ~Notice(void);

		virtual int isKindOf(CUI_WidgetId type)
			{ return(type == CUI_NOTICE_ID || PopupWindow::isKindOf(type)); }
        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);
		virtual int realize(void);
		virtual int cancel(void) { return(0); }  // ignore ESC key
};

#endif // _CUI_NOTICE_H


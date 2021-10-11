#pragma ident "@(#)txtpanel.h   1.4     99/02/19 SMI"

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
//	TextPanel class definition
//
//	$RCSfile: txtpanel.h $ $Revision: 1.9 $ $Date: 1992/09/12 15:19:26 $
//=============================================================================

#ifndef _CUI_TEXTPANEL_H
#define _CUI_TEXTPANEL_H

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_AREA_H
#include "area.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif

#endif // PRE_COMPILED_HEADERS


class TextPanel : public ControlArea
{
	protected:

		char  *file;
		int   firstLine;
		int   line;
		int   column;
		int   index;
		int   numLines;
		short visibleLines;
		bool  hadNewline;
		CUI_TextFunc func;

        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);

		void seek(int line);
        void refreshContents(void);
		void syncCursor(void)
		  { if(window) window->setcur(line - firstLine, column); }
		void drawScrollBar(void);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		TextPanel(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				  CUI_WidgetId id = CUI_TEXTPANEL_ID);
        virtual ~TextPanel(void)                { /* nothing */             }

        // utility routines

		virtual int  isKindOf(CUI_WidgetId type)
		   { return(type == CUI_TEXTPANEL_ID || ControlArea::isKindOf(type)); }
		virtual int realize(void);

		// class-specific routines

		void setcur(int r, int c)				{ window->setcur(r, c); 	}
		void print(char *text);
		void clear(void)			 { hadNewline = FALSE; window->clear(); }
        void clearRow(int rownum)                  { window->clearRow(rownum);    }
		int  scroll(int count)			{ return(window->scroll(count));	}
		int printLine(int rownum, char *text = NULL);

		// message-handlers

		virtual int doKey(int key, Widget *from = NULL);
		virtual int show(void);
};

#endif // _CUI_TEXTPANEL_H


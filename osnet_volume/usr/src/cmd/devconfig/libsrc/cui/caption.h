#pragma ident "@(#)caption.h   1.8     93/03/15 SMI"

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
//	Caption class definition (label plus one child widget)
//
//	$RCSfile: caption.h $ $Revision: 1.2 $ $Date: 1992/12/29 22:13:55 $
//=============================================================================


#ifndef _CUI_CAPTION_H
#define _CUI_CAPTION_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_COMPOSITE_H
#include "composite.h"
#endif

#endif // PRE_COMPILED_HEADERS


class Caption : public Composite
{
	protected:

		int 		 label; 	// compiled value for label string
		CUI_StringId position;	// leftId, rightId, topId, bottomId
		CUI_StringId alignment; // topId, bottomId, centerId, leftId, rightId
		byte		 space; 	// spacing between label and child
		short		 normalColor;

        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);
				int addLabel(int labelValue);
				void syncFlags(void);

	public:

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		Caption(char *Name, Widget * = NULL, CUI_Resource * = NULL,
				CUI_WidgetId = CUI_CAPTION_ID);
        virtual int realize(void);
		virtual ~Caption(void);

		// geometry-management

		virtual int manageGeometry(void);

		// utility routines

		virtual int isKindOf(CUI_WidgetId type)
		{
			return(ID == CUI_CAPTION_ID || Composite::isKindOf(type));
		}
		virtual short &normalAttrib()		{ return(normalColor);			}
        virtual int addChild(Widget *);
		virtual int labelLength(void);
		virtual int alignLabel(short Col);
		virtual CUI_StringId getPosition(void) { return(position); }
		FIELD	*ETIfield(void)
							{ return(children[numChildren - 1]->ETIfield()); }
		virtual int setDefault(void)
						  { return(children[numChildren - 1]->setDefault()); }
		Widget *getChild(void)
			  { return(numChildren >= 0 ? children[numChildren - 1] : NULL); }

		// message-handlers

		virtual int doKey(int key = 0, Widget * from = NULL)
			 { return(numChildren == 2 ? children[1]->doKey(key, from) : 0); }
		virtual int show(void)
					    { return(children[numChildren - 1]->show()); }
		virtual int hide(void)
					    { return(children[numChildren - 1]->hide()); }
		virtual int select(void)
					  { return(children[numChildren - 1]->select()); }
		virtual int unselect(void)
					{ return(children[numChildren - 1]->unselect()); }
		virtual int focus(bool reset = TRUE)
				   { return(children[numChildren - 1]->focus(reset)); }
		virtual int unfocus(void)
					 { return(children[numChildren - 1]->unfocus()); }
		virtual int cancel(void)		{ return(0);			 }
		virtual int done(void)			{ return(0);		 	 }
		virtual int refresh(void)
				   		  { return(children[numChildren - 1]->refresh()); }
};

#endif // _CUI_CAPTION_H



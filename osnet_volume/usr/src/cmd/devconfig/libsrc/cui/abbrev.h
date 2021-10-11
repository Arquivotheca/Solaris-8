#ident "@(#)abbrev.h 1.7 93/03/15 SMI"

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
//	AbbrevMenu class definition
//
//	$RCSfile: abbrev.h $ $Revision: 1.9 $ $Date: 1992/09/12 15:16:56 $
//=============================================================================

#ifndef _CUI_ABBREV_H
#define _CUI_ABBREV_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_COMPOSITE_H
#include "composite.h"
#endif
#ifndef  _CUI_SYSCURSES_H
#include "syscurses.h"
#endif

#endif // PRE_COMPILED_HEADERS


class AbbrevMenuButton : public Composite
{
	protected:

		short normalColor;	   // color at rest
		short activeColor;	   // color when current

        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);
				int addLabel(int labelValue);

	public:

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		AbbrevMenuButton(char *Name, Widget * = NULL, CUI_Resource * = NULL);
		virtual int realize(void);
		virtual ~AbbrevMenuButton(void);

		// geometry-management

		virtual int manageGeometry(void);

		// utility routines

		virtual int isKindOf(CUI_WidgetId type)
			{
				return(ID == CUI_ABBREV_ID || Composite::isKindOf(type));
            }
        virtual short &normalAttrib()       { return(normalColor);          }
        virtual short &activeAttrib()       { return(activeColor);          }
        virtual Window *getWindow(void);
        virtual int addChild(Widget *);
		int setText(void);
		FIELD	*ETIfield(void) 		 { return(children[0]->ETIfield()); }
        virtual int  setDefault(void)  { return(children[0]->setDefault()); }

		// message-handlers

		virtual int doKey(int key = 0, Widget * from = NULL)
								   { return(children[0]->doKey(key, from)); }
		virtual int show(void);
		virtual int hide(void)		{ return(children[0]->hide());			}
		virtual int select(void)	{ return(children[0]->select());		}
		virtual int unselect(void)	{ return(children[0]->unselect());		}
		virtual int focus(bool reset = TRUE);
		virtual int unfocus(void);
		virtual int cancel(void)	{ return(children[0]->cancel());		}
		virtual int done(void)		{ return(0);							}
        virtual int refresh(void)   { return(children[0]->refresh());       }
};

#endif // _CUI_ABBREV_H




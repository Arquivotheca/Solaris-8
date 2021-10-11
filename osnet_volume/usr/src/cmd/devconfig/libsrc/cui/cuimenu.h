#pragma ident "@(#)cuimenu.h 1.8 93/03/15 SMI"

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
//	Menu class definition
//
//	$RCSfile: cuimenu.h $ $Revision: 1.30 $ $Date: 1992/09/12 15:30:15 $
//=============================================================================


#ifndef _CUI_MENU_H
#define _CUI_MENU_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_COMPOSITE_H
#include "composite.h"
#endif
#ifndef _CUI_MITEM_H
#include "mitem.h"
#endif

#endif // PRE_COMPILED_HEADERS


class Menu : public Composite
{
	friend class AbbrevMenuButton;	// needs access to internals

	private:

		static char   *menuToggle;
		static char   *menuRegular;

    // friends:

		friend int Item::realize(void);  // calls addItem()

    protected:

		char	*title; 			// title for our window
		Window	*window;			// window we're displayed in
		ITEM	**items;			// array of ETI menu-items
		int 	itemArraySize;		// size of items array (we can resize)
		int 	numItems;			// number of items currently in array
		MENU	*menu;				// the ETI menu
		CUI_StringId layout;		// fixedRows/fixedCols
		short	measure;			// number of rows/cols
		bool	scrolling;			// do we scroll?
		bool	poppedUp;			// are we popped-up?
		short	saveDefault;		// saved index of default item
		short	borderColor;		// color of our border (if any)
		short	titleColor; 		// color of our title (if any)
		short	normalColor;		// color of our regular Items
		short	activeColor;		// color of our active Items
		short	disabledColor;		// color of our disabled Items

		// internal utility routines

		virtual int adjustChildren(void);
        virtual int setValue(CUI_Resource *);
		virtual int getValue(CUI_Resource *);
		int 		translateKey(int key);
		int 		initWindow(void);
		int 		freeWindow(void);
        int         connectItems(void);
		virtual int setDimensions(void);
        void        getDimensions(void);
		bool		haveSensitiveItem(void);
		void		setMarkString(void);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		Menu(char *Name, Widget * = NULL, CUI_Resource * = NULL,
			 CUI_WidgetId = CUI_MENU_ID);
        virtual int realize(void);
		virtual ~Menu(void);

		// access methods

		virtual short  &borderAttrib()		{ return(borderColor);			}
		virtual short  &titleAttrib()		{ return(titleColor);			}
        virtual short  &normalAttrib()      { return(normalColor);          }
		virtual short  &activeAttrib()		{ return(activeColor);			}
		virtual short  &disabledAttrib()	{ return(disabledColor);		}
		static	char   *toggleMark(void)	{ return(menuToggle);			}
		static	char   *regularMark(void)	{ return(menuRegular);			}
        virtual Window *getWindow(void)     { return(window);               }
        bool    twoItemsSet(void);
		virtual int    setDefault(void) 	{ return(0);					}
				MENU   *ETImenu(void)		{ return(menu); 				}

		// utility routines

		Widget *currentWidget(void)
		{ return(menu ? (Widget *)item_userptr(current_item(menu)) : NULL); }
        virtual int isKindOf(CUI_WidgetId type)
				{ return(type == CUI_MENU_ID || Composite::isKindOf(type)); }
		int 	lookupETIitem(ITEM *item);
		ITEM	*lookupETIitem(int index);
		virtual int addItem(ITEM *item, int index = INT_MAX);
		int 	removeItem(ITEM *item);
		virtual void locateCursor(void) { pos_menu_cursor(menu); refresh(); }
		virtual bool isCurrent(void)	{ return(FALSE);					}
		void	unsetSelected(void);
		bool	setDefaultItem(void);
		int 	popUp(CUI_GrabMode mode = CUI_GRAB_EXCLUSIVE);
		void	popDown(void);
        bool    isPoppedUp()            { return(poppedUp);                 }
		void	forceRefresh(void);

		// interface for API routines

		int 	addItem(int index, Item *item);
		int 	deleteItem(int index);
		Item	*lookupItem(int index);
		int 	setCurrentItem(int index);
		int 	getCurrentItem(void);
		int		countItems(void)		{ return(numItems);					}

        // add Child

		virtual int addChild(Widget *);

		// message-handlers

		virtual int doKey(int key, Widget *from = NULL);
		virtual bool filterKey(CUI_KeyFilter, int);
        virtual int show(void);
		virtual int hide(void);
		virtual int select(void);
		virtual int unselect(void)		{ return(0);						}
		virtual int focus(bool = TRUE)
							   { doCallback(CUI_FOCUS_CALLBACK); return(0); }
		virtual int unfocus(void)
							 { doCallback(CUI_UNFOCUS_CALLBACK); return(0); }
        virtual int cancel(void);
		virtual int done(void)			{ return(0);						}
		virtual int locate(short Row, short Col);
		virtual int resize(short Height, short Width)
								   { return(Widget::resize(Height, Width)); }
        virtual int refresh(void);
		virtual int interpret(char *c)	{ badCommand(c); return(-1);		}
};
#define NULL_MENU (Menu *)0


#endif // _CUI_MENU_H


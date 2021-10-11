#pragma ident "@(#)list.cc 1.14 93/07/23 SMI"

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

//=============================================================================
//	ScrollingList class implementation
//
//	$RCSfile: list.cc $ $Revision: 1.2 $ $Date: 1992/12/30 02:31:37 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#include <stdio.h>
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_STRINGID_H
#include "stringid.h"
#endif
#ifndef  _CUI_AREA_H
#include "area.h"
#endif
#ifndef  _CUI_LIST_H
#include "list.h"
#endif
#ifndef  _CUI_LITEM_H
#include "litem.h"
#endif
#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif

#endif	// PRE_COMPILED_HEADERS


// builtin callback

static int popupCallback(CUI_Widget, void *, void *);


// resources to load

static CUI_StringId resList[] =
{
	sensitiveId,
    nullStringId
};


//
//	constructor
//

ScrollingList::ScrollingList(char *Name, Widget *Parent,
							 CUI_Resource *Resources, CUI_WidgetId id)
	: Menu(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;

    // set default values

	flags |= CUI_ALIGN_CAPTIONS;
	flags |= CUI_SENSITIVE;
    childArraySize = 10;
	numChildren = 0;
	MEMHINT();
	children = (Widget**)CUI_malloc(childArraySize * sizeof(Widget *));

	short heightSet = (height != 1);

    // load resources

    loadResources(resList);
	setValues(Resources);

	// if height was specified, we're not resizeable (add 2 now for border)
	// (this duplicates logic in menu.cc, but height resource can be applied
	// specifically to ScrollingList in the resource list passed as args)

	if(height != 1 && !heightSet)
	{
		height += 2;
        flags &= ~CUI_RESIZEABLE;
	}
}


//
//	realize
//

int ScrollingList::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// we must behave is if we're a Control to our parent, as well as
	// a ControlArea to our children; in order to do so, we create, locate,
	// and realize our VirtualControl (this is simply an invisible Control
	// that points back to us; it handles inter-Control navigation events
	// itself, and passes the rest to us)

	sprintf(CUI_buffer, "%s@Control", name);
	MEMHINT();
    control = new VirtualControl(CUI_buffer, NULL);
	control->widgetPtr() = this;
	control->locate(row + 1, col + 1);	// allow for our border
	control->realize();

	// process any children allocated to us in resource file

	processSavedChildren();

	// adjust our location relative to parent

//	  if(row != -1 && col != -1)
//		  makeAbsolute(row, col);

	// tell Menu's realization routine to do its stuff (will recreate window)

	Menu::realize();

    return(0);
}


//
//	destructor
//

ScrollingList::~ScrollingList(void)
{
	MEMHINT();
    CUI_free(children);
	MEMHINT();
	delete(popupMenu);
    MEMHINT();
    delete(control);
}


//
//	geometry-management
//

int ScrollingList::manageGeometry(void)
{
	// process any children allocated in resource files
	// set our mark-string (affects our size!)
	// then invoke Menu's getDimensions method to calculate our size

	processSavedChildren();
	setMarkString();
    getDimensions();
    return(0);
}


//
//	resource routines
//

int ScrollingList::setValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
        default:
			return(Menu::setValue(resource));
	}
}

int ScrollingList::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(Menu::getValue(resource));
	}
}


//
//	add Child
//
//	This will be invoked (for ListItems) only if we define children
//	in a resource file; all we do is save child pointers in array -
//	on realization we'll check they're ListItems, and add to our menu.
//	We can't add to menu directly, since we're in the middle of the
//	ListItem's constructor, and we can't virtualize the necessary methods
//	(eg, realize - Item must be realized to be added to a Menu).
//

int ScrollingList::addChild(Widget *child)
{
	// check child's type and process accordingly:
	// Menu child becomes our 'local list menu',
	// ListItem children saved for adding to our own menu

	switch(child->getId())
	{
		case CUI_MENU_ID:
		{
			popupMenu = (Menu *)child;
			break;
		}
		case CUI_LITEM_ID:
		{
			// grow child array if necessary, then add child to it

			if(numChildren == childArraySize - 1)  // keep final NULL!
			{
				children = (Widget **)CUI_arrayGrow((char **)children, 10);
				childArraySize += 10;
			}
			children[numChildren++] = child;
			break;
        }
		default:
		{
			char *message;
			message = dgettext( CUI_MESSAGES, "Can't add %s widget as child of ScrollingList");
			CUI_fatal(message, Widget::typeName(child->getId()));
        }
	}
	return(0);
}


//
//	focus/unfocus (set/reset menu's foreground attrib to highlight
//	unless we're in an unfocussed ControlArea, or we have no 'actions'
//	associated with making a selection)
//

#define CAN_SELECT (popupMenu || (flags & CUI_TOGGLE))

int ScrollingList::focus(bool)
{
    if(menu)
	{
		extern bool CUI_doRefresh;
		extern void CUI_itemInitHook(MENU *menu);

		doCallback(CUI_FOCUS_CALLBACK);
#ifdef META_NAVIGATION
		menu_driver(menu, REQ_FIRST_ITEM);
#endif // META_NAVIGATION
        ControlArea *containingArea = getControlArea();
		if(containingArea && containingArea->isCurrent() && CAN_SELECT)
			set_menu_fore(menu, Color::lookupValue(activeColor));
		else
			set_menu_fore(menu, Color::lookupValue(normalColor));
        CUI_itemInitHook(menu);
        CUI_doRefresh = FALSE;
        refresh();
	}
	return(0);
}

int ScrollingList::unfocus(void)
{
    if(menu)
	{
		doCallback(CUI_UNFOCUS_CALLBACK);
		set_menu_fore(menu, Color::lookupValue(normalColor));
        refresh();
	}
	return(0);
}


//
//	show self
//

int ScrollingList::show(void)
{
#ifdef NO_LONGER_NEEDED
	// in order to ensure that our Window is a child of our containing
	// ControlArea's window, we must delete it and recreate it (ugh!)
	// (when it was first created, the parent window didn't exist...)
	// avoid doing this more than once by setting flag...

	if(!(flags & CUI_INITIALIZED))
	{
		freeWindow();

		// adjust our location relative to parent

		if(row != -1 && col != -1)
			makeAbsolute(row, col);

        initWindow();
		flags |= CUI_INITIALIZED;
	}
#endif
	return(Menu::show());
}


//
// if we have any children saved in our array (from resource file)
// realize then, and add them to our menu
//

int ScrollingList::processSavedChildren(void)
{
	for(int i = 0; i < numChildren; i++)
		Menu::addItem(INT_MAX, (Item *)children[i]);
	numChildren = 0;
	return(0);
}


//
//	add ITEM to our List
//	(if we have a Menu resource, make sure this Item pops it up on select)
//

int ScrollingList::addItem(ITEM *ETIitem, int index)
{
	if(popupMenu)
	{
		Item *item = (Item *)item_userptr(ETIitem);
		CUI_addCallback(item, CUI_SELECT_CALLBACK, popupCallback, popupMenu);
	}
	return(Menu::addItem(ETIitem, index));
}


//
//	callback to popup the associated Menu on Item select
//

static int popupCallback(CUI_Widget widget, void *client_data, void *)
{
    short row, col;
    CUI_getCursor(&row, &col);
	ListItem *item = (ListItem *)widget;
	Widget *list = item->getParent();
	CUI_setCursor(row, col + (list->wWidth() / 2) - 1);
	CUI_Widget menu = (CUI_Widget)client_data;
    CUI_popup(menu, CUI_GRAB_EXCLUSIVE);
	return(0);
}


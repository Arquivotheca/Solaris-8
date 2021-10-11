#ident "@(#)mitem.cc 1.7 93/07/23 SMI"

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
//	MenuItem class implementation
//
//	$RCSfile: mitem.cc $ $Revision: 1.25 $ $Date: 1992/09/25 01:13:25 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#include <stdio.h>
#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_CONTROL_H
#include "control.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_MENU_H
#include "cuimenu.h"
#endif
#ifndef  _CUI_PROTO_H
#include "cuiproto.h"
#endif
#ifndef _CUI_CURSVMEM_H
#include "cursvmem.h"
#endif
#ifndef _CUI_EMANAGER_H
#include "emanager.h"
#endif
#ifndef _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif

#endif // PRE_COMPILED_HEADERS


extern bool CUI_doRefresh;


// flag values for our menus

static const long submenu_flags = CUI_RESIZEABLE | CUI_HIDE_ON_EXIT | CUI_ALIGN_CAPTIONS;

// the best we can do in ASCII for a right-arrow

static const chtype ascii_rarrow = '>';

// and in vidMemWrite mode...

static const chtype vidmem_rarrow = ACS_FAT_RARROW | A_ALTCHARSET;


// resources to load

static CUI_StringId resList[] =
{
	menuId,
    nullStringId
};

MenuItem::MenuItem(char* Name, Widget* Parent, CUI_Resource* Resources,
				   CUI_WidgetId id)
	: Item(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;

    // set default values

	subMenu 	= NULL;
	poppedUp	= FALSE;

	// load resources

    loadResources(resList);
	setValues(Resources);
}


//
//	realize
//

int MenuItem::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// if we have a submenu...

	if(subMenu)
	{
		// child must be menu type

		Widget::verifyIsKindOf(subMenu, CUI_MENU_ID);

		// realize our submenu

		subMenu->realize();

		// extend label by 2 to allow for arrow

		sprintf(CUI_buffer, "%s   ", label);
		MEMHINT();
        CUI_free(label);
		MEMHINT();
        label = CUI_newString(CUI_buffer);
		width = strlen(label);
	}

	// now we've (possibly) modified our label, tell Item to do its stuff

	Item::realize();

	// add our ETI ITEM to parent's ETI MENU

	if(parent)
		((Menu *)parent)->addItem(item);

    flags |= CUI_REALIZED;
    return(0);
}


//
//	destructor
//

MenuItem::~MenuItem(void)
{
	if(subMenu)
	{
		MEMHINT();
        delete(subMenu);
	}
}


//
//	resource routines
//

int MenuItem::setValue(CUI_Resource *resource)
{
	int value = (int)resource->value;
	char *stringValue  = CUI_lookupString(value);
    switch(resource->id)
	{
		case menuId: // name of menu to invoke
		{
			Menu *menu = (Menu *)Widget::lookup(stringValue);
            if(!menu)
				CUI_fatal(dgettext( CUI_MESSAGES, "Can't find menu %s to associate with mitem %s"),
						  stringValue, name);
			return(defineSubmenu(menu));
        }
        default:
			return(Item::setValue(resource));
	}
}

int MenuItem::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(Item::getValue(resource));
	}
}


//
//	process/filter keystroke
//

int MenuItem::doKey(int key, Widget * from)
{
	// if we have a popped-up submenu, pass the keystroke on
	// else return -1 to tell caller to proces it
	// (transform KEY_RIGHT into KEY_RETURN for OLIT compatibility)

	if(poppedUp)
	{
		if(key == KEY_RIGHT)
			key = KEY_RETURN;
		return(subMenu->doKey(key, from));
	}
	else
		return(-1);
}

bool MenuItem::filterKey(CUI_KeyFilter filter, int key)
{
	// if we have a popped-up submenu, pass the operation on
	// else filter it here

	if(poppedUp)
		return(subMenu->filterKey(filter, key));
	else
		return(filter(this, key));
}


//
//	define a submenu of this MenuItem
//

int MenuItem::defineSubmenu(Menu *menu)
{
	// menu's ID may not have been set yet, so all we can do is
	// verify that we have a widget of some type; we'll check
	// on realization that we really do have a menu as a child

	menu = (Menu *)Widget::verify(menu);

	// set menu flags and save pointer to subMenu

	menu->flagValues() |= submenu_flags;
	subMenu = menu;

    return(0);
}


//
//	process selection
//

int MenuItem::select()
{
	// first we tell Item to do its stuff...

	Item::select();

	// if we have a subMenu, pop it up

	if(subMenu)
	{
		// locate subMenu

		if(parent->wRow() == -1)   // parent is free-floating
		{
			short Row, Col;
			CUI_getCursor(&Row, &Col);
			subMenu->locate(Row, Col + parent->wWidth() - 3);
		}
		else
		{
			short Row = parent->wRow() + 1;
			Row += item_index(item) - top_row(((Menu *)parent)->ETImenu());
			short Col = parent->wCol() + parent->wWidth() - 1;
			subMenu->locate(Row, Col);
		}

		// tell our menu to show itself, and set poppedUp flag

		subMenu->show();
		poppedUp = TRUE;

		// we don't want to refresh the parent menu, or we'll lose our cursor

		CUI_doRefresh = FALSE;
	}

	return(0);
}


//
//  our child menu has been cancelled
//

int MenuItem::cancel(void)
{
	poppedUp = FALSE;
    return(0);
}


//
//	fix up arrow in vidMemWrite mode
//

int MenuItem::fixupArrow(bool current)
{
	if(subMenu)
	{
		chtype arrow = ascii_rarrow;
		if(vidMemWrite)
			arrow = vidmem_rarrow;

		Window *window = parent->getWindow();
		if(window)
		{
			WINDOW *win = window->getInner();

			chtype attrib = Color::lookupValue(CUI_NORMAL_COLOR);
            Menu *menu = getMenu();
			if(menu)
			{
				if(current)
					attrib = Color::lookupValue(menu->activeAttrib());
				else
					attrib = Color::lookupValue(menu->normalAttrib());
			}
			int Row = item_index(item) - top_row(((Menu *)parent)->ETImenu());
			wmove(win, Row, parent->wWidth() - 4);
			waddch(win, arrow | attrib);
			// wrefresh(win);
		}
	}
	return(0);
}



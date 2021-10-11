#pragma ident "@(#)item.cc 1.10 99/02/19 SMI"

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
//	Item class implementation (abstract class - base for MenuItem and ListItem)
//
//	$RCSfile: item.cc $ $Revision: 1.18 $ $Date: 1992/09/12 15:28:04 $
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
#ifndef  _CUI_CURSVMEM_H
#include "cursvmem.h"
#endif
#ifndef  _CUI_EMANAGER_H
#include "emanager.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif

#endif // PRE_COMPILED_HEADERS


// resources to load

static CUI_StringId resList[] =
{
    labelId,
	setId,
	sensitiveId,
	defaultId,
    nullStringId
};


//
//	constructor
//

Item::Item(char* Name, Widget* Parent, CUI_Resource* Resources,
		   CUI_WidgetId id)
	: Widget(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;

    // set default values

	item  = NULL;
	set   = FALSE;
	MEMHINT();
    label = CUI_newString("NoLabel ");
	flags |= CUI_SENSITIVE;

	// load resources

    loadResources(resList);
	setValues(Resources);
}


//
//	realize
//

int Item::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

    // create the ETI menu-item

	item = new_item(label, NULL);
	if(!item)
		CUI_fatal(dgettext( CUI_MESSAGES, "Can't realize MenuItem"));


    // set value to current value then clear the 'set' flag
    // (we don't use it any more)

    set_item_value(item, set);
    set = FALSE;

	// set selectivity of item depending on flags

	if(!(flags & CUI_SENSITIVE))
	{
		item_opts_off(item, O_SELECTABLE);
	}

    // store back-pointer to self in item's userptr

	set_item_userptr(item, (char *)this);

	flags |= CUI_REALIZED;
    return(0);
}


//
//	destructor
//

Item::~Item(void)
{
	if(item)
		free_item(item);
}


//
//	resource routines
//

int Item::setValue(CUI_Resource *resource)
{
	int value = (int)resource->value;
	char *stringValue  = CUI_lookupString(value);
    switch(resource->id)
	{
		case labelId:
		{
			// add space to label

			sprintf(CUI_buffer, "%s ", stringValue);

			// if we're not realized, delete the default label
			// and create a new one; set width

			if(!(flags & CUI_REALIZED))
			{
				MEMHINT();
				CUI_free(label);
				MEMHINT();
				label = CUI_newString(CUI_buffer);
				width = strlen(label);
			}

			// else copy as much of the new label as will fit
			// into the existing label, and refresh the parent
            // (make sure change won't 'show through' if menu
            // is currently obscured)

			else
			{
				strncpy(label, CUI_buffer, width);
				label[strlen(CUI_buffer)] = 0;
				if(parent)
				{
                    bool save = CUI_updateDisplay(FALSE);
					((Menu *)parent)->forceRefresh();
                    CUI_updateDisplay(save);
				}
			}
			break;
        }
		case setId:
		{
			setOn(value == trueId);
			break;
        }
        default:
			return(Widget::setValue(resource));
	}
	return(0);
}

int Item::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		case setId:
            resource->value = (void *)item_value(item);
			break;
        default:
			return(Widget::getValue(resource));
	}
	return(0);
}


//
//	set value
//

void Item::setOn(bool on)
{
    // if realized, set in the ITEM, else save for later

    if(flags & CUI_REALIZED)
        set_item_value(item, on);
    else
        set = on;
}


//
//	is mitem current
//

bool Item::isCurrent(void)
{
	MENU *menu = ETImenu();
	return(menu ? item == current_item(menu) : FALSE);
}


//
//	return pointer to our Menu
//

Menu *Item::getMenu(void)
{
	MENU *Emenu = ETImenu();
	if(Emenu)
		return((Menu *)menu_userptr(Emenu));
	else
		return(NULL);
}


//
//	process selection
//

int Item::select()
{
	// first we invoke any callbacks...

	doCallback(CUI_SELECT_CALLBACK);

    // now we must handle toggle menus...

    long lflags = getMenu()->flagValues();
    if(lflags & CUI_TOGGLE)
    {
        // remember some state...

        bool wasSet   = item_value(item);
        bool mustHave = !(parent->flagValues() & CUI_NONE_SET);
        bool toggleOffMustHave = (wasSet && mustHave);

        // handle exclusive mode...

        if(!(lflags & CUI_MULTI))
        {
            // if already set, and we must have one item,
            // refuse the request

            if(toggleOffMustHave)
            {
                beep();
                return(0);
            }
 
            // turn the currently-set item off
 
            getMenu()->unsetSelected();

            // if we weren't set, set this item on
 
            if(!wasSet)
                set_item_value(item, TRUE);
        }
 
        else // not in exclusive mode
        {
            // if already set, and we must have one item,
            // check to see if at least one other item is set
            // (refuse request if not)
 
            if(toggleOffMustHave)
            {
                Menu *menu = getMenu();
                if(!menu || !menu->twoItemsSet())
                {
                    beep();
                    return(0);
                }
            }
 
            // toggle the current item
 
            set_item_value(item, !wasSet);
        }
    }
    return(0);
}


//
//	align our labels (this tells us how long to make them...)
//

int Item::alignLabel(short Col)
{
	if(label)
	{
		int newLen = Col + 1; // we align so label ends in col 'Col'
		int oldLen = strlen(label);
		if(newLen > oldLen)
		{
			strcpy(CUI_buffer, label);
			char *lptr = CUI_buffer + oldLen;
			for(int i = 0; i < newLen - oldLen; i++, lptr++)
				*lptr = ' ';
			*lptr = 0;
			MEMHINT();
            CUI_free(label);
			MEMHINT();
            label = CUI_newString(CUI_buffer);
		}
	}
	return(0);
}


//
//	focus/unfocus
//

int Item::focus(bool)
{
	doCallback(CUI_FOCUS_CALLBACK);
    return(0);
}

int Item::unfocus(void)
{
	doCallback(CUI_UNFOCUS_CALLBACK);
    return(0);
}


//
//	redraw item in focus or non-focus mode
//	(tried to use this to handle focussing myself, so we don't focus
//	all selected items in multi-valued menu, but it doesn't help; the
//	standard redraw logic over-rules my efforts)
//

int Item::redraw(bool focus)
{
	Window *window = NULL;
    Menu *menu     = getMenu();
    if(menu)
		window = menu->getWindow();
	if(window)
	{
		chtype focusAttrib = Color::lookupValue(menu->activeAttrib());
		chtype restAttrib  = Color::lookupValue(menu->normalAttrib());
		chtype attrib;
        if(focus)
			attrib = focusAttrib;
		else
			attrib = restAttrib;

		WINDOW *win = window->getInner();
		wmove(win, item->y, (item->x * width));
		wattrset(win, attrib);
		if(menu->flagValues() & CUI_TOGGLE)
			waddstr(win, Menu::toggleMark());
		else
			waddstr(win, Menu::regularMark());
		waddstr(win, label);
		wattrset(win, restAttrib);
        wrefresh(win);
	}
	return(0);
}


//===========================================================================
//						   init/exit hooks
//===========================================================================


#define CURRENT_ITEM(m) (Item *)item_userptr(current_item(m))

void CUI_itemInitHook(MENU *menu)
{
	(CURRENT_ITEM(menu))->focus();
}

void CUI_itemExitHook(MENU *menu)
{
	(CURRENT_ITEM(menu))->unfocus();
}


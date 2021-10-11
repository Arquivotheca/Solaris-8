#pragma ident "@(#)menu.cc 1.15 99/02/19 SMI"

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
//	Menu class definition
//
//	$RCSfile: menu.cc $ $Revision: 1.3 $ $Date: 1992/12/30 01:31:18 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_AREA_H
#include "area.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_MENU_H
#include "cuimenu.h"
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
#ifndef  _CUI_CURSVMEM_H
#include "cursvmem.h"
#endif
#ifndef  _CUI_ABBREV_H
#include "abbrev.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif

#endif	// PRE_COMPILED_HEADERS


#define IS_SELECTABLE(i) (item_opts(i) && O_SELECTABLE)

// prototypes

void calcRowsAndCols(int &rows, int &cols, CUI_StringId layout, short measure,
					 short numChildren);

// hook routines

extern void CUI_itemInitHook(MENU *menu);
extern void CUI_itemExitHook(MENU *menu);


// mark strings and attributes

char *Menu::menuRegular = " ";
char *Menu::menuToggle	= "* ";


// dummy ITEMS array so we can create initially empty menu

static ITEM *dummyItems[2];


// flags to tell us when to refresh, and when not to relocate cursor

// resources to load

static CUI_StringId resList[] =
{
	titleId,
	toggleId,
	exclusiveId,
	layoutTypeId,
    measureId,
	noneSetId,
	borderColorId,
	titleColorId,
	normalColorId,
	activeColorId,
	disabledColorId,
    nullStringId
};


//
//	initialization/cleanup routines
//

static bool menuInitialized = FALSE;

int CUI_initMenu(void)
{
	if(menuInitialized)
		return(0);

	// set init/exit hooks

	set_item_init((MENU *)0, (PTF_void)CUI_itemInitHook);
	set_item_term((MENU *)0, (PTF_void)CUI_itemExitHook);

	// initialize dummy ITEMs array

	dummyItems[0] = new_item("dummy", NULL);
	dummyItems[1] = NULL;

	// set options (init/exit routines are handled in SELECT/UNSELECT)

	set_menu_opts((MENU *)0, O_IGNORECASE);

	// set default attributes for all menus

	set_menu_back((MENU *)0, Color::lookupValue(CUI_NORMAL_COLOR));
	set_menu_fore((MENU *)0, Color::lookupValue(CUI_REVERSE_COLOR));
	set_menu_grey((MENU *)0, Color::lookupValue(CUI_NORMAL_COLOR));

	menuInitialized = TRUE;
	return(0);
}

int CUI_exitMenu(void)
{
	MEMHINT();
    delete(dummyItems[0]);
	return(0);
}


Menu::Menu(char *Name, Widget *Parent, CUI_Resource *resources,
		   CUI_WidgetId id)
	: Composite(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;

	// set defaults

	flags |= CUI_BORDER;				// ensure we have a border
	flags &= ~CUI_HLINE;				// and we're not in HLINE mode
	flags |= CUI_RESIZEABLE;			// and that we're resizeable
	flags |= CUI_REFRESH;				// and that we're refreshing self
	layout		  = fixedColsId;		// format by cols...
	measure 	  = 1;					// one col by default
	scrolling	  = FALSE;				// don't scroll
	poppedUp	  = FALSE;				// not popped-up
	saveDefault   = 0;					// default item on realization
	borderColor   = CUI_NORMAL_COLOR;	// normal border
	titleColor	  = CUI_REVERSE_COLOR;	// reverse title
	normalColor   = CUI_NORMAL_COLOR;	// normal color
	activeColor   = CUI_REVERSE_COLOR;	// active color
	disabledColor = CUI_NORMAL_COLOR;	// disabled color

	// initialize Menu logic if necessary

	if(!menuInitialized)
		CUI_initMenu();

	// initialize array of ITEMs

	itemArraySize = 10;
	numItems  = 0;
	MEMHINT();
	items = (ITEM **)CUI_malloc(itemArraySize * sizeof(ITEM *));

	// create ETI menu with dummy items array, then disconnect for re-use 

	menu = new_menu(dummyItems);
	if(!menu)
		CUI_fatal(dgettext( CUI_MESSAGES, "Can't create Menu '%s'"), Name);
	set_menu_items(menu, NULL);

	// store back-pointer to our self in ETI menu's userptr

	set_menu_userptr(menu, (char *)this);

	// load resources

	loadResources(resList);
	setValues(resources);

    // if height was specified, we're not resizeable (add 2 now for border)

	if(height != 1)
	{
		height += 2;
        flags &= ~CUI_RESIZEABLE;
	}
}


int Menu::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// set attributes for menu items

	set_menu_back(menu, Color::lookupValue(normalColor));
	set_menu_fore(menu, Color::lookupValue(activeColor));
	set_menu_grey(menu, Color::lookupValue(disabledColor));

    // save cursor location (may be needed for window location)

	short cursorRow, cursorCol;
	CUI_getCursor(&cursorRow, &cursorCol);

	// tell Composite to do its stuff

	Composite::realize();

	// set the menu mark-string and options according to flag values

	setMarkString();
	if(flags & CUI_TOGGLE)
		menu_opts_off(menu, O_ONEVALUE);
	else
		menu_opts_on(menu, O_ONEVALUE);

	// set our color attributes

	set_menu_back(menu, Color::lookupValue(normalColor));
	set_menu_fore(menu, Color::lookupValue(activeColor));
	set_menu_grey(menu, Color::lookupValue(disabledColor));

	// restore saved cursor location

	CUI_setCursor(cursorRow, cursorCol);

	// if mappedWhenManaged is set, show self (will create window)

	if(flags & CUI_MAPPED)
		show();

	return(0);
}


Menu::~Menu(void)
{
	MEMHINT();
    CUI_free(title);
	MEMHINT();
    CUI_free(items);
	if(menu)
		free_menu(menu);
	if(window)
	{
		MEMHINT();
        delete(window);
	}
}


//
//	geometry-management
//

int Menu::adjustChildren(void)
{
	// nothing to do for now...
	return(0);
}

int Menu::setDimensions(void)
{
	getDimensions();
	return(0);
}


//
//	resource routines
//

int Menu::setValue(CUI_Resource *resource)
{
	int intValue = (CUI_StringId)resource->value;
	char *strValue = CUI_lookupString(intValue);
    switch(resource->id)
	{
		case titleId:
		{
			MEMHINT();
			title = CUI_newString(strValue);
			if(window)
				window->setTitle(title);
		}
		case toggleId:
		{
			setFlagValue(intValue, CUI_TOGGLE);
			break;
        }
        case noneSetId:
        {
            setFlagValue(intValue, CUI_NONE_SET);
            break;
        }
		case exclusiveId:
		{
			if(intValue == trueId)
				flags &= ~CUI_MULTI;
			else
				flags |= CUI_MULTI;
			break;
		}
        case layoutTypeId:
		{
			switch(intValue)
			{
				case fixedRowsId:
				case fixedColsId:
					layout = (CUI_StringId)intValue;
					break;
				default:
					CUI_fatal(dgettext( CUI_MESSAGES, "Bad value for layoutType resource in %s '%s'"),
							  typeName(), name);
			 }
		}
		case measureId:
		{
			int tmpMeasure = intValue;
			measure = (short)tmpMeasure;
			break;
		}
		case borderColorId:
		{
			return(setColorValue(strValue, borderColor));
		}
		case titleColorId:
		{
			return(setColorValue(strValue, titleColor));
		}
		case normalColorId:
		{
			return(setColorValue(strValue, normalColor));
		}
		case activeColorId:
		{
			return(setColorValue(strValue, activeColor));
		}
		case disabledColorId:
		{
			return(setColorValue(strValue, disabledColor));
		}
        default:
			return(Composite::setValue(resource));
	}
	return(0);
}

int Menu::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(Composite::getValue(resource));
	}
}


//
//	filter/handle key
//

bool Menu::filterKey(CUI_KeyFilter filter, int key)
{
	Widget *item = currentWidget();
	if(item)
		return(item->filterKey(filter, key));
	else
		return(FALSE);
}

int Menu::doKey(int key, Widget * from)
{
	// first check whether the current Item wants to handle the key...

	Widget *current = currentWidget();
	if(current)
	{
		if(current->doKey(key, from) == 0)
			return(0);
	}

	// we must process it here...
	// save current item and translate the key into a menu-driver request

	ITEM *saveItem = current_item(menu);
	int request = translateKey(key);

	// process the request

	switch(request)
	{
		// first we process our extensions

		case REQ_HELP:
		{
			CUI_doRefresh = FALSE;
			if(current)
				current->doHelp();
			else
				doHelp();
			break;
		}
        case REQ_CANCEL:
		{
			// tell self to cancel (and set flag for no refresh)

			cancel();
			CUI_doRefresh = FALSE;
			break;
		}
		case REQ_NEXT_MENU:
		case REQ_PREV_MENU:
        {
			// if we have a parent, and it's a MenuButton...

			if(parent && parent->getId() == CUI_MENUBUTTON_ID)
			{
				// move forward/backward to next widget in ControlArea
				// that is a MenuButton or OblongButton,
				// and is in same row as our button

				ControlArea *area	   = getControlArea();
				Widget		*current   = parent;
                short       currentRow = parent->wRow();

				while(TRUE)
				{
					if(request == REQ_NEXT_MENU)
						current = area->nextWidget(current);
					else
						current = area->previousWidget(current);
					if(!current || current == parent)
					{
						current = NULL;
						break;
					}
					if( (current->getId() == CUI_MENUBUTTON_ID ||
						current->getId() == CUI_OBLONGBUTTON_ID) &&
						current->wRow() == currentRow)
					{
						break;
					}
				}

				// if we've found a suitable candidate...

				if(current)
				{
#ifdef WAS
					// set flag to say we don't want to refresh
					// the previously-current menu (the arrows show through)

					CUI_doRefresh = FALSE;
#endif

					// cancel self and make 'current' the current Widget

					cancel();
					area->setCurrent(current);

					// if we found a MenuButton, send RETURN to ControlArea
					// (so we navigate to and activate the MenuButton)

					Widget *current = area->currentWidget();
					if(current->getId() == CUI_MENUBUTTON_ID)
						area->doKey(KEY_RETURN, NULL);

					area->refresh();
                }
			}
			break;
        }
        case REQ_DONE:
		{
			// tell current item to select itself

			Widget *mitem = currentWidget();
			mitem->select();
			break;
		}
        default:
		{
            // if we don't have at least one selectable item, do nothing

			if(!haveSensitiveItem())
				break;

again:
			// ask the menu driver to process the request

            switch(menu_driver(menu, request))
            {
				case E_OK:
				{
					// ETI libraries will let us visit (but not toggle)
					// a non-selectable item; take care of this here by
					// re-issuing a (possibly modified) request so that
					// we skip the item if it's not selectable

					if(!IS_SELECTABLE(current_item(menu)))
					{
						switch(request)
						{
							case REQ_FIRST_ITEM:
							case REQ_SCR_DPAGE:
								request = REQ_NEXT_ITEM;
								break;
							case REQ_LAST_ITEM:
							case REQ_SCR_UPAGE:
								request = REQ_PREV_ITEM;
								break;
							case REQ_UP_ITEM:
							{
								int index = item_index(current_item(menu));
								if(scrolling && index == 0)
									request = REQ_DOWN_ITEM;
								break;
							}
                            case REQ_DOWN_ITEM:
							{
								int index = item_index(current_item(menu));
								if(scrolling && index == numItems - 1)
									request = REQ_UP_ITEM;
								break;
							}
                        }
						goto again;
					}
					break;
				}
                case E_NO_MATCH:
				{
					// clear pattern buffer & try again

					menu_driver(menu, REQ_CLEAR_PATTERN);
					if(!menu_driver(menu, request))
					{
						// got it - if not selectable, restore
						// previously-current item

						if(!IS_SELECTABLE(current_item(menu)))
							set_current_item(menu, saveItem);
                        break;
					}
					// else fall through...
				}
				default:
				{
					// tell caller we can't handle it
					// (but don't pass on SCR_DPAGE or SCR_UPPAGE)

					if(request == REQ_SCR_DPAGE || request == REQ_SCR_UPAGE)
						return(0);
					else
						return(key);
                }
			}
		}
	}

	// make sure the menu reflects any changes (unless CUI_doRefresh is FALSE)

	if(CUI_doRefresh)
        refresh();
	CUI_doRefresh = TRUE;
    return(0);
}


//
//	translate keystroke into a menu-driver request
//

int Menu::translateKey(int key)
{
	switch(key)
	{
		case KEY_HELP:
			return(REQ_HELP);
        case KEY_UP:
			key = REQ_UP_ITEM;
            break;
		case KEY_DOWN:
			key = REQ_DOWN_ITEM;
            break;
		case KEY_LEFT:
			if(layout == fixedColsId && measure == 1)
				key = REQ_PREV_MENU;
			else
				key = REQ_LEFT_ITEM;
            break;
		case KEY_RIGHT:
			if(layout == fixedColsId && measure == 1)
				key = REQ_NEXT_MENU;
			else
				key = REQ_RIGHT_ITEM;
            break;
        case KEY_HOME:
			key = REQ_FIRST_ITEM;
			break;
        case KEY_END:
			key = REQ_LAST_ITEM;
			break;
        case KEY_PGUP:
			key = REQ_SCR_UPAGE;
            break;
		case KEY_PGDN:
			key = REQ_SCR_DPAGE;
            break;
		case KEY_CANCEL:
			key = REQ_CANCEL;
			break;
		case KEY_RETURN:
		case ' ':
            key = REQ_DONE;
			break;
	}
	return(key);
}


//
//	select menu
//

int Menu::select(void)
{
	// show self, then register for events

	show();
	return(CUI_Emanager->activate(this, CUI_KEYS));
}


//
//	cancel menu
//

int Menu::cancel(void)
{
	// if we're a popup...

	if(poppedUp)
	{
		if(flags & CUI_HIDE_ON_EXIT)
			popDown();
		return(0);
	}

	// else regular menu; if hide-on-exit, hide self

	if(flags & CUI_HIDE_ON_EXIT)
		hide();

	// if our parent is a MenuButton or Mitem, tell it we've cancelled

    if(parent)
	{
		CUI_WidgetId parentId = parent->getId();
		if(parentId == CUI_MENUBUTTON_ID || parentId == CUI_MITEM_ID)
			parent->cancel();
	}

	// if we hid self, refresh current ControlArea

	if(flags & CUI_HIDE_ON_EXIT)
	{
		ControlArea *area = getControlArea();
		if(area)
			area->refresh();
	}

    return(0);
}


//
//	check all our Items, and if any has default resource set,
//	make it current (returns TRUE if we found a default item, else FALSE)
//

bool Menu::setDefaultItem(void)
{
	bool haveDefault = FALSE;
	for(int i = 0; i < numChildren; i++)
	{
		long lflags = children[i]->flagValues();
		if((lflags & CUI_DEFAULT) && (lflags & CUI_SENSITIVE))
		{
			set_current_item(menu, ((Item *)children[i])->ETIitem());
			haveDefault = TRUE;
		}
	}
	return(haveDefault);
}


//
//	show/hide menu
//

int Menu::show(void)
{
	// make sure we're realized and we have a window

	realize();
	if(!window)
		initWindow();

	// show our window and post the menu (if we have any items)

	window->show();
	if(numItems)
	{
		// we may have set current item, but posting zaps it;
		// save and restore

		int index = getCurrentItem();
        int err = post_menu(menu);
		if(err && err != E_POSTED)
			CUI_fatal(dgettext( CUI_MESSAGES, "Fatal error posting Menu (errcode = %d)"), err);

        // if we've saved a default item, make this current

		bool haveDefault;
        if(saveDefault)
		{
			setCurrentItem(saveDefault);
			haveDefault = TRUE;
		}

		// else check flags and set default item if specified

		else
		{
			haveDefault = setDefaultItem();
		}

		// if we didn't set default, and current item is not sensitive,
		// and we have a sensitive item somewhere,
		// move to next till we find a sensitive item

		if(!haveDefault && haveSensitiveItem())
		{
			while(!IS_SELECTABLE(current_item(menu)))
				menu_driver(menu, REQ_NEXT_ITEM);
		}

		// unset any default flag so it doesn't apply next time we post

		for(int i = 0; i < numChildren; i++)
		{
			children[i]->flagValues() &= ~CUI_DEFAULT;
		}

		// finally over-ride default item with value from setCurrentItem

		if(index != -1)
			setCurrentItem(index);
    }
	flags |= CUI_VISIBLE;
	return(refresh());
}

int Menu::hide(void)
{
	// unpost the MENU (ignore any error)

	int err = unpost_menu(menu);
	NO_REF(err);

	// if we have a window, hide it

	if(window)
		window->hide();

	flags &= ~CUI_VISIBLE;
    return(0);
}


//
//	add child
//

int Menu::addChild(Widget *child)
{
	// nothing to do?
	// child will add its ITEM to our MENU when realized...

	return(Composite::addChild(child));
}


//
//	unset currently-set item (for exclusive mode)
//

void Menu::unsetSelected(void)
{
	for(int i = 0; i < numItems; i++)
	{
		if(item_value(items[i]))
			set_item_value(items[i], FALSE);
	}
}


//
//  do we have two items set? (for non-exclusive non-set mode)
//

bool Menu::twoItemsSet(void)
{
    int count = 0;
    for(int i = 0; i < numItems; i++)
    {
        if(item_value(items[i]))
        {
            if(++count == 2)
                return(TRUE);
        }
    }    
    return(FALSE);
}


//
//	lookup by ITEM/index
//

int Menu::lookupETIitem(ITEM *item)
{
	for(int i = 0; i < numItems; i++)
	{
		if(item == items[i])
			return(i);
	}
	return(-1);
}

ITEM *Menu::lookupETIitem(int index)
{
	if(index >= numItems)
		return(NULL);
	else
		return(items[index]);
}


//
//	add/remove item to/from menu
//

int Menu::addItem(ITEM *item, int index)
{
	// we never add beyond (end of array + 1) or before the beginning

	if(index > numItems)
		index = numItems;
	if(index < 0)
		index = 0;

	// remember whether we're visible, and hide our window
	// if we'll change size, delete our window
	// disconnect items

	bool wasVisible = ((flags & CUI_VISIBLE) != 0);
	hide();
    if(window && (flags & CUI_RESIZEABLE))
        freeWindow();
	set_menu_items(menu, NULL);

	// grow items array if necessary

	if(numItems == itemArraySize - 1)  // keep final NULL!
	{
		items = (ITEM **)CUI_arrayGrow((char **)items, 10);
		itemArraySize += 10;
	}

	// shuffle up to make space for the new item

	for(int i = numItems - 1; i >= index; i--)
		items[i + 1] = items[i];

    // add new item and bump count

	items[index] = item;
	numItems++;

	// reconnect items to menu

	connectItems();

	// if we were visible and we're updating the display,
	// make sure we're visible

	bool saveMode = CUI_updateMode();
	if(wasVisible && saveMode)
		show();

	return(0);
}

int Menu::removeItem(ITEM *item)
{
	// error if not in the array

	int index = lookupETIitem(item);
	if(index == -1)
		return(-1);

	// remember whether we're visible,
    // hide self, and if we'll change size, delete our window
	// disconnect items

	bool wasVisible = ((flags & CUI_VISIBLE) != 0);
    hide();
	if(window && (flags & CUI_RESIZEABLE))
		freeWindow();
	set_menu_items(menu, NULL);

    // shuffle down to remove the item

	for( ; index < numItems; index++)
		items[index] = items[index + 1];
	numItems--;

	// reconnect items to menu

	connectItems();

	// if we were visible and we're updating the display...

	bool saveMode = CUI_updateMode();
	if(wasVisible && saveMode)
	{
		// if we have some items, re-show self,
		// else just re-set VISIBLE flag

		if(numItems)
			show();
		else
			flags |= CUI_VISIBLE;
	}
	return(0);
}


//
//	locate menu
//

int Menu::locate(short Row, short Col)
{
	// save new location

	row = Row;
	col = Col;

	// if we've got a window; move it

	if(window)
		window->locate(row, col);

	// refresh and return

	return(refresh());
}


//
//	refresh menu
//

int Menu::refresh(void)
{
	if(window && (flags & CUI_VISIBLE))
	{
		bool updating = CUI_updateMode();
		if(updating)
			window->refresh();

		// fixup arrows for children that have submenus

		// (we have problems drawing arrows for items that have sub-menus;
		// disable screen updating before the refresh, tell children to
		// fixup their arrows, and re-enable updating)

		int first = top_row(menu);
		for(int i = first; i < numChildren && i < first + height; i++)
		{
			if(children[i] == currentWidget())
				((MenuItem *)children[i])->fixupArrow(TRUE);
			else
				((MenuItem *)children[i])->fixupArrow(FALSE);
		}

		// draw scrollbar if required
		// (should optimize so we don't draw if we haven't moved?)

		if(scrolling)
        {
			pos_menu_cursor(menu);
			int first = top_row(menu);
			window->drawScrollBar(first, numItems);
		}

		// refresh everything

		if(updating)
			CUI_refreshDisplay(FALSE);

		// update our cursor

		if(menu)
			pos_menu_cursor(menu);
		if(updating)
#ifdef WAS
			window->refresh();
#else
			wrefresh(window->getInner());
#endif
    }
    return(0);
}


//
//	force refresh of menu (used when we change item's label)
//

extern "C"
{
	//	BEWARE! these are internal ETI routines
	void _show(MENU *m);
	void _draw(MENU *m);
};

void Menu::forceRefresh(void)
{
	if(menu && window && (flags & CUI_VISIBLE))
	{
		_draw(menu);
        _show(menu);
		refresh();
	}
}


//
//	add Item
//

int Menu::addItem(int index, Item *item)
{
	// make sure item is realized, and add it to our menu

	item->realize();
	ITEM *menuItem = item->ETIitem();
	return(addItem(menuItem, index));
}


//
//	delete ListItem
//

int Menu::deleteItem(int index)
{
	ITEM *ETIitem = lookupETIitem(index);
	if(ETIitem)
		return(removeItem(ETIitem));
	return(-1);
}


//
//	lookup ListItem
//

Item *Menu::lookupItem(int index)
{
	ITEM *ETIitem = lookupETIitem(index);
	if(ETIitem)
		return((Item *)item_userptr(ETIitem));
    return(NULL);
}


//
//	make Item current
//

int Menu::setCurrentItem(int index)
{
	// if we're realized, set item current now

	if(flags & CUI_REALIZED)
	{
		ITEM *ETIitem = lookupETIitem(index);
		if(ETIitem && menu)
		{
			if(item_opts(ETIitem) && O_SELECTABLE)
			{
				bool save = CUI_updateDisplay(FALSE);
				set_current_item(menu, ETIitem);
				refresh();
				CUI_updateDisplay(save);
			}
			else
				return(-1);
        }
		else
			return(0);
	}

	// else save the index for when we realize

	else
	{
		saveDefault = (short)index;
	}

	// if parent is an AbbrevMenuButton, we must set its text

	if(parent && (parent->getId() == CUI_ABBREV_ID))
		((AbbrevMenuButton *)parent)->setText();

	return(0);
}


//
//	return index of current item
//

int Menu::getCurrentItem(void)
{
	Item *item = (Item *)currentWidget();
	ITEM *ETIitem = item->ETIitem();
	return(lookupETIitem(ETIitem));
}


//===========================================================================
//					   internal helper routines
//===========================================================================


//
//	initialize/free menu's associated window
//

int Menu::initWindow(void)
{
	// anything to do?

	if(window)
		return(0);

	// disable display while this is going on...

	bool saveMode = CUI_updateDisplay(FALSE);

	// calculate width and height

	getDimensions();

	short Row = row;
	short Col = col;

    // if row or col is -1, we must pop up...

	if(row == -1 || col == -1)
	{
		// if CENTERED, center on-screen, else use current row/col

		if(flags & CUI_CENTERED)
		{
			// subtract 1 from row for message window...

			Row = ((CUI_screenRows() - height) / 2) - 1;
			Col = (CUI_screenCols() - width) / 2;
        }
		else
			CUI_getCursor(&Row, &Col);
	}

	// else make row/col absolute
	// (but only if we're a LIST)

	else
	{
		if(isKindOf(CUI_LIST_ID))
			makeAbsolute(Row, Col);
	}

	// create and realize our window

	int winTitle = CUI_compileString(title);
	CUI_Resource windowResources[15];

	int i = 0;

	// If we're a ScrollingList, we must make our window
	// a child window of our containing ControlArea's window,
	// so that we won't disappear when the area is refreshed;
	// do this by setting the 'parentWindow' resource to ControlArea's name.

	ControlArea *containingArea = NULL;
    if(ID == CUI_LIST_ID)
	{
		if(parent)
		{
			containingArea = parent->getControlArea();
			if(containingArea)
			{
				int lname = CUI_compileString(containingArea->getName());
				CUI_setArg(windowResources[i], parentWindowId, lname); i++;
			}
		}
	}

	// set window's other resources

	CUI_setArg(windowResources[i], rowId,			Row);			i++;
	CUI_setArg(windowResources[i], colId,			Col);			i++;
    CUI_setArg(windowResources[i], heightId,        height);        i++;
	CUI_setArg(windowResources[i], widthId, 		width); 		i++;
	CUI_setArg(windowResources[i], borderWidthId,	2); 			i++;
	CUI_setArg(windowResources[i], titleId, 		winTitle);		i++;
    CUI_setArg(windowResources[i], nullStringId,   0);

	// create window, set colors, and realize

	MEMHINT();
    window = new Window(NULL, NULL, windowResources);
	window->borderAttrib()	 = borderColor;
	window->titleAttrib()	 = titleColor;
	window->interiorAttrib() = normalColor;
	if(ID != CUI_LIST_ID)
		window->flagValues() |= CUI_POPUP;

	// before we realize, move window to within containing area (if any),
	// else it will show up on the screen in the wrong place, if
	// it has a background color different from CUI_screenWindow
	// THIS DOESN'T WORK! - we have problems when our window is a child
	// of the containg-area's window...

//	short saveRow = window->wRow();
//	short saveCol = window->wCol();
//	if(containingArea)
//		window->locate(containingArea->wRow() + 1, containingArea->wCol() + 1);
    window->realize();
//	if(containingArea)
//		window->locate(saveRow, saveCol);

	// associate the curses windows with the menu
	// (unpost first, ignoring error, and re-post if we were posted)

	int err = unpost_menu(menu);
    set_menu_win(menu, window->getOuter());
	set_menu_sub(menu, window->getInner());
	if(err && err == E_POSTED)
	{
		err = post_menu(menu);
		if(err)
			CUI_fatal(dgettext( CUI_MESSAGES, "Fatal error posting Menu (errcode = %d)"), err);
	}

    // don't cycle if we scroll (it's very confusing)


	if(scrolling)
		menu_opts_on(menu, O_NONCYCLIC);


	// set the appropriate attribute in the window (just in case)

#ifdef ATTRIBS
	ui_window_set_attrib(menu_window, UI_TXT_VSTATE, UI_VSTATE_MENU_REST);
#endif /* default is A_NORMAL */

	// restore display-update mode

	CUI_updateDisplay(saveMode);

    return(0);
}

int Menu::freeWindow(void)
{
	if(window)
	{
		window->hide();
		MEMHINT();
        delete(window);
		window = NULL;
    }
	return(0);
}


//
//	(re)connect items to menu
//

int Menu::connectItems(void)
{
	// do nothing if we have no items

	if(!numItems)
		return(0);

    // do the connect

	int err = set_menu_items(menu, items);
	if(err)
		CUI_fatal(dgettext( CUI_MESSAGES, "Fatal error connecting items to Menu (errcode = %d)"),
				  err);

	// connecting unsets the current menu-format - respecify it

	getDimensions();
    return(0);
}


//
//	get menu's dimensions
//

void Menu::getDimensions(void)
{
	// if we have no items, do nothing

	if(!numItems)
		return;

	// save current item (something resets it...)

	ITEM *saveCurrent = current_item(menu);

	// calculate number of rows and cols in menu

	int numRows = 1;
	int numCols = 1;

	calcRowsAndCols(numRows, numCols, layout, measure, numChildren);

	// if we're not resizeable (fixed height specified)
	// reset numRows to specified height (less 2 we already added for border)

	if(!(flags & CUI_RESIZEABLE))
		numRows = height - 2;

	// format the menu and (re)calculate height and width

	set_menu_format(menu, numRows, numCols);
	int newHeight, newWidth;
	scale_menu(menu, &newHeight, &newWidth);

    // if we have a title, make sure we're wide enough to show it

	if(title)
	{
		int len = strlen(title) + 2;
		if(newWidth < len)
			newWidth = len;
	}

	// set widget's height and width (allow for border)
	// (if we're not resizeable, we've already set height)

	if(flags & CUI_RESIZEABLE)
		height = (short)newHeight + 2;
	if(width < (short)newWidth + 2)
		width = (short)newWidth + 2;

	// determine whether we should scroll

	if(numItems > height - 2 && layout == fixedColsId && measure == 1)
		scrolling = TRUE;
	else
		scrolling = FALSE;

	// restore current item

	set_current_item(menu, saveCurrent);
}


//
//	do we have at least one sensitive item?
//

bool Menu::haveSensitiveItem(void)
{
	for(int i = 0; i < numItems; i++)
	{
		if(IS_SELECTABLE(items[i]))
			return(TRUE);
	}
	return(FALSE);
}


//
// set the menu mark-string according to flag values
//

void Menu::setMarkString(void)
{
    if(flags & CUI_TOGGLE)
	{
		char *mark = Menu::toggleMark();
		if(vidMemWrite)
			mark[0] = ACS_CHECKMARK;
		set_menu_mark(menu, mark);
	}
	else
		set_menu_mark(menu, Menu::regularMark());
}


//
//	popup/down free-floating menu
//

int Menu::popUp(CUI_GrabMode)
{
	// we don't use the CUI_GrabMode parameter (default is CUI_GRAB_EXCLUSIVE)

	// save cursor location for popup

	short cursorRow, cursorCol;
	CUI_getCursor(&cursorRow, &cursorCol);

    if(!poppedUp)
	{
        // first refresh current ControlArea, otherwise it may 'show through'

        Widget *currentWidget = CUI_Emanager->currentWidget();
		ControlArea *area = currentWidget->getControlArea();
		area->refresh();

		// if we're not realized, do so
		// (set flags to ensure we show, and will hide on exit;
		// ensure we popup at cursor)

		if(!(flags & CUI_REALIZED))
		{
			flags |= CUI_MAPPED;
			flags |= CUI_HIDE_ON_EXIT;
			row = col = -1;
			CUI_setCursor(cursorRow, cursorCol);
            realize();
        }

		// else move our window to current cursor location and show

		else
		{
			window->locate(cursorRow, cursorCol);
			show();
		}

		// make sure cursor isn't moved

		CUI_cursorOK = TRUE;
		CUI_doRefresh = FALSE;

		// register for events

		CUI_Emanager->activate(this, CUI_KEYS);

		// save state

		poppedUp = TRUE;
	}
    return(0);
}

void Menu::popDown(void)
{
	if(poppedUp)
	{
		// de-register for events and hide

		CUI_Emanager->deactivate(this);
		hide();
		poppedUp = FALSE;

		// send focus event to the newly-current widget
		// (but tell it not to reset its focus)

		Widget *current = CUI_Emanager->currentWidget();
		if(current)
			current->focus(FALSE);
    }
	else
	{
		cancel();
		CUI_cursorOK = TRUE;
	}
}


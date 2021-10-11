#pragma ident "@(#)mbutton.cc   1.6     93/07/23 SMI" 

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
//	MenuButton class implementation
//
//	$RCSfile: mbutton.cc $ $Revision: 1.4 $ $Date: 1992/12/30 21:23:26 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#include <stdio.h>
#ifndef  _CUI_APPLICATION_H
#include "app.h"
#endif
#ifndef  _CUI_LIB_H
#include "cuilib.h"
#endif
#ifndef  _CUI_CURSVMEM_H
#include "cursvmem.h"
#endif
#ifndef  _CUI_FLAGS_H
#include "flags.h"
#endif
#ifndef  _CUI_MBUTTON_H
#include "mbutton.h"
#endif
#ifndef  _CUI_WINDOW_H
#include "window.h"
#endif
#ifndef  _CUI_COLOR_H
#include "color.h"
#endif

#endif // PRE_COMPILED_HEADERS


// flag values for our menus

#define FLAGS (CUI_RESIZEABLE | CUI_HIDE_ON_EXIT | CUI_ALIGN_CAPTIONS)

// the best we can do in ASCII for an arrow

#define ASCII_ARROW 	"=>"

// and in vidMemWrite mode...

#define TEMP_ARROW		'^'     // we use this temporarily
#define VIDMEM_ARROW	(ACS_FAT_DARROW | A_ALTCHARSET)


// resources to load

static CUI_StringId resList[] =
{
    menuId,         // the menu we invoke
	nullStringId
};


MenuButton::MenuButton(char* Name, Widget* Parent, CUI_Resource* Resources,
					   CUI_WidgetId id)
	: OblongButton(Name, Parent, NULL, id)
{
	if(!ID)
		ID = id;
    menuPtr    = NULL;
	menuActive = FALSE;

	loadResources(resList);
	setValues(Resources);
}


int MenuButton::realize(void)
{
	// anything to do?

	if(flags & CUI_REALIZED)
		return(0);

	// invoke Button's realize method

	return(OblongButton::realize());
}


//
//	destructor
//

MenuButton::~MenuButton(void)
{
	MEMHINT();
	delete(menuPtr);
}


//
//	resource routines
//

int MenuButton::setValue(CUI_Resource *resource)
{
	int value = (int)resource->value;
	char *stringValue  = CUI_lookupString(value);
    switch(resource->id)
	{
        case menuId: // name of menu to invoke
		{
			// lookup the menu by name and tell it we're its parent
			// (this will invoke our addChild() method, which verifies type)

			Widget *menu = Widget::lookup(stringValue);
            if(!menu)
				CUI_fatal(dgettext( CUI_MESSAGES, "Can't find menu %s to associate with button %s"),
						  stringValue, name);
			menu->setParent(this);
            break;
        }
        default:
			return(OblongButton::setValue(resource));
	}
	return(0);
}

int MenuButton::getValue(CUI_Resource *resource)
{
	switch(resource->id)
	{
		default:
			return(OblongButton::getValue(resource));
	}
}


//
//	set label
//

int MenuButton::setLabel(char *Label)
{
	if(Label && Label[0] != '(')
	{
		// add arrow to label (we'll pretty it up as we display, if we can)

		char arrowBuffer[4];
		if(vidMemWrite)
			sprintf(arrowBuffer, "%c", TEMP_ARROW);
		else
			sprintf(arrowBuffer, "%s", ASCII_ARROW);

		char buffer[90];
		if(Label[0])
			sprintf(buffer, "%s %s", Label, arrowBuffer);
		else
			sprintf(buffer, "%s", arrowBuffer);
        Button::setLabel(buffer);
	}
	return(0);
}


//
//	add Child
//

int MenuButton::addChild(Widget *widget)
{
	char *message;
	message = dgettext( CUI_MESSAGES, "Child of MenuButton must be a Menu - found a %s");

	// verify that the child is a Menu and save its pointer

	if(widget->getId() != CUI_MENU_ID)
		CUI_fatal(message, Widget::typeName(widget->getId()));
	menuPtr = (Menu *)widget;

	// set menu flags

	menuPtr->flagValues() |= FLAGS;

	return(0);
}


//
//	filter/handle key
//

bool MenuButton::filterKey(CUI_KeyFilter filter, int key)
{
	if(menuActive && menuPtr)
		return(menuPtr->filterKey(filter, key));
	else
		return(filter(this, key));
}

int MenuButton::doKey(int key, Widget *from)
{
	// if menu is active, ask it to process the key

	if(menuActive)
	{
		if(menuPtr->doKey(key, from) != 0)
		{
			// menu didn't handle it - give it back to parent

			return(OblongButton::doKey(key, from));
		}
	}
	else  // menu is not active
	{
		// if RETURN or SPACE, popup our menu

		if(key == KEY_RETURN || key == ' ')
			return(popup());

		// else let parent handle it

		else
			return(OblongButton::doKey(key, from));
	}
	return(0);
}


//
//	locate the cursor
//

void MenuButton::locateCursor(void)
{
	if(menuPtr && menuActive)
	{
		menuPtr->locateCursor();
		CUI_doRefresh = FALSE;
	}
	else
	{
		FORM *form = ETIform();
		form_driver(form, REQ_BEG_LINE);
		form_driver(form, REQ_NEXT_CHAR);
		pos_form_cursor(form);
	}
}


//
//	focus/unfocus
//

int MenuButton::focus(bool)
{
	// if parent is an AbbrevMenuButton, make sure it gets
	// a chance to invoke a focus callback

	if(parent && parent->isKindOf(CUI_MENUBUTTON_ID))
		parent->focus();

	// tell Control to do its work, then fix up arrow if it succeeded

	int ret = Control::focus();
	if(ret == 0)
		fixupArrow(TRUE);
	return(ret);
}

int MenuButton::unfocus(void)
{
	// if parent is an AbbrevMenuButton, make sure it gets
	// a chance to invoke an unfocus callback

	if(parent && parent->isKindOf(CUI_MENUBUTTON_ID))
		parent->unfocus();

    // tell Control to do its work

	Control::unfocus();

	// if menu is active, deactiveate and hide it

	if(menuActive)
	{
		menuActive = FALSE;
		menuPtr->hide();
	}

	// fix up arrow

	return(fixupArrow(FALSE));
}


//
//	refresh self
//

int MenuButton::refresh(void)
{
	if(menuActive)
		menuPtr->refresh();
	return(0);
}


//
//	popup our menu
//

int MenuButton::popup(void)
{
	if(menuPtr)
	{
		// locate and show menu

		short Row = row + 1;
		short Col = col;
		makeAbsolute(Row, Col); // make coordinates absolute
		menuPtr->locate(Row, Col);
		menuPtr->show();
		menuActive = TRUE;
	}
	else
		CUI_infoMessage(dgettext( CUI_MESSAGES, "MenuButton '%s' has no menu"), name);
	return(0);
}


//
//	fix up arrow in vidMemWrite mode
//

int MenuButton::fixupArrow(bool current)
{
	if(vidMemWrite)
	{
		Window *window = parent->getWindow();
		if(window)
		{
			WINDOW *win = window->getInner();

			chtype attrib;
			if(current)
				attrib = Color::lookupValue(activeColor);
			else
				attrib = Color::lookupValue(normalColor);

			chtype newArrow = 0L;
			int i;
			for(i = 0; label[i]; i++)
			{
				if(label[i] == TEMP_ARROW)
				{
					newArrow = VIDMEM_ARROW | attrib;
					break;
				}
			}
			if(newArrow)
			{
				wmove(win, row, col + i);
				waddch(win, newArrow);

				// refresh window only if display is in update mode

				if(CUI_updateMode())
					wrefresh(win);
			}
		}
	}
	return(0);
}

